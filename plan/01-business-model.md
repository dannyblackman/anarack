# Business Model

## Value Proposition

Access to real hardware synthesizers at a fraction of the purchase cost. Producers get the authentic analog/digital sound on their tracks without the £1,000-5,000 per-synth investment.

## Revenue Model

### Subscription Tiers

Three tiers. Each one solves a specific pain point that makes the user upgrade to the next.

| Tier | Price | Hours | Queue | Target |
|---|---|---|---|---|
| **Creator** | £15/month | 2 hours | Standard | Trying it out, casual use |
| **Creator Pro** | £39/month | 8 hours | Priority | Serious bedroom producers, regular users |
| **Professional** | £149/month | 30 hours | Instant (never queues) | Working engineers, studios |

### The Upgrade Ladder

Queue priority is the upgrade trigger. Each tier solves a pain the previous one creates:

**Free browser demo → Creator (£15/month)**
- Hear a real Moog in 10 seconds, zero install. Get hooked.
- £15 is low enough to try without thinking. "Less than a Splice subscription for real hardware."
- 2 hours is enough to use it on one track and get locked in via project recall.

**Creator → Creator Pro (£39/month)**
- Two triggers that stack:
  1. **Queue frustration** — after a few sessions, creators hit queues during peak. "Creator Pro gets priority access."
  2. **Flagship envy** — they see the Moog One and Minimoog in the synth browser but can't connect. "Upgrade to Creator Pro to access flagship synths."
- Both triggers hit at the point where the user is already hooked — they're using it on tracks, their projects depend on it.
- £39 is still impulse-affordable. 8 hours + priority queue + flagship access is a huge step up.

**Creator Pro → Professional (£149/month)**
- For users who are billing clients or working on releases. Waiting is never acceptable.
- Professional tier gets **instant access** via the reserved synth pool — never queues.
- Multi-synth sessions included (layer a Moog bass with a Prophet pad in one session).
- 96kHz option for masters and high-end work.
- 30 hours — enough for heavy project work across a full month.
- The pitch: "A studio day costs £400-800. This is £149/month for unlimited sessions from your own setup."

### Queue Mechanics

**Core principle: no user ever gets kicked off a synth.** Being interrupted mid-session is a terrible experience at any price point. Instead, we solve contention with reserved capacity and queue priority.

**Two pools of synths:**

- **Shared pool** (~12 synths) — available to all tiers. Queue priority: Creator Pro > Creator > PAYG.
- **Pro-reserved pool** (~5-8 synths) — duplicates of the most popular synths (Moog, Prophet, Juno, etc.). Only visible to Professional subscribers. This is how Pros get near-instant access without ever displacing anyone.

A Professional subscriber just sees "Moog Sub37 — Available" and clicks connect. The server transparently routes them to whichever physical unit is free (shared or reserved). They never know multiple units exist — it's just always available.

Creator tiers only see the shared pool. They don't know the reserved pool exists — to them, the queue just works normally.

**Queue priority (shared pool only):**
- **Creator Pro** — priority queue. Jump ahead of standard Creators. Typical wait during peak: 0-5 minutes.
- **Creator** — standard queue. Off-peak: instant. Peak: may wait 5-20 minutes.
- **PAYG** — lowest priority. Fills empty slots only.

The plugin shows queue position and estimated wait in real time. It also shows: "Upgrade to Creator Pro — you'd be connected right now."

**Why duplicates of popular synths make sense:**
- The top 3-4 standard/premium synths will get 70%+ of bookings. Duplicate those.
- A second Moog Sub37 (used: ~£800) pays for itself in a few months of Pro subscriptions.
- Less popular / niche synths only need one unit — they rarely have contention.
- The reserved pool is also a selling point: "Professional subscribers get their own dedicated synth rack."

### Synth Access Tiers

Not all synths are available on all subscription tiers. Flagship/rare synths are the killer upgrade reason to Professional.

| Category | Examples | Creator (£15) | Creator Pro (£39) | Professional (£149) |
|---|---|---|---|---|
| **Standard** | Minilogue XD, MiniBrute 2, Juno-106 | Yes | Yes | Yes |
| **Premium** | Prophet Rev2, Sub37, OB-6, Peak | Yes | Yes | Yes |
| **Flagship** | Moog One, Minimoog Model D, CS-80 | No | Yes | Yes |

**Why this works:**
- Flagship access is the killer upgrade from Creator (£15) to Creator Pro (£39): "You want the Moog One? Upgrade to Creator Pro." That's the most compelling tier jump.
- Creator Pro at £39/month gets you the full synth collection — no restrictions on which synths you can play. At that price point, gating the best gear would feel stingy.
- Professional tier (£149) differentiates on **capacity and priority**, not synth access: 30 hours vs 8, instant access via reserved pool, multi-synth sessions, 96kHz. These matter to working engineers; the synth catalogue is the same.
- Flagships available to Creator Pro + Professional (smaller combined pool than all users), so contention stays manageable without duplicating £6k+ synths.
- Each flagship added to the rack is a marketing event that drives Creator → Creator Pro upgrades: "The Moog One has arrived at Anarack."
- Creator tier still gets great synths (Prophet, Sub37, OB-6 are world-class instruments). They're not getting a lesser experience — they're just missing the ultra-rare stuff, which is the nudge to upgrade.

### Flagship Capacity Management

Flagships are expensive to duplicate, but both Creator Pro and Professional users have access. This creates potential contention — especially problematic for Professional users who are paying for a frictionless experience.

**Three levers, applied progressively:**

**1. Flagship queue priority (always on)**
Professional users always jump ahead of Creator Pro users in the flagship queue. Not booting anyone — just always next in line. A Creator Pro user might wait 10 minutes for the Moog One during peak. A Professional user waits 0-2 minutes.

**2. Flagship session time limits for Creator Pro (during peak)**
Creator Pro gets flagship access but capped at 60 minutes per session during peak hours. This keeps the synth rotating. Professional users get unlimited session length on flagships. The plugin shows a gentle countdown: "Flagship session: 15 minutes remaining. Save your work or switch to a standard synth."

**3. Duplicate when the data says so (reactive)**
Monitor flagship utilisation. When a flagship consistently hits >50% utilisation during peak hours, that's the signal to buy a second unit for the reserved pool. The revenue from Pro subscriptions funds it — a second Moog One at £6k is paid for by ~40 months of one Pro subscriber, or ~3 months of 15 Pro subscribers. Let the data tell you when to spend, not guesswork.

**Why this doesn't need solving upfront:**
At launch you'll have maybe 5 Professional and 10 Creator Pro users sharing one flagship. That's essentially zero contention. By the time it becomes a problem (50+ Creator Pro users), the subscription revenue is more than enough to fund a duplicate. The capacity problem and the funding for its solution grow at the same rate.

**Duplication priority order** (based on likely demand):
1. Moog One — the most desirable synth in production. Duplicate first.
2. Minimoog Model D — iconic, increasingly rare. Second duplicate.
3. Others as data shows demand.

### Synth Acquisition Strategy

Start with attainable synths, add flagships as revenue justifies them:

1. **Launch** — synths you already own (Rev2, Sub37) + affordable additions (Minilogue, MiniBrute, Juno). Total investment: ~£1,000-2,000.
2. **Growth** — add premium synths as Pro subscribers grow (OB-6, Prophet 6, Peak). £800-2,000 each.
3. **Flagship** — once Professional tier has 20+ subscribers, invest in the first flagship (Moog One or Minimoog). Marketing event: "The Moog One is now on Anarack." £5,000-8,000 — paid for by ~3-4 months of Pro subscriptions.
4. **Ongoing** — each new flagship is funded by revenue and marketed as an event. The synth collection becomes a growing asset and a reason to stay subscribed ("what's coming next?").

### Pay-As-You-Go

£0.30/minute (£18/hour) for non-subscribers. No project recall — you get your bounced audio and that's it. Lowest queue priority. Exists to let people try without committing, but everything about it pushes toward subscribing.

### Bolt-Ons

- **Extra hours** — £12/hour (Creator/Creator Pro), £10/hour (Professional). Rolls over 1 month.
- **Multi-synth session** — +£5/hour for Creator tiers (free on Professional)

### Peak Time Management

Peak times (evenings, weekends) are when queue pressure is highest — and when upgrade nudges are most effective. Strategies:

- **Off-peak bonus hours** — Creator gets 2 hours, but off-peak hours (weekday daytime) don't count toward the limit. Encourages spreading load and gives creators more value without increasing peak contention.
- **Session time limits during peak** — 60-minute max session during peak for Creators (no limit for Professional). Keeps synths rotating.
- **"Available now" notifications** — push notification (in plugin) when a favourite synth becomes free during off-peak. Drives usage to low-demand times.

## The Retention Hook: Project Recall

This is the core retention mechanism — modelled on how Universal Audio's subscription works. Your projects depend on having an active subscription.

### How It Works

1. Producer uses Anarack's Moog Sub37 on a track. The plugin saves:
   - Synth ID + patch state (full SysEx dump)
   - All MIDI data and CC automation
   - Session metadata (sample rate, timestamp, etc.)

2. Producer keeps working. The Anarack track sits in their DAW project as MIDI + plugin state — not bounced audio. They're encouraged to keep it as MIDI because:
   - They can tweak the sound later without rebooking
   - "Render in place" bounces a pristine WAV through the real synth on demand
   - They might want to try the same part on a different synth later

3. Six months later, they reopen the project to do a remix. The Anarack plugin loads and shows:
   > "This track used **Moog Sub37** — patch 'Thick Bass v2'"
   > [Connect & Recall] [Render Offline]

4. One click: the synth loads their exact patch, and they can tweak or re-render.

### Without a Subscription

The plugin still loads (it's installed locally). It shows all the project data — which synth, which patch, the MIDI. But it **can't connect**. The track is silent. Just like UA plugins without a subscription.

The producer sees: "Subscribe to reconnect and hear this track."

This creates the UA-style lock-in: cancelling means your projects lose their Anarack tracks. Most producers won't risk that for £10-20/month.

### Making It Stickier

- **Preset cloud** — all your custom patches are stored in your Anarack account. Cancel and you lose access to patches you've spent hours crafting.
- **Render in place** — the plugin can queue a high-quality offline render (the server records a pristine WAV and sends it back). Producers are incentivised to keep MIDI on the track rather than committing to audio, because they can always re-render with tweaks.
- **Cross-synth experimentation** — "Try this patch on the Prophet Rev2 instead?" One click to hear your MIDI through a different synth. Only works with a subscription.
- **Version history** — the plugin keeps a history of patch tweaks per project. Roll back to "how the bass sounded last Tuesday." Stored in the cloud.

## Target Customers

### Launch target: Professional engineers and producers

The music industry is squeezing engineers hard. Artist budgets are shrinking, big studios are pricing out independent engineers, but clients still expect records that sound expensive. An engineer working from a home setup or small studio can't justify £30k+ in synths — but they can justify £149/month for access to a full rack when a project needs it.

**Why start here:**
- They understand the value immediately — no education needed
- £149/month is a rounding error on a project budget
- They'll use it on real releases — the best marketing possible ("the synths on this track were recorded via Anarack")
- Professional word of mouth carries weight
- Smaller volume but much higher ARPU — you need fewer customers to hit £10k MRR
- They'll push you on quality and reliability, which makes the product better for everyone

### Growth target: Bedroom producers and beatmakers

Once the product is proven with pros, expand to the Creator tier. These users are:
- Making beats/tracks at home, want hardware character but can't afford the gear
- Already spending on Splice, Plugin Alliance, etc.
- Know the difference between a Moog and a Moog emulation
- Price-sensitive but willing to pay £19/month for something that genuinely improves their sound

### Bonus: Content creators

YouTubers, TikTok musicians who want to demo "real synth" sounds. Short sessions, high social media reach. Every video they make is free marketing.

## Competitive Landscape

| Alternative | Price | Limitation |
|---|---|---|
| Buy the synth | £1,000-5,000 | Capital outlay, space, maintenance |
| Studio hire | £50-100/hour | Must be physically present |
| Soft synth plugins | £10-30/month | Not the real thing |
| Sample packs | £10-50 one-off | Static, can't tweak |
| **Anarack** | **£19-149/month** | **Latency (~40ms), not for live performance** |

## Key Metrics

- **Subscriber count** — primary metric. Target: 200+ subscribers for £10k MRR (blended ~£20/month average)
- **Utilisation rate** — target 20-30% across the rack. Low is fine — we want subscribers who pay but don't use all their hours
- **Monthly churn** — target <5%. Project recall should make this very low
- **Activation rate** — % of free demo users who subscribe within 7 days
- **Project recall usage** — how many subscribers have projects that depend on Anarack? Higher = stickier

## Defensibility

- Hardware fleet + maintenance expertise is a real barrier to entry
- **Project lock-in** — the longer someone uses Anarack, the more projects depend on it
- **Preset library** — hours of sound design stored in the cloud, painful to lose
- First-mover advantage in a niche that hasn't been served this way before
- Technical know-how (low-latency audio streaming) isn't trivial to replicate
- Community and brand loyalty in the synth world is strong

## Revenue Scenarios

Pro-first launch, then widen the funnel. The upgrade ladder means creators naturally move up tiers as they get hooked.

### Phase 1 (launch): Pro-focused
- 25 Professional × £149 = £3,725/month
- Plus PAYG from early testers: £300/month
- **Total: ~£4,000/month with just 25 customers**

### Phase 2 (growth): Open Creator tiers
- 35 Professional (£149) + 50 Creator Pro (£39) + 100 Creator (£15)
- = £5,215 + £1,950 + £1,500 = £8,665/month
- Plus PAYG + bolt-ons: £1,400/month
- **Total: ~£10,000/month**

### Mature state
- 50 Professional (£149) + 100 Creator Pro (£39) + 300 Creator (£15)
- = £7,450 + £3,900 + £4,500 = £15,850/month
- Plus PAYG: £1,500/month
- **Total: ~£17,350/month**

### Revenue mix at £10k MRR

| Tier | Count | Revenue | % of MRR |
|---|---|---|---|
| Professional | 35 | £5,215 | 52% |
| Creator Pro | 50 | £1,950 | 20% |
| Creator | 100 | £1,500 | 15% |
| PAYG + bolt-ons | — | £1,400 | 13% |
| **Total** | **~185 users** | **~£10,065** | **100%** |

Half the revenue comes from 35 professionals. The long tail of creators adds volume and feeds the upgrade ladder.

### Why this math works
- 35 pro subscribers at £149 = same revenue as 350 creators at £15
- Pros are easier to reach (industry networks, studios, forums) than mass-market bedroom producers
- Each pro subscriber validates the product for the creator audience that follows
- The upgrade ladder is self-reinforcing: queue pressure during growth naturally pushes creators to higher tiers
- With <5% churn (project recall lock-in) you're mostly adding, not replacing
