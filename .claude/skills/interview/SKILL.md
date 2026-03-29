---
name: interview
description: Interview the user about a feature or decision, producing an allium spec
argument-hint: <feature-name>
---

# Stakeholder Interview: "$ARGUMENTS"

You are conducting a structured stakeholder interview to capture the intent and rules for a feature, producing an `.allium` spec file.

## Setup

1. Invoke `/allium` to load the allium language reference
2. Generate the output filename: `docs/plans/$(date +%Y%m%d-%H%M)-allium-$ARGUMENTS.allium`

## Interview Process

Use `AskUserQuestion` for each question — **one question at a time**, never batch multiple questions.

### Phase 1: Scope

Establish what we're building and what we're NOT building.

Ask about:

- What is this in one sentence?
- Who/what are the actors involved? (users, hardware, services, protocols)
- What external systems does it interact with? (Pi, JACK, synths, browser, network)
- What is explicitly OUT of scope?

After this phase, write the file header comment and `external entity` declarations.

### Phase 2: Happy Path

Walk through the core flow step by step.

Ask about:

- What triggers the flow?
- What happens next? (repeat until flow completes)
- What data moves between steps? (MIDI, audio, control messages)
- What does the user see/hear at each stage?

After this phase, write `entity`, `value`, and core `rule` blocks.

### Phase 3: Edge Cases

Probe for rules the stakeholder hasn't mentioned yet.

Ask about:

- What happens when [X] fails? (network drops, hardware disconnects, latency spikes)
- Are there limits or caps? (concurrent users, session length, buffer sizes)
- What if the user does [unexpected thing]?
- Are there timing or ordering constraints?

After this phase, add guard rules and invariants.

### Phase 4: Refinement

Review what we have and tighten it.

- Read back a summary of the spec so far
- Ask: "Is anything missing or wrong?"
- Ask: "Are there any decisions you'd like to defer for now?"
- Add `deferred` declarations and resolved questions

## Writing the Spec

Write the `.allium` file incrementally after each phase — don't wait until the end. Follow the allium conventions:

- `external entity` for things outside our control
- `value` for immutable data shapes
- `entity` for stateful domain objects
- `rule` for business logic (when/requires/ensures)
- `surface` for UI contracts
- `deferred` for things we acknowledge but won't specify yet
- Use `-- comments` for resolved questions and context

## Wrapping Up

When the interview is complete:

1. Read back the full spec to the stakeholder for final approval
2. Summarise any open questions as `-- Deferred:` comments in the spec
3. Suggest running `/plan $ARGUMENTS` as the next step to create an implementation plan from the spec
