# Timelines

## Phase 0 — Prototype (Weeks 1-4)

**Goal:** Prove that remote synth control with acceptable latency is possible.

### Week 1-2: Core Audio/MIDI Path
- [x] Set up Raspberry Pi 5 with Lite OS
- [x] Install and configure JACK at 64 samples / 48kHz
- [x] Connect Scarlett 18i16 via USB, verify low-buffer audio works
- [x] Connect Prophet Rev2 via USB MIDI
- [x] Write Python server: receive UDP/WebSocket MIDI → forward to Rev2 via rtmidi
- [x] Test locally: send a note, hear the synth respond
- [x] Audio streaming from Pi to Mac (JACK → WebSocket → Web Audio API)
- [x] Native client with 4ms round-trip latency (UDP MIDI + UDP audio + PyAudio)
- [x] Auto-start on Pi boot via systemd

### Week 2-3: Network Audio + Remote Testing
- [x] Audio streaming working over LAN (both browser and native client)
- [x] Tailscale configured for remote access
- [ ] Test over real internet from a different location (friend's house / cafe)
- [ ] Measure internet latency — confirm <50ms is achievable

### Week 3-4: Shareable Demo
- [ ] Set up Cloudflare Tunnel on Pi (public WSS URL, no Tailscale needed)
- [ ] Host browser client on GitHub Pages
- [ ] Anyone with the link can play the Rev2 from their browser
- [ ] Send to 5-10 producer friends — collect feedback and reactions
- [ ] Test mobile (phone on 5G) — can you play on the bus?

### Milestone: Shareable Demo Link
A URL that anyone can click to play a real Prophet Rev2 from their browser. This is the proof of concept AND the first marketing asset. Record reactions.

**Go/no-go decision:** If latency is >80ms or feels unusable over real internet, investigate further. If friends are excited and it feels playable, proceed to Phase 1.

## Phase 1 — MVP Product (Months 2-3)

**Goal:** A bookable service with 5 synths that real users can pay for.

### Month 2: Infrastructure
- [ ] Purchase Mac Mini, MOTU interface, MIDI interface
- [ ] Wire up 5 synths with permanent audio + MIDI connections
- [ ] Port prototype code from Pi to Mac Mini
- [ ] Verify all 5 synths work with low-latency audio/MIDI path
- [ ] Set up recording pipeline (per-session WAV capture + storage)

### Month 2-3: Web App
- [ ] Landing page with synth catalogue and pricing
- [ ] User auth (email/password, keep it simple)
- [ ] Booking system: calendar view, pick synth + time slot
- [ ] Payment integration (Stripe — pay-as-you-go + subscription tiers)
- [ ] Session page: synth-specific control UI, audio monitor, recording controls
- [ ] Download page: access completed session recordings
- [ ] Basic admin dashboard: see bookings, monitor synth status

### Month 3: Beta Launch
- [ ] Invite 5-10 beta users (from Reddit/Discord outreach)
- [ ] £5/hour beta pricing
- [ ] Collect feedback on latency, UI, overall experience
- [ ] Fix critical issues
- [ ] Collect testimonials and screen recordings

## Phase 2 — Public Launch (Months 4-5)

**Goal:** Open to the public, start generating revenue.

### Month 4: Polish + Launch
- [ ] Fix issues from beta feedback
- [ ] Synth-specific preset save/recall (SysEx)
- [ ] WebRTC fallback for users who can't install JACK/Tailscale
- [ ] First YouTube video: "I built a remote synth studio" + demo
- [ ] Reddit launch post on r/synthesizers and r/WeAreTheMusicMakers
- [ ] Open bookings at full pricing
- [ ] Referral program: give 1 hour, get 1 hour

### Month 5: Iterate
- [ ] Analyse usage patterns: which synths are most popular? Average session length?
- [ ] Add most-requested features from user feedback
- [ ] Second YouTube video: producer makes a full track using Anarack
- [ ] Start TikTok/Reels content
- [ ] Target: 20-30 paying users, £1,000+ MRR

## Phase 3 — Growth (Months 6-12)

**Goal:** Expand the synth collection, grow to £10k MRR.

### Month 6-8: Expand
- [ ] Add 3-5 more synths based on demand (see hardware plan)
- [ ] Weekly YouTube content schedule
- [ ] Discord community launch
- [ ] Partnership outreach (YouTube producers, music courses)
- [ ] Target: 50+ paying users, £3,000+ MRR

### Month 9-12: Scale
- [ ] Add remaining synths to reach 15 total
- [ ] PR outreach (Sound on Sound, MusicRadar, Attack Magazine)
- [ ] Consider premium tier with rare/vintage gear
- [ ] Mobile-friendly session UI (iPad as controller)
- [ ] Multi-synth sessions (use 2-3 synths at once)
- [ ] Target: 100+ paying users, £10,000+ MRR

## Key Decision Points

| When | Decision | Criteria |
|---|---|---|
| End of Week 4 | Go/no-go on Phase 1 | Latency <50ms and feels usable |
| End of Month 3 | Go/no-go on public launch | 3+ beta users would pay full price |
| Month 6 | Expand synth collection? | Utilisation >20% on existing synths |
| Month 9 | Consider Ltd company | Revenue trending toward £30k+/year |
| Month 12 | Next phase planning | At/near £10k MRR target |

## Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Latency too high for usable experience | Fatal — no product | Prototype proves this early, before major investment |
| Low demand / can't find users | Slow growth | Marketing starts during beta; pivot pricing if needed |
| Synth hardware failure | Session disruption | Maintain spares; insurance; route users to alternative synth |
| Internet outage | Service down | UPS + 4G failover for critical connectivity |
| Someone builds this with more money | Competition | First-mover advantage; community; niche is small enough to not attract VC |
