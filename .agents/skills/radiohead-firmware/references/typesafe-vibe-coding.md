# TypeSafe and Jev for vibe coding

Read this reference for open-ended feature exploration, agent/skill routing, requirement
interpretation, review prioritization, or a proposed TypeSafe integration.

## Role in the workflow

Jev is TypeSafe's System One model: it accepts application state plus typed questions and
returns bounded decisions, probabilities, and confidence. It does not generate source
code or choose an agent's next action by itself.

Use a three-part workflow:

1. The coding agent explores, explains, plans, and generates code.
2. Jev optionally supplies narrow semantic judgments that improve routing or gating.
3. Deterministic tools compile, test, measure, and verify the result.

This separation is useful for vibe coding because natural-language requests are often
ambiguous while the build and device constraints are exact.

## Promising developer-side judgments

- **Skill routing:** choose the most relevant skill from a bounded roster, then let the
  coding agent confirm or reject the suggestion.
- **Task routing:** classify a request as explanation, diagnosis, implementation, review,
  or hardware-validation work when the user's language is ambiguous.
- **Requirement coverage:** judge each natural-language requirement independently against
  a concise proposed plan or change summary.
- **Risk triage:** score review concerns such as likely behavior change, hardware-test
  dependence, or uncertainty in an external API assumption. Code decides which scores
  require a deeper review.
- **Evidence relevance:** rank small candidate excerpts from documentation, logs, or diffs
  before presenting the most relevant evidence to the coding model.

Treat these as hypotheses to evaluate on real project examples, not guarantees.

## Primitive selection

- Use **Choice** for one selection from a closed set, such as the best matching module or
  skill. Include a no-match option when no candidate may fit.
- Use **Noul** for an independent yes/no probability, such as whether a requirement is
  covered or a change plausibly needs physical hardware testing.
- Use **Score** for ordered semantic levels, such as negligible/limited/material/critical
  user-visible risk. Define each level concretely.

Ask independent questions together. Keep the relevant source text and identifiers in
named state fields. Put policy and thresholds in code, not in inferred model behavior.

## What Jev should not do

- Do not ask it to write C++, HTML, configuration, patches, or prose.
- Do not use it for arithmetic, counting, dates, exact bounds, pin conflicts, or memory
  calculations.
- Do not use it as evidence that code compiles, a route works, audio remains stable, or a
  device wakes correctly.
- Do not send the whole repository when a small diff, requirement list, or candidate set
  answers the question. Irrelevant state reduces accuracy.
- Do not assume mathematical relationships between separately asked probabilities.
- Do not trust adversarial or instruction-like content inside state without testing the
  precise question and criteria.

## Experiment protocol

When `TYPESAFE_API_KEY` is available and a live experiment is within the user's request:

1. Read the current TypeSafe documentation and selected SDK/API reference.
2. Name the decision the software needs; do not begin with a generic prompt.
3. Prepare minimal state and one narrow question per independently useful judgment.
4. Keep questions, criteria, thresholds, and the selected model version together in an
   easy-to-review developer-side file. Do not add them to the ESP32 firmware by default.
5. Test representative positive, negative, ambiguous, and boundary examples.
6. Compare the Jev-assisted result with the coding agent alone and with deterministic
   checks. Record both improvements and regressions.
7. Make failure safe: uncertain or low-quality judgments should fall back to the coding
   agent, human review, or normal deterministic workflow.

Never print, commit, or transmit the API key to the device. Do not make TypeSafe a build
dependency unless a concrete maintained developer tool is intentionally added.

## Current primary references

- Documentation index: <https://docs.typesafe.ai/llms.txt>
- Agent skill and vibe-coding guidance: <https://docs.typesafe.ai/agent-skill>
- Building with System One: <https://docs.typesafe.ai/concepts/how-to-build-with-system-one>
- Skill-suggestion cookbook: <https://docs.typesafe.ai/cookbooks/skill_suggestion>
- Current Jev limitations: <https://docs.typesafe.ai/model-jaggedness/jev-1.13>

Read the live pages before implementing an integration because models, SDK contracts,
limits, and recommendations can change.

