---
name: interview
description: Interview the user about a feature or decision, producing a spec
argument-hint: <feature-name>
---

# Interview: "$ARGUMENTS"

You are conducting a structured interview to capture the intent, constraints, and rules for a feature or decision.

## Setup

Generate the output filename: `docs/plans/$(date +%Y%m%d-%H%M)-interview-$ARGUMENTS.md`

## Interview Process

Use `AskUserQuestion` for each question — **one question at a time**, never batch multiple questions.

### Phase 1: Scope

Establish what we're building and what we're NOT building.

Ask about:

- What is this in one sentence?
- Who/what interacts with it? (users, hardware, services)
- What is explicitly OUT of scope?

### Phase 2: Happy Path

Walk through the core flow step by step.

Ask about:

- What triggers the flow?
- What happens next? (repeat until flow completes)
- What data moves between steps?
- What does the user see/hear at each stage?

### Phase 3: Edge Cases

Probe for things not yet mentioned.

Ask about:

- What happens when [X] fails? (network drops, hardware disconnects, latency spikes)
- Are there limits or caps? (concurrent users, session length, audio quality)
- What if the user does [unexpected thing]?
- Are there timing or ordering constraints?

### Phase 4: Refinement

Review what we have and tighten it.

- Read back a summary of the spec so far
- Ask: "Is anything missing or wrong?"
- Ask: "Are there any decisions you'd like to defer for now?"

## Writing the Spec

Write the spec incrementally after each phase — don't wait until the end.

## Wrapping Up

When the interview is complete:

1. Read back the full spec for final approval
2. Summarise any open questions
3. Suggest running `/plan $ARGUMENTS` as the next step to create an implementation plan
