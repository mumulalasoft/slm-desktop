# Contract: SLM Polkit Agent Runtime

Component identity:
- Binary: `slm-polkit-agent`
- User unit: `slm-polkit-agent.service`

## Runtime contract (baseline)
- Agent must register as an authentication agent for the active user session.
- On authentication request (example: `pkexec`), agent must:
  - show auth dialog
  - accept password submit and cancel
  - return success/failure without crashing shell session
- Single active dialog at a time.

## Service lifecycle contract
- Unit starts under user session target.
- On failure, systemd restart policy applies.
- If another authentication agent already exists for same subject:
  - agent logs registration failure reason
  - process behavior must remain deterministic (no crash loop without clear log)

## UI contract (baseline)
- Dialog must include:
  - action/message text
  - identity/user text
  - password input
  - `Cancel` and `OK` actions
- Keyboard:
  - Enter submits when valid
  - Escape cancels
- Theme:
  - follows SLM theme tokens (including runtime changes)

## Error handling contract
- If UI fails to load:
  - process exits with explicit error log
  - systemd status indicates failure reason
- If no agent is available:
  - caller-side error remains explicit (`No authentication agent found`)

## Compatibility policy
- Keep current command-path behavior for `pkexec` auth flow.
- Any state machine expansion (queue/timeout/wrong-password sub-state) must preserve baseline behavior.

## Test and smoke references
- Runtime smoke lane:
  - `scripts/smoke-polkit-agent-runtime.sh`
  - `ctest -R "^polkit_agent_runtime_smoke_test$"` (requires `SLM_POLKIT_RUNTIME_SMOKE=1`)
- Manual verification:
  - `pkexec --disable-internal-agent ls /root`
