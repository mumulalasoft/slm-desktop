#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/toppanel-Debug}"
FAILED=0

note() { printf '[filemanager-gate] %s\n' "$*"; }
fail() { printf '[filemanager-gate][FAIL] %s\n' "$*" >&2; FAILED=1; }
pass() { printf '[filemanager-gate][OK] %s\n' "$*"; }

note "checking FileManager extraction readiness..."

# 1) Canonical QML path must be apps/filemanager.
if rg -n 'Qml/components/filemanager' \
    Main.qml Qml src tests \
    --glob '!build/**' \
    --glob '!.git/**' \
    --glob '!Qml/components/filemanager/**' \
    >/dev/null 2>&1; then
    fail "legacy QML path 'Qml/components/filemanager' referenced outside compatibility shim boundary."
else
    pass "no legacy QML path references outside compatibility shim boundary."
fi

# 2) Public header candidates must exist.
for header in \
    src/apps/filemanager/include/filemanagerapi.h \
    src/apps/filemanager/include/filemanagermodel.h \
    src/apps/filemanager/include/filemanagermodelfactory.h
do
    if [[ -f "$header" ]]; then
        pass "found $header"
    else
        fail "missing $header"
    fi
done

# 3) FileManager tests are grouped under tests/filemanager.
for test_src in \
    tests/filemanager/fileoperationsmanager_test.cpp \
    tests/filemanager/fileoperationsservice_dbus_test.cpp \
    tests/filemanager/fileopctl_smoke_test.cpp \
    tests/filemanager/filemanagerapi_daemon_recovery_test.cpp \
    tests/filemanager/filemanagerapi_fileops_contract_test.cpp \
    tests/filemanager/filemanager_dialogs_smoke_test.cpp
do
    if [[ -f "$test_src" ]]; then
        pass "found $test_src"
    else
        fail "missing $test_src"
    fi
done

# 4) Standalone/profile targets must be present in CMake graph.
if [[ -d "$BUILD_DIR" ]]; then
    _target_help_output="$(cmake --build "$BUILD_DIR" --target help 2>/dev/null || true)"
    if [[ "$_target_help_output" == *"slm-filemanager-core"* ]]; then
        pass "target slm-filemanager-core present"
    else
        fail "target slm-filemanager-core not found in $BUILD_DIR"
    fi
    if [[ "$_target_help_output" == *"filemanager_test_suite"* ]]; then
        pass "target filemanager_test_suite present"
    else
        fail "target filemanager_test_suite not found in $BUILD_DIR"
    fi
else
    note "build directory '$BUILD_DIR' not found, skipping target graph checks."
fi

# 5) Standalone module must include QObject headers in target sources for AUTOMOC.
if rg -n 'set\(SLM_FILEMANAGER_CORE_HEADERS' modules/slm-filemanager/cmake/Sources.cmake >/dev/null 2>&1; then
    pass "standalone source manifest exposes SLM_FILEMANAGER_CORE_HEADERS"
else
    fail "missing SLM_FILEMANAGER_CORE_HEADERS in modules/slm-filemanager/cmake/Sources.cmake"
fi

for qobj_header in \
    'filemanagerapi.h' \
    'filemanagermodel.h' \
    'filemanagermodelfactory.h'
do
    if rg -n "$qobj_header" modules/slm-filemanager/cmake/Sources.cmake >/dev/null 2>&1; then
        pass "standalone header list contains $qobj_header"
    else
        fail "standalone header list missing $qobj_header"
    fi
done

if rg -n '\$\{SLM_FILEMANAGER_CORE_HEADERS\}' modules/slm-filemanager/CMakeLists.txt >/dev/null 2>&1; then
    pass "slm-filemanager-core target includes SLM_FILEMANAGER_CORE_HEADERS"
else
    fail "slm-filemanager-core target does not include SLM_FILEMANAGER_CORE_HEADERS"
fi

if [[ "$FAILED" -ne 0 ]]; then
    note "extraction gate failed."
    exit 1
fi

note "extraction gate passed."
