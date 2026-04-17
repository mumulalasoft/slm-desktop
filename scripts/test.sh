#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_BUILD_DIR="${ROOT_DIR}/build/toppanel-Debug"

usage() {
  cat <<EOF
Usage:
  $0 [build_dir]
  $0 unlock [build_dir] [ctest_regex]
  $0 filemanager-gates [build_dir] [stable|strict]
  $0 filemanager-smoke [build_dir]
  $0 secret-consent [build_dir]
  $0 policy-core [build_dir]
  $0 storage-smoke [build_dir]
  $0 recovery-smoke
  $0 baseline-flaky [build_dir]
  $0 nightly [build_dir]

Modes:
  default    Run lint + fileops suite + full ctest suite.
  unlock     Run unlock-related DBus test flow (uses secure password prompt helper).
  filemanager-gates  Run FileManager DBus gate subset via unified script.
  filemanager-smoke  Run FileManager smoke+regression subset in isolated DBus session.
  secret-consent     Run Secret consent contract suite (label: secret-consent).
  policy-core        Run stable settings policy suite (label: policy-core).
  storage-smoke      Run storage runtime smoke suite (label: storage-smoke).
  recovery-smoke     Run recovery runtime smoke checks (scripts/units/CLI dispatch).
  baseline-flaky     Run baseline-flaky labeled tests explicitly.
  nightly    Run default suite plus runtime smoke lanes (polkit + package-policy wrapper + secret-consent + policy-core + storage-smoke).
EOF
}

run_default_suite() {
  local build_dir="$1"

  echo "[test] build_dir=${build_dir}"
  local fileops_regex="${SLM_TEST_FILEOPS_REGEX:-^(fileoperationsmanager_test|fileoperationsservice_dbus_test|fileopctl_smoke_test|filemanagerapi_daemon_recovery_test)$}"
  local skip_ui_lint="${SLM_TEST_SKIP_UI_LINT:-0}"
  local skip_animation_lint="${SLM_TEST_SKIP_ANIMATION_LINT:-0}"
  local skip_cap_matrix_lint="${SLM_TEST_SKIP_CAPABILITY_MATRIX_LINT:-0}"
  local skip_topbar_popup_lint="${SLM_TEST_SKIP_TOPBAR_POPUP_LINT:-0}"
  local skip_topbar_recent_menu_guard="${SLM_TEST_SKIP_TOPBAR_RECENT_MENU_GUARD:-0}"
  local topbar_recent_menu_guard_regex="${SLM_TEST_TOPBAR_RECENT_MENU_GUARD_REGEX:-^topbar_mainmenu_recent_icons_guard_test$}"
  local skip_notification_animation_guard="${SLM_TEST_SKIP_NOTIFICATION_ANIMATION_GUARD:-0}"
  local notification_animation_guard_regex="${SLM_TEST_NOTIFICATION_ANIMATION_GUARD_REGEX:-^notification_animation_contract_test$}"
  local skip_notification_visual_guard="${SLM_TEST_SKIP_NOTIFICATION_VISUAL_GUARD:-0}"
  local notification_visual_guard_regex="${SLM_TEST_NOTIFICATION_VISUAL_GUARD_REGEX:-^notification_visual_contract_test$}"
  local skip_notification_priority_guard="${SLM_TEST_SKIP_NOTIFICATION_PRIORITY_GUARD:-0}"
  local notification_priority_guard_regex="${SLM_TEST_NOTIFICATION_PRIORITY_GUARD_REGEX:-^notificationmanager_priority_routing_test$}"
  local exclude_labels="${SLM_TEST_FULL_EXCLUDE_LABELS:-baseline-flaky}"

  if [[ "${skip_ui_lint}" != "1" ]]; then
    echo "[test] running UI style lint"
    "${ROOT_DIR}/scripts/lint-ui-style.sh"
  fi

  if [[ "${skip_animation_lint}" != "1" ]]; then
    echo "[test] running animation token lint"
    "${ROOT_DIR}/scripts/check-animation-token-usage.sh"
  fi

  if [[ "${skip_cap_matrix_lint}" != "1" ]]; then
    echo "[test] running capability matrix lint"
    "${ROOT_DIR}/scripts/check-capability-matrix.sh"
  fi

  if [[ "${skip_topbar_popup_lint}" != "1" ]]; then
    echo "[test] running topbar popup contract lint"
    "${ROOT_DIR}/scripts/check-topbar-popup-contract.sh"
  fi

  echo "[test] running file operations contract suite: ${fileops_regex}"
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
  ctest --test-dir "${build_dir}" --output-on-failure -R "${fileops_regex}"

  if [[ "${skip_topbar_recent_menu_guard}" != "1" ]]; then
    echo "[test] running topbar recent menu guard suite: ${topbar_recent_menu_guard_regex}"
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    ctest --test-dir "${build_dir}" --output-on-failure -R "${topbar_recent_menu_guard_regex}"
  fi

  if [[ "${skip_notification_animation_guard}" != "1" ]]; then
    echo "[test] running notification animation guard suite: ${notification_animation_guard_regex}"
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    ctest --test-dir "${build_dir}" --output-on-failure -R "${notification_animation_guard_regex}"
  fi

  if [[ "${skip_notification_visual_guard}" != "1" ]]; then
    echo "[test] running notification visual guard suite: ${notification_visual_guard_regex}"
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    ctest --test-dir "${build_dir}" --output-on-failure -R "${notification_visual_guard_regex}"
  fi

  if [[ "${skip_notification_priority_guard}" != "1" ]]; then
    echo "[test] running notification priority guard suite: ${notification_priority_guard_regex}"
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    ctest --test-dir "${build_dir}" --output-on-failure -R "${notification_priority_guard_regex}"
  fi

  echo "[test] running full suite"
  if [[ -n "${exclude_labels}" ]]; then
    echo "[test] excluding labels from full suite: ${exclude_labels}"
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    ctest --test-dir "${build_dir}" --output-on-failure -LE "${exclude_labels}"
  else
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    ctest --test-dir "${build_dir}" --output-on-failure
  fi

  local soak_enabled="${SLM_TEST_ENABLE_SOAK:-0}"
  if [[ "${soak_enabled}" == "1" ]]; then
    local soak_minutes="${SLM_TEST_SOAK_MINUTES:-5}"
    local soak_regex="${SLM_TEST_SOAK_REGEX:-^(filemanagerapi_daemon_recovery_test)$}"
    local soak_qpa="${SLM_TEST_SOAK_QPA_PLATFORM:-${QT_QPA_PLATFORM:-offscreen}}"

    if ! [[ "${soak_minutes}" =~ ^[0-9]+$ ]] || [[ "${soak_minutes}" -lt 1 ]]; then
      echo "[test] invalid SLM_TEST_SOAK_MINUTES='${soak_minutes}' (expected integer >= 1)" >&2
      exit 2
    fi

    local soak_seconds=$((soak_minutes * 60))
    local start_ts
    start_ts="$(date +%s)"
    local deadline_ts=$((start_ts + soak_seconds))
    local iter=0

    echo "[test] soak mode enabled"
    echo "[test] soak_minutes=${soak_minutes} soak_regex=${soak_regex} qpa=${soak_qpa}"

    while [[ "$(date +%s)" -lt "${deadline_ts}" ]]; do
      iter=$((iter + 1))
      local now_ts
      now_ts="$(date +%s)"
      local remain=$((deadline_ts - now_ts))
      if [[ "${remain}" -lt 0 ]]; then
        remain=0
      fi
      echo "[test][soak] iteration=${iter} remaining_sec=${remain}"
      QT_QPA_PLATFORM="${soak_qpa}" \
      ctest --test-dir "${build_dir}" --output-on-failure -R "${soak_regex}"
    done

    local end_ts
    end_ts="$(date +%s)"
    local elapsed=$((end_ts - start_ts))
    echo "[test] soak completed elapsed_sec=${elapsed} iterations=${iter}"
  fi
}

run_secret_consent_suite() {
  local build_dir="$1"
  exec "${ROOT_DIR}/scripts/test-secret-consent-suite.sh" "${build_dir}"
}

run_policy_core_suite() {
  local build_dir="$1"
  "${ROOT_DIR}/scripts/test-policy-core-suite.sh" "${build_dir}"
}

run_storage_smoke_suite() {
  local build_dir="$1"
  "${ROOT_DIR}/scripts/test-storage-runtime-suite.sh" "${build_dir}"
}

run_recovery_smoke_suite() {
  "${ROOT_DIR}/scripts/test-recovery-runtime-suite.sh"
}

run_baseline_flaky_suite() {
  local build_dir="$1"
  echo "[test] run baseline-flaky label suite"
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
  ctest --test-dir "${build_dir}" --output-on-failure -L baseline-flaky
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "${1:-}" == "unlock" ]]; then
  if [[ "${2:-}" == "-h" || "${2:-}" == "--help" ]]; then
    usage
    echo
    "${ROOT_DIR}/scripts/test-session-unlock.sh" --help
    exit 0
  fi
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  UNLOCK_REGEX="${3:-sessionstateservice_dbus_test}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 unlock [build_dir] [ctest_regex]" >&2
    exit 2
  fi
  echo "[test] mode=unlock build_dir=${BUILD_DIR} regex=${UNLOCK_REGEX}"
  exec "${ROOT_DIR}/scripts/test-session-unlock.sh" \
    --build-dir "${BUILD_DIR}" \
    --regex "${UNLOCK_REGEX}"
fi

if [[ "${1:-}" == "filemanager-gates" ]]; then
  if [[ "${2:-}" == "-h" || "${2:-}" == "--help" ]]; then
    usage
    echo
    "${ROOT_DIR}/scripts/test-filemanager-dbus-gates.sh" --help
    exit 0
  fi
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  GATE_MODE="${3:-stable}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 filemanager-gates [build_dir] [stable|strict]" >&2
    exit 2
  fi
  if [[ "${GATE_MODE}" != "stable" && "${GATE_MODE}" != "strict" ]]; then
    echo "Invalid gate mode: ${GATE_MODE} (expected stable|strict)" >&2
    exit 2
  fi
  echo "[test] mode=filemanager-gates build_dir=${BUILD_DIR} gate_mode=${GATE_MODE}"
  exec "${ROOT_DIR}/scripts/test-filemanager-dbus-gates.sh" \
    --build-dir "${BUILD_DIR}" \
    "--${GATE_MODE}"
fi

if [[ "${1:-}" == "filemanager-smoke" ]]; then
  if [[ "${2:-}" == "-h" || "${2:-}" == "--help" ]]; then
    usage
    echo
    "${ROOT_DIR}/scripts/test-filemanager-smoke-regression.sh" --help || true
    exit 0
  fi
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 filemanager-smoke [build_dir]" >&2
    exit 2
  fi
  echo "[test] mode=filemanager-smoke build_dir=${BUILD_DIR}"
  exec "${ROOT_DIR}/scripts/test-filemanager-smoke-regression.sh" "${BUILD_DIR}"
fi

if [[ "${1:-}" == "secret-consent" ]]; then
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 secret-consent [build_dir]" >&2
    exit 2
  fi
  echo "[test] mode=secret-consent build_dir=${BUILD_DIR}"
  run_secret_consent_suite "${BUILD_DIR}"
fi

if [[ "${1:-}" == "policy-core" ]]; then
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 policy-core [build_dir]" >&2
    exit 2
  fi
  echo "[test] mode=policy-core build_dir=${BUILD_DIR}"
  run_policy_core_suite "${BUILD_DIR}"
  exit 0
fi

if [[ "${1:-}" == "storage-smoke" ]]; then
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 storage-smoke [build_dir]" >&2
    exit 2
  fi
  echo "[test] mode=storage-smoke build_dir=${BUILD_DIR}"
  run_storage_smoke_suite "${BUILD_DIR}"
  exit 0
fi

if [[ "${1:-}" == "recovery-smoke" ]]; then
  echo "[test] mode=recovery-smoke"
  run_recovery_smoke_suite
  exit 0
fi

if [[ "${1:-}" == "baseline-flaky" ]]; then
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 baseline-flaky [build_dir]" >&2
    exit 2
  fi
  echo "[test] mode=baseline-flaky build_dir=${BUILD_DIR}"
  run_baseline_flaky_suite "${BUILD_DIR}"
  exit 0
fi

if [[ "${1:-}" == "nightly" ]]; then
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 nightly [build_dir]" >&2
    exit 2
  fi
  echo "[test] mode=nightly build_dir=${BUILD_DIR}"
  NIGHTLY_FULL_EXCLUDE_LABELS="${SLM_TEST_NIGHTLY_FULL_EXCLUDE_LABELS:-}"
  if [[ -n "${NIGHTLY_FULL_EXCLUDE_LABELS}" ]]; then
    echo "[test] nightly full-suite extra excludes: ${NIGHTLY_FULL_EXCLUDE_LABELS}"
    export SLM_TEST_FULL_EXCLUDE_LABELS="${NIGHTLY_FULL_EXCLUDE_LABELS}"
  fi
  run_default_suite "${BUILD_DIR}"
  POLKIT_MODE="${SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE:-auto}"
  case "${POLKIT_MODE}" in
    required)
      echo "[test] running nightly polkit runtime smoke (required)"
      SLM_POLKIT_RUNTIME_SMOKE=1 \
      ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "^polkit_agent_runtime_smoke_test$"
      ;;
    auto)
      if systemctl --user --no-pager --full status slm-polkit-agent.service >/dev/null 2>&1; then
        echo "[test] running nightly polkit runtime smoke (auto:enabled)"
        SLM_POLKIT_RUNTIME_SMOKE=1 \
        ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "^polkit_agent_runtime_smoke_test$"
      else
        echo "[test] skipping nightly polkit runtime smoke (auto:no-user-service)"
      fi
      ;;
    skip)
      echo "[test] skipping nightly polkit runtime smoke (mode=skip)"
      ;;
    *)
      echo "[test] invalid SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE='${POLKIT_MODE}' (expected required|auto|skip)" >&2
      exit 2
      ;;
  esac
  PKG_POLICY_MODE="${SLM_TEST_NIGHTLY_PACKAGE_POLICY_WRAPPER_MODE:-auto}"
  case "${PKG_POLICY_MODE}" in
    required)
      echo "[test] running nightly package-policy wrapper runtime smoke (required)"
      SLM_PACKAGE_POLICY_RUNTIME_SMOKE=1 \
      ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "^packagepolicy_wrapper_runtime_smoke_test$"
      ;;
    auto)
      if command -v apt >/dev/null 2>&1 && command -v dpkg >/dev/null 2>&1; then
        apt_resolved=""
        apt_resolved="$(readlink -f "$(command -v apt)" 2>/dev/null || true)"
        if [[ "${apt_resolved}" == *"/slm-package-policy/wrappers/apt" ]]; then
          echo "[test] running nightly package-policy wrapper runtime smoke (auto:enabled)"
          SLM_PACKAGE_POLICY_RUNTIME_SMOKE=1 \
          ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "^packagepolicy_wrapper_runtime_smoke_test$"
        else
          echo "[test] skipping nightly package-policy wrapper runtime smoke (auto:wrapper-not-active)"
        fi
      else
        echo "[test] skipping nightly package-policy wrapper runtime smoke (auto:missing apt/dpkg)"
      fi
      ;;
    skip)
      echo "[test] skipping nightly package-policy wrapper runtime smoke (mode=skip)"
      ;;
    *)
      echo "[test] invalid SLM_TEST_NIGHTLY_PACKAGE_POLICY_WRAPPER_MODE='${PKG_POLICY_MODE}' (expected required|auto|skip)" >&2
      exit 2
      ;;
  esac
  SECRET_CONSENT_MODE="${SLM_TEST_NIGHTLY_SECRET_CONSENT_MODE:-auto}"
  SECRET_CONSENT_SKIP_BUILD="${SLM_TEST_NIGHTLY_SECRET_CONSENT_SKIP_BUILD:-1}"
  if [[ "${SECRET_CONSENT_SKIP_BUILD}" != "0" && "${SECRET_CONSENT_SKIP_BUILD}" != "1" ]]; then
    echo "[test] invalid SLM_TEST_NIGHTLY_SECRET_CONSENT_SKIP_BUILD='${SECRET_CONSENT_SKIP_BUILD}' (expected 0|1)" >&2
    exit 2
  fi
  case "${SECRET_CONSENT_MODE}" in
    required)
      echo "[test] running nightly secret-consent suite (required, skip_build=${SECRET_CONSENT_SKIP_BUILD})"
      SLM_SECRET_CONSENT_SKIP_BUILD="${SECRET_CONSENT_SKIP_BUILD}" \
      "${ROOT_DIR}/scripts/test-secret-consent-suite.sh" "${BUILD_DIR}"
      ;;
    auto)
      if [[ -x "${ROOT_DIR}/scripts/test-secret-consent-suite.sh" ]]; then
        echo "[test] running nightly secret-consent suite (auto:enabled, skip_build=${SECRET_CONSENT_SKIP_BUILD})"
        SLM_SECRET_CONSENT_SKIP_BUILD="${SECRET_CONSENT_SKIP_BUILD}" \
        "${ROOT_DIR}/scripts/test-secret-consent-suite.sh" "${BUILD_DIR}"
      else
        echo "[test] skipping nightly secret-consent suite (auto:runner-not-found)"
      fi
      ;;
    skip)
      echo "[test] skipping nightly secret-consent suite (mode=skip)"
      ;;
    *)
      echo "[test] invalid SLM_TEST_NIGHTLY_SECRET_CONSENT_MODE='${SECRET_CONSENT_MODE}' (expected required|auto|skip)" >&2
      exit 2
      ;;
  esac
  POLICY_CORE_MODE="${SLM_TEST_NIGHTLY_POLICY_CORE_MODE:-required}"
  POLICY_CORE_SKIP_BUILD="${SLM_TEST_NIGHTLY_POLICY_CORE_SKIP_BUILD:-1}"
  if [[ "${POLICY_CORE_SKIP_BUILD}" != "0" && "${POLICY_CORE_SKIP_BUILD}" != "1" ]]; then
    echo "[test] invalid SLM_TEST_NIGHTLY_POLICY_CORE_SKIP_BUILD='${POLICY_CORE_SKIP_BUILD}' (expected 0|1)" >&2
    exit 2
  fi
  case "${POLICY_CORE_MODE}" in
    required)
      echo "[test] running nightly policy-core suite (required, skip_build=${POLICY_CORE_SKIP_BUILD})"
      SLM_POLICY_CORE_SKIP_BUILD="${POLICY_CORE_SKIP_BUILD}" \
      run_policy_core_suite "${BUILD_DIR}"
      ;;
    auto)
      if ctest --test-dir "${BUILD_DIR}" -N -L policy-core 2>/dev/null | grep -q '^Total Tests: [1-9]'; then
        echo "[test] running nightly policy-core suite (auto:enabled, skip_build=${POLICY_CORE_SKIP_BUILD})"
        SLM_POLICY_CORE_SKIP_BUILD="${POLICY_CORE_SKIP_BUILD}" \
        run_policy_core_suite "${BUILD_DIR}"
      else
        echo "[test] skipping nightly policy-core suite (auto:no-policy-core-tests)"
      fi
      ;;
    skip)
      echo "[test] skipping nightly policy-core suite (mode=skip)"
      ;;
    *)
      echo "[test] invalid SLM_TEST_NIGHTLY_POLICY_CORE_MODE='${POLICY_CORE_MODE}' (expected required|auto|skip)" >&2
      exit 2
      ;;
  esac
  STORAGE_SMOKE_MODE="${SLM_TEST_NIGHTLY_STORAGE_RUNTIME_MODE:-skip}"
  STORAGE_SMOKE_SKIP_BUILD="${SLM_TEST_NIGHTLY_STORAGE_RUNTIME_SKIP_BUILD:-1}"
  if [[ "${STORAGE_SMOKE_SKIP_BUILD}" != "0" && "${STORAGE_SMOKE_SKIP_BUILD}" != "1" ]]; then
    echo "[test] invalid SLM_TEST_NIGHTLY_STORAGE_RUNTIME_SKIP_BUILD='${STORAGE_SMOKE_SKIP_BUILD}' (expected 0|1)" >&2
    exit 2
  fi
  case "${STORAGE_SMOKE_MODE}" in
    required)
      echo "[test] running nightly storage runtime smoke (required, skip_build=${STORAGE_SMOKE_SKIP_BUILD})"
      SLM_STORAGE_RUNTIME_HARDWARE_MODE=required \
      SLM_STORAGE_SMOKE_SKIP_BUILD="${STORAGE_SMOKE_SKIP_BUILD}" \
      run_storage_smoke_suite "${BUILD_DIR}"
      ;;
    auto)
      if [[ -x "${ROOT_DIR}/scripts/test-storage-runtime-suite.sh" ]]; then
        echo "[test] running nightly storage runtime smoke (auto:enabled, skip_build=${STORAGE_SMOKE_SKIP_BUILD})"
        SLM_STORAGE_RUNTIME_HARDWARE_MODE=optional \
        SLM_STORAGE_SMOKE_SKIP_BUILD="${STORAGE_SMOKE_SKIP_BUILD}" \
        run_storage_smoke_suite "${BUILD_DIR}"
      else
        echo "[test] skipping nightly storage runtime smoke (auto:runner-not-found)"
      fi
      ;;
    skip)
      echo "[test] skipping nightly storage runtime smoke (mode=skip)"
      ;;
    *)
      echo "[test] invalid SLM_TEST_NIGHTLY_STORAGE_RUNTIME_MODE='${STORAGE_SMOKE_MODE}' (expected required|auto|skip)" >&2
      exit 2
      ;;
  esac
  exit 0
fi

BUILD_DIR="${1:-${DEFAULT_BUILD_DIR}}"

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "Build directory not found: ${BUILD_DIR}" >&2
  echo "Usage: $0 [build_dir]" >&2
  exit 2
fi

run_default_suite "${BUILD_DIR}"
