# Business Model

## Value Proposition

Access to real hardware synthesizers at a fraction of the purchase cost. Producers get the authentic analog/digital sound on their tracks without the £1,000-5,000 per-synth investment.

## Revenue Model

### Subscription Tiers

Three tiers. Each one solves a specific pain point that makes the user upgrade to the next. All prices are VAT-inclusive (handled by Paddle).

| Tier | Monthly | Annual (2 months free) | Hours | Queue | Target |
|---|---|---|---|---|---|
| **Creator** | £15/month | £150/year | 4 hours | Standard | Trying it out, casual use |
| **Creator Pro** | £39/month | £390/year | 8 hours | Priority | Serious bedroom producers, regular users |
| **Professional** | £149/month | £1,490/year | 30 hours | Instant (never queues) | Working engineers, studios |

**Hours rollover:** Unused hours roll over month to month, capped at 6 months' allowance (Creator: max 24 hours banked, Creator Pro: max 48, Professional: max 180). This is a deliberate retention lever — like Splice credits, banked hours create a sunk cost that discourages cancellation. "I've got 12 hours saved up, I can't cancel now."

### The Upgrade Ladder

Queue priority is the upgrade trigger. Each tier solves a pain the previous one creates:

**Free browser demo → Creator (£15/month)**
- Hear a real Moog in 10 seconds, zero install. Get hooked.
- £15 is low enough to try without thinking. "Less than a Splice subscription for real hardware."
- 4 hours is enough to use it on a couple of tracks and get locked in via project recall.

**Creator → Creator Pro (£39/month)**
- Two triggers that stack:
  1. **Queue frustration** — after a few sessions, creators hit queues during peak. "Creator Pro gets top priority in the queue." They'll jump ahead of standard Creators, so waits are shorter — but we're not kicking anyone off a session, so there may still be short waits at the busiest times.
  2. **Flagship limits** — they get 30 minutes/month on flagships, enough to fall in love with the Moog One but not enough to use it seriously. "Upgrade to Creator Pro for unlimited flagship sessions."
- Both triggers hit at the point where the user is already hooked — they've heard what the Moog One sounds like on their track, and 30 minutes isn't enough.
- £39 is still impulse-affordable. 8 hours + top queue priority + unlimited flagship access is a huge step up.

**Creator Pro → Professional (£149/month)**
- For users who are billing clients or working on releases. Waiting is never acceptable.
- Professional tier gets **instant access** via the reserved synth pool — never queues.
- Multi-synth sessions included (layer a Moog bass with a Prophet pad in one session).
- 96kHz option for masters and high-end work.
- 30 hours — enough for heavy project work across a full month.
- The pitch: "A studio day costs £400-800. This is £149/month for unlimited sessions from your own setup."

### Queue Mechanics

**Core principle: no user ever gets kicked off a synth** — unless their credits run out. Being interrupted mid-session is a terrible experience at any price point. Instead, we solve contention with reserved capacity and queue priority. If a user's monthly hours or PAYG balance hits zero, they get a warning and the option to top up before the session ends gracefully.

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

All synths are available on all subscription tiers — but flagships have usage limits on the Creator tier that drive upgrades.

| Category | Synths | Creator (£15) | Creator Pro (£39) | Professional (£149) |
|---|---|---|---|---|
| **Filler** | Minilogue, MiniBrute, Minitaur | Yes | Yes | Yes |
| **Headliner** | Rev2, Subsequent 37, OB-6, Prophet 5, Prophet 10, Juno-106, Summit, Hydrasynth | Yes | Yes | Yes |
| **Flagship** | Moog One, Minimoog Model D | 30 mins/month | Yes | Yes |

**Why this works:**
- Creators get to *try* flagships — 30 minutes is enough to hear a Moog One on their track and fall in love. The upgrade trigger is "I want more of this" not "I can't have this." That's a much stronger emotional driver.
- Locking out flagship access entirely feels punitive and breeds resentment. Limited access creates aspiration.
- Creator Pro at £39/month removes the flagship cap — full access, no restrictions. At that price point, gating the best gear would feel stingy.
- Professional tier (£149) differentiates on **capacity and priority**, not synth access: 30 hours vs 8, instant access via reserved pool, multi-synth sessions, 96kHz. These matter to working engineers; the synth catalogue is the same.
- Flagships see less contention from Creators (30 mins/month means minimal load), so capacity management stays simple without duplicating £6k+ synths.
- Each flagship added to the rack is a marketing event that drives Creator → Creator Pro upgrades: "The Moog One has arrived at Anarack. All tiers can try it."
- Creator tier still gets great synths (Prophet, Sub37, OB-6 are world-class instruments) with no limits — they're getting a full experience, just with a taster of the ultra-rare stuff.

### Flagship Capacity Management

All three tiers can access flagships, but Creators are capped at 30 mins/month — so the real contention is between Creator Pro and Professional users. This is manageable.

**Three levers, applied progressively:**

**1. Flagship queue priority (always on)**
Professional users always jump ahead of Creator Pro users in the flagship queue. Not booting anyone — just always next in line. A Creator Pro user might wait 10 minutes for the Moog One during peak. A Professional user waits 0-2 minutes. Creators queue behind both, but with only 30 mins/month they rarely add pressure.

**2. Flagship session length visibility**
No hard time limits — but users set an estimated session duration when they connect. The queue uses this to show realistic wait times. This keeps the experience respectful (no one gets cut off) while giving waiting users useful information.

**3. Duplicate when the data says so (reactive)**
Monitor flagship utilisation. When a flagship consistently hits >50% utilisation during peak hours, that's the signal to buy a second unit for the reserved pool. The revenue from Pro subscriptions funds it — a second Moog One at £6k is paid for by ~40 months of one Pro subscriber, or ~3 months of 15 Pro subscribers. Let the data tell you when to spend, not guesswork.

**Why this doesn't need solving upfront:**
At launch you'll have maybe 5 Professional and 10 Creator Pro users sharing one flagship. That's essentially zero contention. By the time it becomes a problem (50+ Creator Pro users), the subscription revenue is more than enough to fund a duplicate. The capacity problem and the funding for its solution grow at the same rate.

**Duplication priority order** (based on likely demand):
1. Moog One — the most desirable synth in production. Duplicate first.
2. Minimoog Model D — iconic, increasingly rare. Second duplicate.
3. Others as data shows demand.

### Synth Catalogue

The catalogue leads with 10 aspirational headliner synths — gear people can't justify buying themselves. That's the value proposition. Cheaper synths (Minitaur, Minilogue, MiniBrute etc.) get added later to pad the count and variety, but they're not the reason anyone subscribes.

**All synths must be fully MIDI-controllable** — every parameter accessible via MIDI CC/NRPN, full preset recall via SysEx. This rules out semi-modular/patch cable synths (Matriarch, ARP 2600, etc.) since patch cables can't be set remotely and defeats project recall.

#### The 10 Headliners

| # | Synth | Type | Status | Est. used price |
|---|---|---|---|---|
| 1 | Moog Subsequent 37 | Analogue mono | Owned | — |
| 2 | Moog Minimoog Model D (2022 reissue) | Analogue mono | Buy new | ~£5,000 |
| 3 | Moog One | Analogue poly | Buy | £6,000-8,000 |
| 4 | Sequential Rev2 | Analogue poly | Owned | — |
| 5 | Sequential Prophet 10 | Analogue poly | Buy | £3,000-4,000 |
| 6 | Sequential Prophet 5 | Analogue poly | Buy | £3,000-4,000 |
| 7 | Oberheim OB-6 | Analogue poly | Buy | £2,000-2,500 |
| 8 | Roland Juno-106 | Analogue poly | Buy | £1,500-2,000 |
| 9 | Novation Summit | Hybrid poly | Buy | £1,200-1,500 |
| 10 | ASM Hydrasynth | Wavetable/digital | Buy | £800-1,000 |

**Synths to buy: 7 | Estimated total: £21,000-28,000** (before tax relief and VAT — see financials)

#### Filler Synths (added post-launch for count and variety)

Affordable synths that round out the catalogue. Not the selling point, but they add choice and reduce queue pressure on the headliners:
- Moog Minitaur (~£250 used)
- Korg Minilogue (owned)
- Arturia MiniBrute 2 (~£250 used)
- Behringer Model D, Poly D (~£200-400 used)
- Others as budget allows

#### Synth Acquisition Strategy

1. **Soft launch** — prove the concept with 5 synths already available: Subsequent 37 (owned), Rev2 (owned), Minilogue (owned), Prophet 5 (borrowed), Hydrasynth (borrowed). Zero synth investment.
2. **Proper launch** — buy the remaining 7 headliners. Total investment ~£21,000-28,000 (see financials for effective cost after tax relief/VAT). This is the full catalogue that justifies marketing spend.
3. **Post-launch** — add filler synths to grow the count. £1,000-2,000 total. Each new headliner added later is a marketing event: "The [synth] has arrived at Anarack."

### Partner Synth Network (Phase 2 — post soft-launch, pre marketing push)

Once the concept and market are proven with owned synths, the fastest way to scale the catalogue is partnering with people who already own large collections that sit underutilised: studios with downtime, synth collectors, private owners with racks gathering dust.

**How it works:**
- Revenue split based on time used on their synths. Partner earns a percentage of every session minute on their gear.
- Anarack supplies and installs a dedicated rig at the partner's location (Mac/USB audio interface/networking hardware). This is our capital investment, so it requires a **long-term agreement** (minimum 12-24 months) to justify the hardware spend and setup time.
- Installation takes 1-2 days on-site: set up the rig, wire every synth, test MIDI + audio + latency, configure monitoring.
- Partner synths appear in the Anarack browser alongside owned synths — users don't know or care where the hardware lives.

**Why this works:**
- Scales the synth catalogue dramatically without £50k+ in gear purchases. A single partner with 10 synths doubles the rack overnight.
- Partners earn passive income from gear that's otherwise sitting idle.
- Geographic distribution reduces latency — a partner in the US means American users get sub-30ms rather than transatlantic latency.
- US partners also help manage peak demand: UK evenings (our busiest period) are US afternoon, and US evening demand can be served locally instead of competing for UK capacity.
- Each partner location adds redundancy — if one rig goes down, sessions can potentially be routed to the same synth model at another location.

**Partner requirements:**
- Stable, always-on internet connection (minimum 50Mbps up)
- Synths must be maintained and in good working order
- Dedicated space — the rig and synths can't be moved or unplugged between sessions
- Willingness to troubleshoot basic hardware issues (power cycle, cable check) with remote guidance

**Target partners:**
- UK recording studios with large synth collections and downtime between bookings
- Synth collectors (especially US-based) with significant racks they rarely use
- A few well-placed partners — 2-3 in the UK, 1-2 in the US — would massively expand the catalogue and coverage

**When to start:** After soft launch has validated demand but before the main marketing push. The partner network should be in place so that when marketing drives traffic, the catalogue is deep enough to convert and retain. Build the rack, then turn on the tap.

### Manufacturer & Retailer Partnerships

Anarack is effectively a try-before-you-buy showroom at scale. Every session is extended hands-on time with a real synth — far more effective than 5 minutes at a music shop. That's valuable to manufacturers and retailers, and it creates partnership opportunities that reduce our costs and add a new revenue stream.

#### Manufacturer Partnerships (Moog, Sequential, Novation, ASM, etc.)

**What we offer them:**
- Extended demo time with their products at scale — hundreds of producers playing their synths every month
- Measurable data: "X users played your Prophet 10 this month, average session Y minutes"
- First-to-play launches: "The new Sequential [whatever] — play it first on Anarack" is a marketing event for both parties
- Credibility by association: their synths presented in a professional, well-maintained environment

**What we want from them:**
- **Discounted or sponsored synths** — demo/endorsement pricing, or loaner units for the rack. Manufacturers already supply demo units to shops; this is a more effective version of that.
- **Co-marketing** — featured on their website/socials as an official demo partner. "Try the Moog One on Anarack" on moog.com is massive.
- **Early access to new products** — exclusive launch window on Anarack before retail availability. Drives subscriptions and press coverage.
- **Endorsement** — "Official Moog demo partner" is a trust signal that's worth more than any ad spend.

**Pitch to manufacturers:** "Your synths are sitting in music shops where people play them for 5 minutes through headphones. On Anarack, producers spend an hour with your synth in their actual studio workflow, hear it through their monitors, use it on a real track — and then we show them a link to buy their own. Which is more likely to convert?"

#### Retailer Partnerships (Thomann, Sweetwater, Andertons, Jigsaw24, etc.)

**Affiliate model:**
- After every session, the UI shows: "Loved the Prophet 10? Buy your own" with an affiliate link to the retailer
- Standard affiliate commission on music gear is 3-8% — passive revenue on every conversion
- Could negotiate higher commission or exclusive partnership in exchange for being the sole "buy your own" retailer

**Bulk purchase discounts:**
- Negotiate discounted pricing on Anarack's own synth purchases in exchange for being the exclusive retail partner
- Retailer gets ongoing affiliate revenue + brand association; Anarack gets cheaper gear
- "Anarack synths supplied by Thomann" or similar co-branding

**Why retailers would say yes:**
- New customer acquisition channel they don't have — people who play a synth for hours and then want to own one are high-intent buyers
- Anarack users are qualified leads — they already know exactly which synth they want and why
- No risk to the retailer — affiliate model means they only pay on conversion

#### Affiliate Revenue Potential

Conservative estimate — if 1-2% of active subscribers buy a synth per year through affiliate links:
- 200 subscribers × 1.5% conversion × £2,000 average synth price × 5% commission = ~£300/year
- Not life-changing, but it's pure passive income and grows with the user base
- More importantly, it funds the partnership conversation — retailers engage because there's a commercial model, not just goodwill

#### Timing

- **Soft launch:** no leverage yet, too early to approach manufacturers
- **Post soft launch:** approach manufacturers with real usage data and demo videos. Start with smaller/hungrier brands (ASM, Novation) who are more likely to engage early
- **Post crowdfund/proper launch:** approach the big names (Moog, Sequential) with subscriber numbers and session data. At this point the pitch is backed by evidence, not just an idea

### Pay-As-You-Go

£0.30/minute (£18/hour) for non-subscribers. No project recall — you get your bounced audio and that's it. Lowest queue priority. Exists to let people try without committing, but everything about it pushes toward subscribing.

### Bolt-Ons

- **Extra hours** — £12/hour (Creator/Creator Pro), £10/hour (Professional). Added to rollover balance (still subject to 6-month cap).
- **Multi-synth session** — +£5/hour for Creator tiers (free on Professional)

### Peak Time Management

Peak times (evenings, weekends) are when queue pressure is highest — and when upgrade nudges are most effective. Strategies:

- **Off-peak incentives** — off-peak hours (weekday daytime) could count at a reduced rate (e.g. 2:1 — 30 mins of off-peak use only deducts 15 mins from balance). Encourages spreading load without complicating the tier structure.
- **Session length visibility** — no hard time limits on any tier (no one gets kicked off). Instead, when a user starts a session they set an estimated duration. The queue shows waiting users how long until the synth is likely free. This gives queue intelligence without restricting how people use their time.
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

## White-Label: Remote Demo Platform for Manufacturers (Secondary Business)

The core Anarack tech — low-latency MIDI control + audio streaming + browser UI — can be white-labelled to synth manufacturers as an embeddable "try before you buy" demo platform on their own websites.

### The Product

An embeddable widget or hosted page that a manufacturer puts on their product page. A customer visits moog.com/one, clicks "Play it now", and gets a live session with a real Moog One — controlled from their browser, audio streamed back in real time. Same tech as Anarack, just branded for the manufacturer.

### Why Manufacturers Would Pay For This

- **Online synth sales are growing** but the biggest barrier is "I haven't played it." Soft synths have free trials; hardware doesn't. This solves that.
- **More effective than any demo video** — 10 minutes of hands-on time with a real synth converts better than a hundred YouTube reviews.
- **They already spend on demo programmes** — shipping demo units to shops, maintaining showroom stock, trade show booths. This is cheaper and reaches more people.
- **Data** — they get analytics on which presets people play, how long sessions last, which parameters get tweaked. Product insight they can't get any other way.

### How It Works

- Manufacturer provides the synth (or synths) — either at their own premises or hosted at an Anarack location
- Anarack provides the tech: rig, software, streaming infrastructure, embeddable UI
- Sessions are short (5-10 mins) and free for the end user — this is marketing spend for the manufacturer, not a consumer product
- "Buy now" button at the end of every session links to the manufacturer's store or retail partners

### Revenue Model

- **SaaS fee** — monthly platform fee per synth connected (e.g. £200-500/month per synth)
- **Per-session fee** — alternative model: charge per demo session (e.g. £1-2 per session, manufacturer pays)
- **Setup fee** — one-off fee for rig installation if hosted at manufacturer's premises

### Why This Is a Secondary Business

- The core Anarack platform needs to be built and proven first — the white-label version is the same tech, repackaged
- Manufacturers won't engage until they can see it working (same as the partnership conversation — need traction first)
- Don't distract from the primary subscription business until it's stable
- But once the tech is proven, this is high-margin B2B revenue with long contracts and low churn

### Potential Scale

If 3-5 manufacturers each connect 2-3 synths at £300/month per synth:
- 10 synths × £300 = £3,000/month B2B revenue
- Minimal additional infrastructure cost — it's the same platform
- Strengthens manufacturer relationships, which feeds back into the primary business (discounts, endorsements, early access)

## Analogue Effects Rack (Future Phase — Post Proper Launch)

A premium add-on to the synth subscription: access to real analogue and mechanical effects that plugins genuinely can't replicate. Not digital effects pedals (plugins are too close) — only the units where the physical medium *is* the sound.

### Why Only Analogue/Mechanical

The whole pitch mirrors the synth value proposition: "plugins get close, but not close enough." That's only true for effects where the physical process creates the character:

- **A real plate reverb** — metal physically vibrating creates three-dimensionality and harmonic complexity that convolution IRs and algorithms don't capture
- **A real spring reverb** — splashy, unpredictable behaviour is the whole point
- **A real tape echo** — tape degradation, wow/flutter, saturation can't be faked
- **Real tape saturation** — running audio through actual tape adds harmonics plugins approximate but don't match

Digital effects (Strymon, Eventide, Boss) — skip these. The plugins are basically indistinguishable. Nobody's subscribing for a remote Strymon Timeline.

### The Effects Rack

| Unit | Type | Est. price | Remote control | Notes |
|---|---|---|---|---|
| Roland Space Echo RE-201 | Tape echo | £2,000-3,000 | Servo-controlled knobs | The hero unit. Iconic sound, impossible to replicate digitally. |
| EMT 140 (or Plate Reverb Co equivalent) | Plate reverb | £3,000-10,000 | Simple — damping + level (1-2 servos) | 200kg, nobody's putting one at home. Perfect for "access over ownership." |
| AKG BX20 or Fender spring tank | Spring reverb | £500-2,000 | Minimal — level + EQ | Few parameters, easy to control remotely. |
| Studer A800 or similar | Tape saturation | £5,000-15,000 | Speed + input level (MIDI or servo) | Running audio through real tape. The ultimate "you can hear the difference." |

### Servo-Controlled Knobs (for units without MIDI)

Most of these vintage units have zero MIDI control — the knobs are purely mechanical. Solution: small servo motors physically turning the knobs, driven by MIDI CC.

**How it works:**
- Small servo/stepper motor attached to each knob via 3D-printed non-destructive coupler (clamp-on, no drilling into vintage gear)
- Arduino/Pico microcontroller receives MIDI CC → converts to servo position
- User turns virtual knob in the UI → servo turns physical knob → effect changes
- Rotary encoder feedback reads actual knob position back to the UI

**Engineering reality:**
- Individual components are all solved problems — hobby servos, 3D printing, Arduino, basic code
- A working prototype for one unit is a weekend or two for a competent maker
- Production-reliable units running 24/7 need more iteration — quiet servos (digital, £20-30 each), reliable couplers, wear management
- The Space Echo has ~7 knobs = ~7 servos, one Arduino, ~£100-150 in components per unit
- Main challenges: servo noise during recording (use quiet motors or only move between takes), physical movement latency (100-500ms for servo to reach position), and long-term reliability

**Plate and spring reverbs are easier** — they have 1-3 parameters each. A plate reverb with one damping servo and a level control is a much simpler build than a full Space Echo retrofit.

### Pricing: Effects as an Add-On

Effects work as a modular upsell on top of the synth subscription:

| Plan | Synths only | Synths + Effects | Effects only |
|---|---|---|---|
| Creator | £15/month | £25/month (+£10) | £10/month |
| Creator Pro | £39/month | £49/month (+£10) | £10/month |
| Professional | £149/month | £159/month (+£10) | £10/month |

Or flip it — effects at £15, synths as the add-on. Either way, the bundle is where most people land.

**Why a separate add-on rather than bundled:**
- Not everyone wants effects — some producers only want synths
- Keeps the base synth price low for conversion
- Effects users who don't need synths can subscribe too (mixing engineers who just want a real plate reverb on their mixes)
- Creates another upgrade trigger: "You're already using our synths — add a real Space Echo to your chain for £10/month"

### Audio Routing for Effects

Effects require audio to flow in *both* directions — different from synths (which only send audio back):

1. User's DAW sends audio out to Anarack via the plugin (dry signal)
2. Server routes audio through the physical effect unit
3. Wet/processed audio returns to the user's DAW

The plugin would appear as both an instrument (for synths) and an effect (insert or send) in the DAW. JUCE supports both plugin types from the same codebase.

Latency matters more here — effects are processing existing audio, so the round-trip delay is additive. At <50ms it's usable for mixing (not real-time monitoring). Users would likely print/bounce the wet signal rather than monitor through it live.

### Timing

This is firmly post-proper-launch:
1. Prove the synth business first — effects add complexity
2. Build the servo-controlled prototype once there's revenue to fund it
3. Start with the simplest unit (plate or spring reverb — fewest parameters)
4. Add the Space Echo once the servo engineering is proven
5. Tape saturation last — most expensive, most complex

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

## Risks & Threats

### Technical Risks

**Latency might not be good enough**
- If we can't consistently hit <40ms round-trip, the experience feels sluggish and producers won't tolerate it
- Users on poor internet connections will have a bad time and blame Anarack, not their broadband
- This is the existential risk — if the core experience isn't good enough, nothing else matters
- *Mitigation:* The soft launch exists to prove this. If it doesn't work on Tailscale with a Pi, we know before spending serious money. Production stack (WireGuard, Mac Mini, wired ethernet) will be significantly better. Set honest expectations: "studio recording tool, not live performance." Show latency in the UI so users understand their connection quality.

**Reliability at scale**
- Synths crash, MIDI locks up, audio glitches. One bad session and a user churns.
- Running 10 synths 24/7 unattended is very different from using them in your own studio
- *Mitigation:* MIDI heartbeat monitoring per synth, auto-restart on failure, session credits for technical issues. Start with modern/reliable synths, add vintage carefully. The Juno-106 will cause more support headaches than all other synths combined.

**Internet dependency**
- If a user's connection drops mid-session, they lose their flow (not their work — server-side recording catches everything)
- Frustrating in a way that local gear never is
- *Mitigation:* Server-side recording as backup, automatic reconnection, session time paused during disconnection. Recommend wired ethernet in the plugin UI when jitter is high.

### Market Risks

**Market might be too small**
- How many producers would actually pay monthly for remote synth access vs just buying a Minilogue or using plugins?
- The "real hardware matters" crowd might be smaller than we think
- *Mitigation:* Soft launch validates demand before major investment. Pro-first strategy means you only need ~35 Professional subscribers at £149 to hit meaningful revenue. If you can't find 35 engineers who'll pay £149/month, the market isn't there.

**Plugins keep getting better**
- Neural Amp Modeler, AI-based modelling, increasingly sophisticated emulations
- If someone releases a Moog One plugin that's 99% indistinguishable, the value proposition weakens
- *Mitigation:* This is a slow trend, not an overnight disruption. Analogue purists will always exist. The effects rack (plate reverb, tape echo) is harder to model than synths. The experience of "this is a real synth" has marketing/emotional value beyond pure audio quality. Position around authenticity, not just sound quality.

**"Good enough" kills premium**
- Most listeners can't tell a Diva from a real Moog in a finished mix
- Producers might know the difference but decide it doesn't matter enough to pay for
- *Mitigation:* Target producers who already believe hardware matters — don't try to convert plugin loyalists. The project recall lock-in means even if they start questioning the value, switching cost keeps them subscribed.

### Competitive Risks

**Someone with more money replicates it**
- If Anarack proves the model works, a well-funded company (Splice, Native Instruments, Roland Cloud) could replicate it with bigger budgets, more synths, and existing user bases
- *Mitigation:* First-mover advantage, community, brand loyalty. The partner network and manufacturer relationships create switching costs. The white-label business makes Anarack the infrastructure layer rather than a competitor. See also: Splice as a partner/acquirer below.

**Synth manufacturers do it themselves**
- Moog could offer "play our synths remotely" directly. They have the synths, the brand, and the audience.
- *Mitigation:* Manufacturers don't want to run a streaming infrastructure business. The white-label play specifically defends against this — position Anarack as the technology partner, not a competitor. "We power your remote demos" is more attractive to Moog than building it themselves.

### Operational Risks

**Single point of failure**
- One person, one location, one internet connection, one power supply. If broadband goes down or a fuse blows, the entire service is offline.
- *Mitigation:* UPS for power continuity, 4G/5G failover for internet, monitoring and alerts. Partner studios (Phase 2) add geographic redundancy. This is the strongest argument for getting partners online early.

**Maintenance burden**
- Vintage synths break. Juno-106 voice chips, Minimoog tuning, Space Echo tape loops. You become a synth technician as much as a tech company.
- *Mitigation:* Start with mostly modern/reissue synths (Minimoog 2022 reissue, not vintage). Budget properly for maintenance (already in the financials). Only add vintage pieces once revenue supports the maintenance overhead. Build relationships with synth repair specialists.

**Scaling is physical**
- Unlike software, you can't spin up more servers. More users = more synths = more space, power, maintenance, money.
- *Mitigation:* The partner network is the scaling solution — other people's synths, other people's space. Low utilisation is a feature (subscribers pay but don't use all hours). You don't need to scale linearly with users.

### Financial Risks

**High upfront cost relative to uncertain demand**
- £25-35k in synths before you know if people will pay at proper launch scale
- *Mitigation:* Soft launch validates demand with zero synth investment. Crowdfunding shifts risk to backers who've already committed. Manufacturer partnerships and partner studios reduce capital requirements. Buy synths incrementally as revenue grows.

**Revenue concentration**
- At £10k MRR, 52% comes from 35 Professional subscribers. Losing 5 Pros = losing £745/month (7.4% of revenue).
- *Mitigation:* Diversify revenue across tiers as Creator base grows. Effects add-on creates additional revenue streams. Annual subscriptions reduce churn risk. Project recall makes cancellation painful.

### Behavioural Risks

**Producers are creatures of habit**
- Convincing someone to change their workflow (open a plugin, connect to a remote synth, deal with latency) when they could just load Diva and start playing is a hard sell
- *Mitigation:* The DAW plugin minimises workflow change — it's just another instrument. The browser demo requires zero change to try. Target producers who are already frustrated with plugin sound, not happy plugin users.

**The novelty wears off**
- "Playing a real Moog remotely" is exciting the first time. After 6 months, is it still worth the subscription?
- *Mitigation:* Project recall lock-in — their projects depend on Anarack. Hours rollover creates sunk cost. Expanding synth catalogue gives a "what's new" reason to stay. Community and presets add value beyond the core product. This is why the Splice-style credit retention matters.

## Strategic Opportunities & Exit

### Splice Partnership / Acquisition

Splice is the most natural partner or acquirer for Anarack. The strategic fit is almost perfect:

**Why Splice makes sense:**
- They already have **Splice Rent-to-Own** — a plugin/VST rental library with millions of users. Anarack is the physical hardware extension of the same concept.
- They have the user base (millions of producers), the billing infrastructure, and the brand trust in the producer community
- "Splice Hardware" or "Splice Studio" — real synths accessible through Splice's existing subscription — is an obvious product line extension
- They understand the credit/rollover retention model (Splice credits work exactly the same way as Anarack's hour rollover)
- Anarack's tech (low-latency audio streaming, MIDI routing, servo-controlled effects) is genuinely hard to build. Buying is faster and cheaper than building.

**Partnership model:**
- Anarack powers a "Hardware Synths" section within Splice's platform
- Splice drives users, Anarack provides the infrastructure and gear
- Revenue share or white-label licensing fee
- Anarack retains independence but gets access to Splice's massive user base

**Acquisition model:**
- Splice acquires Anarack as a hardware streaming division
- Anarack's tech becomes Splice's competitive moat — no other sample/plugin platform has real hardware
- Synth catalogue and partner network transfer as assets
- Exit valuation based on recurring revenue, tech IP, and strategic value to Splice's platform

**When this becomes relevant:**
- Not at soft launch — too early, no leverage
- At £10k+ MRR with proven tech and growing user base, it's a credible conversation
- A working white-label deployment with a manufacturer would significantly increase attractiveness
- The ideal timing: approach Splice (or let them approach you) once Anarack has proven the model but before scaling requires massive capital investment

**Other potential acquirers/partners:**
- **Universal Audio** — the strongest fit after Splice. Their entire business model is subscription-locked plugins that your projects depend on — Anarack's project recall is literally modelled on this. They already have hardware (Apollo interfaces) + software (UAD plugins) + subscription (Spark). Adding real hardware synth access is a natural next step. They understand the lock-in model better than anyone.
- **Native Instruments** — they have Komplete, hardware controllers (Komplete Kontrol, Maschine), and a massive producer audience. Hardware synth access would complement their ecosystem. They're also owned by Francisco Partners (PE) who are looking to grow the platform.
- **Plugin Alliance** — already connected via Motive Unknown. They run a plugin subscription model. Hardware synth access is a natural upsell. Smaller company, so potentially more interested in an acqui-hire or partnership.
- **Roland Cloud** — Roland already has a cloud subscription for their own software synths. Adding real hardware (including their own Juno) is a logical extension. They have the brand cachet and the hardware catalogue.
- **Focusrite Group** (Focusrite/Novation/Sequential) — they own both audio interfaces and synth brands. Anarack's tech could power a "try before you buy" feature for their own products. They acquired Sequential in 2021 — they're actively building a synth ecosystem.
- **Sweetwater / Thomann** — major retailers who could use the tech as a "try before you buy" sales tool. Less likely acquirers, more likely partners or licensees.
- **inMusic** (Akai, Alesis, Denon DJ, M-Audio) — they own multiple music tech brands and are acquisitive. Less synth-focused but have the infrastructure.
- **Ableton** — they don't make hardware synths but they dominate the DAW market. A "real hardware synths inside Ableton" integration would be a killer feature. Long shot as an acquirer but a dream partnership.

### Defensibility Summary

| Threat | Primary defence | Secondary defence |
|---|---|---|
| Latency not good enough | Soft launch proves viability before investment | Honest positioning as recording tool, not live instrument |
| Plugins get better | Target analogue purists, not plugin converts | Effects rack (plate, tape) is harder to model |
| Well-funded competitor | First-mover, community, manufacturer relationships | White-label makes us the infrastructure, not the competitor |
| Market too small | Pro-first strategy needs only 35 subscribers | Label partnerships drive volume through warm channels |
| Single point of failure | UPS, 4G failover, monitoring | Partner studios add redundancy |
| Novelty wears off | Project recall lock-in, hour rollover | Growing catalogue, community, effects expansion |
| High upfront cost | Soft launch validates with zero synth cost | Crowdfunding, manufacturer deals, partner studios |
