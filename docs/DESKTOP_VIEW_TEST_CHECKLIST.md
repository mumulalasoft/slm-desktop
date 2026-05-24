# Desktop View Runtime Verification Checklist

## Scope

This checklist validates the system contract:

`Desktop View = FileManager(path=~/Desktop, mode=desktop_view)`

It is focused on runtime behavior, menu source, selection dynamics, fallback policy, and layering.

---

## Preconditions

1. Build and run current shell build.
2. Ensure `~/Desktop` exists (or allow auto-create test).
3. Ensure Crown global menu is visible.
4. Ensure no detached File Manager window is active at test start.

---

## A. Filesystem Binding

### A1. Desktop uses real filesystem content

Steps:
1. Create a file in terminal: `touch ~/Desktop/dv-test-a.txt`.
2. Observe desktop surface.

Expected:
1. `dv-test-a.txt` appears on desktop.
2. Item is selectable and activatable.
3. No static/fake icon list behavior.

### A2. Incremental update

Steps:
1. Delete file from terminal: `rm ~/Desktop/dv-test-a.txt`.
2. Observe desktop surface.

Expected:
1. File disappears without full shell restart.
2. No UI freeze.

### A3. Missing desktop directory resilience

Steps:
1. Move desktop dir: `mv ~/Desktop ~/Desktop.bak`.
2. Trigger refresh (click desktop / reopen session).

Expected:
1. `~/Desktop` is recreated automatically.
2. Desktop surface remains functional.

---

## B. Selection + Dynamic Global Menu

### B1. No selection menu

Steps:
1. Ensure no app window is active/focused.
2. Click empty desktop area.
3. Open global menu "File".

Expected:
1. Menu source is desktop provider context.
2. "File" includes:
   - `New Folder`
   - `Open Terminal Here`

### B2. Single selection menu

Steps:
1. Create one file on desktop.
2. Select exactly one item.
3. Open global menu "File".

Expected:
1. Menu updates to single-selection actions:
   - `Open`
   - `Rename`
   - `Compress`
   - `Move to Trash`
   - `Properties`

### B3. Multi selection menu

Steps:
1. Select 2+ items on desktop.
2. Open global menu "File".

Expected:
1. Menu updates to multi-selection actions:
   - `Compress`
   - `Move to Trash`
2. Single-only actions are not shown.

---

## C. Action Flow Correctness

### C1. Rename action uses File Manager flow

Steps:
1. Select one desktop item.
2. Trigger global menu `File -> Rename`.

Expected:
1. Rename flow is handled by FileManager window/dialog pipeline.
2. Item name change is reflected on desktop after apply.

### C2. Properties action uses File Manager flow

Steps:
1. Select one desktop item.
2. Trigger global menu `File -> Properties`.

Expected:
1. Properties dialog opens via FileManager flow.
2. Metadata reflects real file path on `~/Desktop`.

### C3. Trash action

Steps:
1. Select file(s).
2. Trigger `Move to Trash`.

Expected:
1. Files removed from desktop listing.
2. Standard trash behavior applies.

---

## D. Global Menu Source Policy

### D1. No active app -> desktop provider

Steps:
1. Ensure no active app window.
2. Open crown global menu.

Expected:
1. Menu remains available.
2. Source is desktop menu provider, not fallback placeholder.

### D2. Active app exists -> app menu

Steps:
1. Focus an app window that exports menu.
2. Open crown global menu.

Expected:
1. App menu is used.
2. Desktop provider is not forced when app context exists.

### D3. Return to desktop context

Steps:
1. Unfocus/close active app.
2. Click desktop.
3. Open crown global menu.

Expected:
1. Desktop menu provider becomes active again.
2. Selection-sensitive menu is restored.

---

## E. Layering (Z-Order)

### E1. Desktop surface placement

Expected visual order:
1. Crown / AppDeck / AppHub overlays above windows.
2. App windows above DesktopSurface.
3. DesktopSurface above wallpaper.

Fail conditions:
1. Desktop icons appear above app windows.
2. Desktop surface is hidden below wallpaper.
3. Menu popups render behind desktop.

---

## F. Failure-Mode Policy

### F1. Backend unavailable

Simulation:
1. Force model/backend unavailable state (debug build or injected failure).

Expected:
1. Desktop provider is disabled.
2. Fallback menu may be used only in this complete failure case.
3. No crash loop.

### F2. Recovery

Steps:
1. Restore backend availability.

Expected:
1. Desktop provider re-enabled.
2. Menus and selection context recover automatically.

---

## Pass Criteria

System is valid only if all are true:

1. Desktop shows real `~/Desktop` entries.
2. Global menu source in idle context is DesktopMenuProvider (not fallback).
3. Menu actions adapt to selection count.
4. Rename/properties routes through FileManager flow.
5. Layer order remains correct and stable.
