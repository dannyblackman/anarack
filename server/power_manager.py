"""
Anarack Power Manager — controls mains power to synths via Shelly smart plugs.

Phase 1: manual on/off via Pi Agent HTTP routes.
Phase 2: session-aware auto power with idle timer + JACK boot confirmation.

Supports Shelly Gen2 (Plus/Pro) HTTP RPC API with Gen1 fallback.

Usage:
    Instantiated by Pi Agent. Not run standalone.
"""

import asyncio
import json
import logging
import time

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("power-mgr")

try:
    import aiohttp
except ImportError:
    log.error("aiohttp required: pip install aiohttp")
    raise


class SynthPowerState:
    """Tracks power state for a single synth."""

    def __init__(self, synth_id: str, config: dict):
        self.synth_id = synth_id
        self.address = config["address"]  # e.g. "http://shellypluspluguk-XXXX.local"
        self.relay = config.get("relay", 0)
        self.boot_delay_ms = config.get("boot_delay_ms", 8000)
        self.boot_timeout_ms = config.get("boot_timeout_ms", 20000)
        self.off_duration_ms = config.get("off_duration_ms", 30000)
        self.idle_off_ms = config.get("idle_off_ms", 900000)  # 15 min
        self.ready_check = config.get("ready_check", "delay")  # "delay" or "jack_port"
        self.jack_port_match = config.get("jack_port_match", "")

        self.is_on: bool = False
        self.last_on: float = 0
        self.last_off: float = 0
        self.last_activity: float = 0
        self.booting: bool = False


class PowerManager:
    """Manages power state for all synths on this Pi."""

    def __init__(self):
        self.synths: dict[str, SynthPowerState] = {}
        self._session: aiohttp.ClientSession | None = None

    def register_synth(self, synth_id: str, power_config: dict):
        """Register a synth with its power configuration."""
        self.synths[synth_id] = SynthPowerState(synth_id, power_config)
        log.info(f"Registered power control for {synth_id}: {power_config['address']}")

    async def _get_session(self) -> aiohttp.ClientSession:
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=10)
            )
        return self._session

    async def _shelly_request(self, address: str, relay: int, turn_on: bool) -> bool:
        """Send power command to Shelly. Tries Gen2 RPC first, falls back to Gen1."""
        session = await self._get_session()
        on_str = "true" if turn_on else "false"

        # Gen2 (Shelly Plus / Pro)
        gen2_url = f"{address}/rpc/Switch.Set?id={relay}&on={on_str}"
        try:
            async with session.get(gen2_url) as resp:
                if resp.status == 200:
                    data = await resp.json()
                    log.info(f"Shelly Gen2 {'on' if turn_on else 'off'}: {data}")
                    return True
        except Exception as e:
            log.debug(f"Gen2 request failed ({e}), trying Gen1...")

        # Gen1 fallback
        gen1_url = f"{address}/relay/{relay}?turn={'on' if turn_on else 'off'}"
        try:
            async with session.get(gen1_url) as resp:
                if resp.status == 200:
                    data = await resp.json()
                    log.info(f"Shelly Gen1 {'on' if turn_on else 'off'}: {data}")
                    return True
                else:
                    log.error(f"Shelly returned {resp.status}")
                    return False
        except Exception as e:
            log.error(f"Shelly request failed: {e}")
            return False

    async def _shelly_status(self, address: str, relay: int) -> dict | None:
        """Get current relay status from Shelly."""
        session = await self._get_session()

        # Gen2
        try:
            url = f"{address}/rpc/Switch.GetStatus?id={relay}"
            async with session.get(url) as resp:
                if resp.status == 200:
                    return await resp.json()
        except Exception:
            pass

        # Gen1 fallback
        try:
            url = f"{address}/relay/{relay}"
            async with session.get(url) as resp:
                if resp.status == 200:
                    return await resp.json()
        except Exception:
            pass

        return None

    async def power_on(self, synth_id: str) -> dict:
        """Power on a synth. Returns status dict."""
        synth = self.synths.get(synth_id)
        if not synth:
            return {"ok": False, "error": f"Unknown synth: {synth_id}"}

        if synth.booting:
            return {"ok": False, "error": "Already booting", "state": "booting"}

        if synth.is_on:
            return {"ok": True, "state": "on", "message": "Already on"}

        # Enforce off_duration guard
        if synth.last_off > 0:
            elapsed = (time.time() - synth.last_off) * 1000
            if elapsed < synth.off_duration_ms:
                remaining = int(synth.off_duration_ms - elapsed)
                return {
                    "ok": False,
                    "error": f"Cooldown: wait {remaining}ms before re-powering",
                    "state": "cooldown",
                    "remaining_ms": remaining,
                }

        # Send power on command
        success = await self._shelly_request(synth.address, synth.relay, turn_on=True)
        if not success:
            return {"ok": False, "error": "Shelly request failed", "state": "error"}

        synth.booting = True
        synth.last_on = time.time()

        # Wait for boot
        if synth.ready_check == "jack_port" and synth.jack_port_match:
            ready = await self._wait_for_jack_port(synth)
        else:
            await asyncio.sleep(synth.boot_delay_ms / 1000)
            ready = True

        synth.booting = False
        synth.is_on = ready
        synth.last_activity = time.time()

        if ready:
            log.info(f"{synth_id} powered on and ready ({synth.boot_delay_ms}ms boot)")
            return {"ok": True, "state": "on"}
        else:
            log.warning(f"{synth_id} powered on but boot confirmation timed out")
            return {"ok": True, "state": "on", "warning": "Boot confirmation timed out"}

    async def power_off(self, synth_id: str) -> dict:
        """Power off a synth. Returns status dict."""
        synth = self.synths.get(synth_id)
        if not synth:
            return {"ok": False, "error": f"Unknown synth: {synth_id}"}

        if not synth.is_on and not synth.booting:
            return {"ok": True, "state": "off", "message": "Already off"}

        # 2s guard after last MIDI activity to avoid cutting power mid-write
        if synth.last_activity > 0:
            since_activity = time.time() - synth.last_activity
            if since_activity < 2.0:
                wait = 2.0 - since_activity
                log.info(f"Waiting {wait:.1f}s after last activity before power off")
                await asyncio.sleep(wait)

        success = await self._shelly_request(synth.address, synth.relay, turn_on=False)
        if not success:
            return {"ok": False, "error": "Shelly request failed", "state": "error"}

        synth.is_on = False
        synth.booting = False
        synth.last_off = time.time()
        log.info(f"{synth_id} powered off")
        return {"ok": True, "state": "off"}

    async def status(self, synth_id: str) -> dict:
        """Get power status for a synth."""
        synth = self.synths.get(synth_id)
        if not synth:
            return {"ok": False, "error": f"Unknown synth: {synth_id}"}

        # Get live status from Shelly
        shelly_data = await self._shelly_status(synth.address, synth.relay)

        state = "booting" if synth.booting else ("on" if synth.is_on else "off")

        result = {
            "ok": True,
            "synth_id": synth_id,
            "state": state,
            "last_on": synth.last_on,
            "last_off": synth.last_off,
        }

        if shelly_data:
            result["shelly"] = {
                "output": shelly_data.get("output", shelly_data.get("ison")),
                "power_w": shelly_data.get("apower", shelly_data.get("power", 0)),
                "temperature_c": shelly_data.get("temperature", {}).get("tC"),
            }
            # Sync our state with reality
            shelly_on = shelly_data.get("output", shelly_data.get("ison", False))
            if shelly_on and not synth.is_on and not synth.booting:
                synth.is_on = True
                result["state"] = "on"
            elif not shelly_on and synth.is_on:
                synth.is_on = False
                result["state"] = "off"

        return result

    async def _wait_for_jack_port(self, synth: SynthPowerState) -> bool:
        """Poll for JACK MIDI port appearance (Phase 2 — stub for now)."""
        # TODO: poll `jack_lsp` output for synth.jack_port_match
        # For now, fall back to fixed delay
        log.info(f"JACK port polling not yet implemented, using boot_delay_ms")
        await asyncio.sleep(synth.boot_delay_ms / 1000)
        return True

    def record_activity(self, synth_id: str):
        """Record MIDI/audio activity for idle timer."""
        synth = self.synths.get(synth_id)
        if synth:
            synth.last_activity = time.time()

    async def close(self):
        """Clean up HTTP session."""
        if self._session and not self._session.closed:
            await self._session.close()
