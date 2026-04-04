#!/usr/bin/env python3
"""
WebSocket relay for Anarack browser demos.

Runs on the VPS (66.245.195.65). Accepts WSS connections from browsers
and relays them to the Pi's midi_router.py WebSocket over the WireGuard tunnel.

Browser ─── WSS ───► VPS relay ─── WS ───► Pi (10.0.0.2:8765)

Paths:
  /        → MIDI relay (JSON messages)
  /audio   → Audio relay (binary PCM chunks)

Usage:
  python3 ws_relay.py --pi-host 10.0.0.2 --pi-port 8765 --port 8900
  python3 ws_relay.py --pi-host 10.0.0.2 --pi-port 8765 --port 8900 \
    --cert /etc/letsencrypt/live/relay.anarack.com/fullchain.pem \
    --key /etc/letsencrypt/live/relay.anarack.com/privkey.pem

For GitHub Pages (HTTPS), the relay must serve WSS (TLS). Use --cert/--key
with a Let's Encrypt cert, or put it behind nginx/caddy for TLS termination.
"""

import argparse
import asyncio
import logging
import ssl

import websockets

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(message)s')
log = logging.getLogger('ws-relay')


async def relay_handler(browser_ws, pi_host: str, pi_port: int):
    """Relay a single browser WebSocket connection to the Pi."""
    remote = browser_ws.remote_address

    # Determine path
    ws_path = '/'
    try:
        if hasattr(browser_ws, 'request') and hasattr(browser_ws.request, 'path'):
            ws_path = browser_ws.request.path
    except Exception:
        pass

    is_audio = '/audio' in str(ws_path)
    kind = 'audio' if is_audio else 'midi'
    pi_path = '/audio' if is_audio else '/'
    pi_url = f'ws://{pi_host}:{pi_port}{pi_path}'

    log.info(f'{kind} client connected: {remote}')

    try:
        async with websockets.connect(pi_url, max_size=None) as pi_ws:
            # Relay in both directions concurrently
            async def browser_to_pi():
                try:
                    async for msg in browser_ws:
                        await pi_ws.send(msg)
                except websockets.ConnectionClosed:
                    pass

            async def pi_to_browser():
                try:
                    async for msg in pi_ws:
                        await browser_ws.send(msg)
                except websockets.ConnectionClosed:
                    pass

            # Run both directions, stop when either side closes
            done, pending = await asyncio.wait(
                [asyncio.create_task(browser_to_pi()),
                 asyncio.create_task(pi_to_browser())],
                return_when=asyncio.FIRST_COMPLETED
            )
            for t in pending:
                t.cancel()

    except (ConnectionRefusedError, OSError) as e:
        log.warning(f'{kind} relay failed for {remote}: Pi unreachable at {pi_url} ({e})')
        try:
            await browser_ws.close(1011, 'Pi unreachable')
        except Exception:
            pass

    log.info(f'{kind} client disconnected: {remote}')


async def main():
    parser = argparse.ArgumentParser(description='Anarack WebSocket relay')
    parser.add_argument('--port', type=int, default=8900, help='Relay listen port')
    parser.add_argument('--host', default='0.0.0.0', help='Relay listen host')
    parser.add_argument('--pi-host', default='10.0.0.2', help='Pi WireGuard IP')
    parser.add_argument('--pi-port', type=int, default=8765, help='Pi WebSocket port')
    parser.add_argument('--cert', help='TLS certificate (for WSS)')
    parser.add_argument('--key', help='TLS private key (for WSS)')
    args = parser.parse_args()

    ssl_ctx = None
    if args.cert and args.key:
        ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ssl_ctx.load_cert_chain(args.cert, args.key)
        proto = 'wss'
    else:
        proto = 'ws'

    handler = lambda ws, path=None: relay_handler(ws, args.pi_host, args.pi_port)

    server = await websockets.serve(
        handler,
        args.host,
        args.port,
        ssl=ssl_ctx,
        max_size=None,
        ping_interval=20,
        ping_timeout=20
    )

    log.info(f'Relay listening on {proto}://{args.host}:{args.port}')
    log.info(f'Forwarding to ws://{args.pi_host}:{args.pi_port}')

    try:
        await asyncio.Future()  # run forever
    finally:
        server.close()
        await server.wait_closed()


if __name__ == '__main__':
    asyncio.run(main())
