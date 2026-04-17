# InputCapture Helper Backend Contract

`InputCaptureService` supports DBus-native and helper adapter modes for compositor integration.

## Mode A: DBus-direct backend (preferred for native integration)

Set:

- `SLM_INPUTCAPTURE_DBUS_SERVICE`
- `SLM_INPUTCAPTURE_DBUS_PATH` (optional, default `/org/slm/Desktop/InputCaptureBackend`)
- `SLM_INPUTCAPTURE_DBUS_IFACE` (optional, default `org.slm.Desktop.InputCaptureBackend`)

When set, `desktopd` forwards InputCapture methods to that DBus service/interface.

Auto-detect (no env required):

- If `org.slm.Compositor.InputCaptureBackend` is present on session bus,
  backend is selected as DBus-native compositor mode.
- Else if `org.slm.Compositor.InputCapture` is present, backend is selected
  as DBus-native compositor mode.
- Else if `org.slm.Desktop.InputCaptureBackend` is present, backend is selected
  as DBus-direct desktop mode.

Current runtime:

- Shell process publishes `org.slm.Compositor.InputCaptureBackend` as
  a stateful provider contract (session/barrier/enable/disable/release).
- Shell process also publishes primitive bridge object:
  - service: `org.slm.Compositor.InputCaptureBackend`
  - path: `/org/slm/Compositor/InputCapture`
  - iface: `org.slm.Compositor.InputCapture`
  - methods: `SetPointerBarriers/EnableSession/DisableSession/ReleaseSession`
- Provider can enforce strict native path with
  `options.require_compositor_command=true`, then operation succeeds only when:
  - structured primitive bridge on command bridge reports `ok=true`, or
  - primitive bridge reports `ok=true`.
- Primitive resolution precedence in provider:
  1. structured primitive methods on command bridge
     (`set/enable/disable/releaseInputCaptureSession`),
  2. primitive DBus bridge (`SLM_INPUTCAPTURE_PRIMITIVE_*`),
  3. command forwarding (`inputcapture ...`).
- Full compositor-native pointer capture/barrier internals remain incremental:
  structured calls are now wired, but concrete compositor-side semantics are still
  backend-dependent.

Primitive bridge override (optional):

- `SLM_INPUTCAPTURE_PRIMITIVE_SERVICE` (default `org.slm.Compositor.InputCaptureBackend`)
- `SLM_INPUTCAPTURE_PRIMITIVE_PATH` (default `/org/slm/Compositor/InputCapture`)
- `SLM_INPUTCAPTURE_PRIMITIVE_IFACE` (default `org.slm.Compositor.InputCapture`)

Compositor command bridge override (optional, KWin IPC client):

- `SLM_INPUTCAPTURE_COMPOSITOR_SERVICE`
- `SLM_INPUTCAPTURE_COMPOSITOR_PATH` (default `/org/slm/Compositor/InputCapture`)
- `SLM_INPUTCAPTURE_COMPOSITOR_IFACE` (default `org.slm.Compositor.InputCapture`)

Notes:

- KWin IPC path for `inputcapture ...` only attempts DBus bridge when
  `SLM_INPUTCAPTURE_COMPOSITOR_SERVICE` is configured.
- This avoids accidental self-recursion with shell-owned primitive object.

Expected methods:

- `CreateSession(sessionPath, appId, options)`
- `SetPointerBarriers(sessionPath, barriers, options)`
- `EnableSession(sessionPath, options)`
- `DisableSession(sessionPath, options)`
- `ReleaseSession(sessionPath, reason)`

Each method should return a `QVariantMap` with at least:

- `ok` (`bool`)
- optional `reason`
- optional `results`

## Mode B: Helper-process backend

Set:

- `SLM_INPUTCAPTURE_HELPER=/path/to/helper-binary`

When DBus-direct variables are not set and helper is set, `desktopd` uses
`InputCaptureBackend` mode `helper-process` and delegates operations to executable helper.

## Invocation model

Helper is executed as:

```bash
<helper-binary> <operation>
```

Where `<operation>` is one of:

- `create`
- `set_barriers`
- `enable`
- `disable`
- `release`

Request payload is written to helper `stdin` as compact JSON.
Helper must return JSON on `stdout`.

## Request payload

Base fields (operation dependent):

- `session_handle` (string)
- `app_id` (string, `create` only)
- `barriers` (array, `set_barriers` only)
- `reason` (string, `release` only)
- `options` (object, when provided by caller)

## Response payload

Expected helper output JSON:

```json
{
  "ok": true,
  "reason": "optional-reason",
  "details": {}
}
```

Rules:

- `ok=true` => backend marks `applied=true`.
- `ok=false` => backend marks `applied=false` and propagates `reason`.
- Non-zero exit, timeout, or invalid JSON => treated as backend failure.

## Timeouts

- process start timeout: `1000 ms`
- operation timeout: `3000 ms`

## Security notes

- Helper path must be executable.
- Helper runs with user session privileges; no elevation is implied.
- Caller-side permission and capability checks still occur in:
  - `ImplPortalService`
  - `InputCaptureService`
- Helper must not make trust decisions; it only executes compositor-specific operations.
