# Financial Planning

## Startup Costs

### Phase 0 — Prototype (already have most of this)

| Item | Cost |
|---|---|
| Raspberry Pi 5 (8GB) | £75 |
| USB cables + misc | £10 |
| Focusrite Scarlett (owned) | £0 |
| Prophet Rev2 (owned) | £0 |
| Moog Sub37 (owned) | £0 |
| **Total** | **~£85** |

### Phase 1 — Launch (5 synths)

| Item | Cost |
|---|---|
| Mac Mini M4 Pro | £1,600 |
| MOTU 24Ai + 24Ao | £2,500 |
| MIDI interface (mioXL) | £300 |
| 3 additional synths (used) | £1,500 |
| Rack, patchbay, cables | £500 |
| UPS + power conditioning | £250 |
| Domain, hosting, Stripe setup | £100 |
| **Total** | **~£6,750** |

### Phase 2 — Growth (10-15 synths)

| Item | Cost |
|---|---|
| 5-10 additional synths (used) | £3,000-6,000 |
| Additional MIDI interface | £250 |
| ADAT expander (if needed) | £200 |
| **Total** | **~£3,500-6,500** |

### Total Investment to Full Scale

**£10,000-13,000** to get to 15 synths with professional infrastructure. This can be spread across 6-12 months as revenue grows.

## Monthly Operating Costs

| Item | Monthly Cost |
|---|---|
| Broadband (fibre, static IP) | £50 |
| Electricity (rack + synths on standby) | £40 |
| Tailscale Business (if needed) | £0-45 |
| Domain + hosting (web app) | £20 |
| Stripe fees (2.9% + 20p per transaction) | ~3% of revenue |
| Synth maintenance / calibration | £50 (averaged) |
| Insurance (equipment) | £30 |
| **Total fixed costs** | **~£200-250/month** |

## Revenue Projections

### Assumptions

- 5 synths at launch, growing to 15 by month 12
- Sessions are 1-hour slots at £15/hour (blended rate including bundles)
- Operating hours: 16 hours/day (covering UK + US time zones)
- Utilisation grows from 10% at launch to 30%+ at scale

### Subscriber Growth (Year 1)

Pro-first launch strategy. Revenue is subscription-driven. The key metric is subscriber count, not hours booked. Many subscribers will pay monthly but only use a fraction of their hours — that's the model working as intended (UA-style retention via project recall).

**Phase 1 (Months 1-6): Professional tier only.** Focus on working engineers and producers. Higher ARPU, fewer customers needed, better product feedback.

**Phase 2 (Months 7+): Open Creator + Creator Pro tiers.** Once proven with pros, widen the funnel. Queue pressure from new users naturally drives upgrades.

| Month | Pro | Creator Pro | Creator | Sub Revenue | PAYG | Total | Costs | Profit |
|---|---|---|---|---|---|---|---|---|
| 1 (beta) | 5 | — | — | £745 | £50 | £795 | £200 | £595 |
| 2 | 10 | — | — | £1,490 | £100 | £1,590 | £200 | £1,390 |
| 3 (launch) | 15 | — | — | £2,235 | £200 | £2,435 | £250 | £2,185 |
| 4 | 20 | — | — | £2,980 | £300 | £3,280 | £250 | £3,030 |
| 5 | 25 | — | — | £3,725 | £400 | £4,125 | £250 | £3,875 |
| 6 | 28 | — | — | £4,172 | £500 | £4,672 | £250 | £4,422 |
| 7 | 30 | 10 | 30 | £5,340 | £600 | £5,940 | £250 | £5,690 |
| 8 | 32 | 25 | 60 | £6,643 | £700 | £7,343 | £250 | £7,093 |
| 9 | 34 | 35 | 80 | £7,631 | £800 | £8,431 | £250 | £8,181 |
| 10 | 35 | 45 | 100 | £8,490 | £900 | £9,390 | £250 | £9,140 |
| 11 | 37 | 50 | 120 | £9,263 | £1,000 | £10,263 | £250 | £10,013 |
| 12 | 40 | 55 | 140 | £10,205 | £1,100 | £11,305 | £250 | £11,055 |

**Year 1 total revenue: ~£69,000**
**Year 1 total profit (after operating costs): ~£66,000**
**Year 1 profit (after startup investment): ~£59,000-63,000**

### Path to £10k MRR

With the pro-first approach + upgrade ladder:
- 35 Professional (£149) + 50 Creator Pro (£39) + 100 Creator (£15)
- = £5,215 + £1,950 + £1,500 = £8,665/month subscriptions
- Plus PAYG + bolt-ons: ~£1,400/month
- **Total: ~£10,000/month — reachable by month 10-11**

The upgrade ladder accelerates this: as Creator subscribers hit queues during peak, some upgrade to Creator Pro (£39), which nearly triples their contribution.

### Why Low Utilisation Is a Feature

We *want* subscribers who don't use all their hours:
- A Creator pays £15/month for 2 hours. If they use 45 mins, we made £15 for 45 mins of synth time
- A Creator Pro pays £39/month for 8 hours. Most months they'll use 3-4. That's £10-13 effective hourly rate — but we keep the subscription.
- A Professional pays £149/month for 30 hours. Most months they'll use 8-12. That's £12-19 effective hourly rate.
- Their projects depend on Airsynth (project recall), so they all keep paying
- Synth capacity stays available — we never hit a wall where everyone's trying to use their hours at once

Target utilisation: 15-25% of total capacity. Higher than that = time to add more synths.

## Break-Even Analysis

Monthly fixed costs: ~£250
Variable costs: 3-5% of revenue (payment gateway fees — see below)

**Break-even: ~£260-275/month** (trivially low)

The real question is break-even on the **hardware investment**. At £6,750 startup cost:
- Month 3-4 revenue covers operating costs
- Cumulative profit covers startup investment by **month 8-10**

## Payment Gateway: Stripe vs Paddle

This is a critical decision because it determines how much international tax admin we take on.

### Option 1: Stripe

**Pros:**
- Familiar (already used it), excellent developer experience
- Lower base fees: 1.5% + 20p (UK cards), 2.5% + 20p (EU), 3.25% + 20p (international)
- Full control over billing, invoicing, customer relationship
- Stripe Billing handles subscription management, proration, dunning
- Stripe Tax add-on can calculate and collect VAT/sales tax globally

**Cons:**
- **We are the Merchant of Record** — we're responsible for collecting and remitting VAT/GST/sales tax in every country we sell to
- Stripe Tax helps calculate the right rate, but we still file the returns ourselves (or via an accountant)
- Need to register for VAT in the UK immediately (digital services to consumers)
- May need to register for VAT OSS (EU One Stop Shop) for EU customers
- US sales tax varies by state — Stripe Tax calculates it, but compliance is on us
- More admin overhead as we scale internationally

**Cost at £10k MRR (estimated):**
- ~70% UK cards (1.5% + 20p): ~£120
- ~20% EU cards (2.5% + 20p): ~£55
- ~10% international (3.25% + 20p): ~£35
- Stripe Tax: +0.5% per transaction: ~£50
- **Total: ~£260/month (~2.6% effective rate)**

### Option 2: Paddle

**Pros:**
- **Paddle is the Merchant of Record** — they handle all VAT/GST/sales tax collection, filing, and remittance globally. We never touch tax compliance.
- One invoice from Paddle to us, they handle all customer tax invoices
- Built for SaaS/digital subscriptions specifically
- No need to register for VAT ourselves (Paddle is the seller, not us)
- Handles EU VAT OSS, US sales tax, Australian GST, etc. — all automatic
- Significantly less admin overhead, especially as we grow internationally

**Cons:**
- Higher fees: 5% + 50p per transaction (standard rate)
- Less control over the checkout experience (improving but not as flexible as Stripe)
- We don't "own" the customer billing relationship in the same way
- Payout timing: Paddle pays us on a schedule (typically net 7-14 days), not instant like Stripe
- Fewer integrations than Stripe's ecosystem

**Cost at £10k MRR (estimated):**
- 5% + 50p across all transactions: ~£550/month
- No additional tax calculation fees
- **Total: ~£550/month (~5.5% effective rate)**

### Recommendation

**Start with Paddle. Switch to Stripe later if/when it makes sense.**

The reasoning:
- At early stage, international tax compliance is a distraction. Paddle eliminates it entirely.
- The fee difference is ~£290/month at £10k MRR — that's the price of not dealing with VAT returns in 5+ countries. Worth it.
- As a sole operator, every hour spent on tax admin is an hour not spent on the product or marketing.
- If/when revenue hits £20k+ MRR and we hire an accountant, we can evaluate switching to Stripe + Stripe Tax for the lower fees.
- Paddle is widely used by plugin/music tech companies (similar audience, similar model).

**Decision point:** revisit at £20k MRR. At that scale, the fee difference is ~£600/month, which starts to justify the cost of professional tax compliance support + Stripe.

### Price Display

With Paddle as Merchant of Record:
- Prices shown to customers are inclusive of local tax (Paddle handles this automatically)
- UK user sees: £15/month (includes VAT)
- US user sees: $19/month (includes applicable sales tax)
- EU user sees: €18/month (includes VAT)
- Paddle handles currency conversion and local pricing — we set base prices and they adjust

## VAT & International Tax

### Digital Services VAT (UK + International)

Digital services (which Airsynth is) have specific VAT rules:

**If using Paddle:** Paddle handles all of this. Skip this section.

**If using Stripe (or switching later):**

- **UK:** Must register for VAT and charge 20% on UK consumer sales from day one. The £90k threshold applies to physical goods, not digital services sold B2C. B2B sales to VAT-registered businesses can be reverse-charged.
- **EU:** Must register for VAT OSS (One Stop Shop) and charge each country's VAT rate (17-27%). One quarterly return covers all EU countries.
- **US:** Sales tax varies by state. Stripe Tax calculates the rate. Must register in states where we have "economic nexus" (typically >$100k revenue or 200 transactions in a state).
- **Australia/NZ:** GST at 10%/15% on digital services to consumers.
- **Canada:** GST/HST varies by province.

This is manageable but grows in complexity with each new market. An accountant with digital services experience is essential if going the Stripe route.

### Pricing Strategy for International

- Set base prices in GBP
- Use Paddle's (or Stripe's) local pricing to show equivalent prices in local currency
- Consider purchasing power parity for some markets — a £15/month subscription is affordable in the UK/US but expensive in some markets. Could offer regional pricing later.

## Tax Considerations (UK Business)

- Trade under existing company: **Studio Audience Ltd** trading as **Airsynth** (no new company needed)
- "Airsynth is a trading name of Studio Audience Ltd" in footer/terms
- Ensure SIC codes on Companies House cover digital services (free to add if needed)
- Equipment purchases are capital allowances (tax deductible)
- Home studio: portion of mortgage/rent, utilities, broadband as business expense
- Corporation tax on profits (currently 25% for profits over £250k, 19% for small profits under £50k)
- Director's salary + dividends for tax-efficient personal income
- **Get an accountant before launch** — digital services VAT + international sales is specialist territory. Budget ~£150-200/month for a good one.
