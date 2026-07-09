# Documentation Map

## Read Order

For a new human or agent joining the project, read these in order:

1. `README.md`
2. `AGENTS.md`
3. `docs/harness/README.md`
4. `docs/harness/agent-workflow.md`
5. `docs/harness/testing-and-validation.md`
6. `docs/adr/ADR-0001-esp-idf-first.md`
7. `docs/architecture/repo-layout.md`
8. `docs/architecture/board-bringup-interface-map.md`
9. `docs/architecture/refresh-and-sync-policy.md`
10. `docs/architecture/provisioning-and-config.md`
11. `docs/architecture/runtime-scheduler.md`
12. `docs/architecture/sleep-schedule-and-manual-wake.md`
13. `docs/architecture/serial-config-protocol.md`
14. `docs/architecture/provider-abstraction.md`
15. `docs/architecture/ui-home-screen-proposal.md`
16. `docs/architecture/roadmap.md`
17. `docs/research/README.md`
18. `docs/research/decisions/framework-evaluation.md`
19. `docs/research/decisions/display-library-evaluation.md`
20. `docs/research/decisions/esp-idf-vs-arduino.md`
21. `docs/research/waveshare/board-facts.md`
22. `docs/research/waveshare/power-and-buttons.md`
23. `docs/research/providers/minimax.md`
24. `docs/examples/sdcard-config.example.json`
25. `docs/questions/decision-checklist.md`

## Sections

- `adr/` - architecture decision records
- `architecture/` - repository shape, firmware boundaries, roadmap
- `development/` - in-flight development specs (e.g. `image-dashboard.md`)
- `examples/` - operator-facing config examples and future captured fixtures
- `harness/` - how AI agents are expected to work in this repo
- `questions/` - unresolved product and technical decisions
- `research/` - vendor facts, provider notes, reference-project analysis

## Active Spec

The current dashboard feature is defined in
[`docs/development/image-dashboard.md`](development/image-dashboard.md).
The older multi-domain PRD (`docs/architecture/multi-domain-dashboard.md`)
is kept as reference but is superseded by the image-dashboard spec.

## Minimum Document Set For Continuing Firmware Work

- `docs/adr/ADR-0001-esp-idf-first.md`
- `docs/architecture/repo-layout.md`
- `docs/architecture/roadmap.md`
- `docs/architecture/board-bringup-interface-map.md`
- `docs/architecture/refresh-and-sync-policy.md`
- `docs/architecture/runtime-scheduler.md`
- `docs/architecture/sleep-schedule-and-manual-wake.md`
- `docs/architecture/provisioning-and-config.md`
- `docs/architecture/serial-config-protocol.md`
- `docs/architecture/provider-abstraction.md`
- `docs/architecture/ui-home-screen-proposal.md`
- `docs/harness/testing-and-validation.md`
- `docs/research/decisions/framework-evaluation.md`
- `docs/research/decisions/display-library-evaluation.md`
- `docs/research/decisions/esp-idf-vs-arduino.md`
- `docs/research/waveshare/board-facts.md`
- `docs/research/waveshare/power-and-buttons.md`
- `docs/research/providers/minimax.md`
- `docs/questions/decision-checklist.md`

## Writing Rules

- Prefer stable facts over speculation.
- When a claim comes from a vendor example or reference repo, name the source path.
- Promote only durable decisions into `adr/`.
- Keep provider-specific details out of architecture docs when possible.
- Keep board-specific details out of provider docs when possible.
- When validated implementation changes behavior, update the matching docs in the same task.
