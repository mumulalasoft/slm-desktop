#!/usr/bin/env bash
set -euo pipefail

echo "[folder-sharing-smoke] start"

if ! command -v net >/dev/null 2>&1; then
  echo "[folder-sharing-smoke][FAIL] 'net' command not found. Install Samba tools first."
  exit 2
fi

tmp_dir="${TMPDIR:-/tmp}/slm-folder-share-smoke-$$"
share_name="SLM_Smoke_$$"
mkdir -p "$tmp_dir"
cleanup() {
  net usershare delete "$share_name" >/dev/null 2>&1 || true
  rmdir "$tmp_dir" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "[folder-sharing-smoke] usershare info check"
if ! net usershare info >/dev/null 2>&1; then
  echo "[folder-sharing-smoke][WARN] usershare info failed."
  echo "[folder-sharing-smoke][HINT] usually means usershare is not enabled or current user has no permission."
fi

echo "[folder-sharing-smoke] create share name=$share_name path=$tmp_dir"
if ! net usershare add "$share_name" "$tmp_dir" "SLM smoke test share" "Everyone:R" "guest_ok=n" >/tmp/slm-folder-share-add.log 2>&1; then
  echo "[folder-sharing-smoke][FAIL] usershare add failed"
  cat /tmp/slm-folder-share-add.log || true
  exit 1
fi

echo "[folder-sharing-smoke] verify created"
if ! net usershare info "$share_name" >/tmp/slm-folder-share-info.log 2>&1; then
  echo "[folder-sharing-smoke][FAIL] usershare info for created share failed"
  cat /tmp/slm-folder-share-info.log || true
  exit 1
fi

echo "[folder-sharing-smoke] delete share"
if ! net usershare delete "$share_name" >/tmp/slm-folder-share-del.log 2>&1; then
  echo "[folder-sharing-smoke][FAIL] usershare delete failed"
  cat /tmp/slm-folder-share-del.log || true
  exit 1
fi

echo "[folder-sharing-smoke][PASS] usershare add/info/delete flow succeeded"
