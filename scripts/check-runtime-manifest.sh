#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCES_FILE="${ROOT_DIR}/cmake/Sources.cmake"
CMAKE_FILE="${ROOT_DIR}/CMakeLists.txt"

echo "[runtime-manifest] root=${ROOT_DIR}"

if [[ ! -f "${SOURCES_FILE}" ]]; then
  echo "[runtime-manifest][WARN] missing ${SOURCES_FILE}"
  echo "[runtime-manifest] baseline runtime sources not committed yet"
  exit 0
fi

tmp_manifest="$(mktemp)"
tmp_missing="$(mktemp)"
cleanup() {
  rm -f "${tmp_manifest}" "${tmp_missing}"
}
trap cleanup EXIT

# Collect path-like entries from Sources.cmake list blocks.
grep -oE '[A-Za-z0-9_./-]+\.(qml|js|qsb|svg|png|jpg|jpeg|cpp|c|h|hpp)' "${SOURCES_FILE}" \
  | sort -u > "${tmp_manifest}" || true

# Add essential top-level runtime files.
printf '%s\n' "CMakeLists.txt" "Main.qml" "main.cpp" >> "${tmp_manifest}"
sort -u -o "${tmp_manifest}" "${tmp_manifest}"

total=0
missing=0
while IFS= read -r rel; do
  [[ -z "${rel}" ]] && continue
  # Ignore generated absolute artifacts listed in source manifests.
  [[ "${rel}" == /* ]] && continue
  total=$((total + 1))
  if [[ ! -f "${ROOT_DIR}/${rel}" ]]; then
    echo "${rel}" >> "${tmp_missing}"
    missing=$((missing + 1))
  fi
done < "${tmp_manifest}"

echo "[runtime-manifest] entries=${total} missing=${missing}"

if [[ "${missing}" -gt 0 ]]; then
  echo "[runtime-manifest][WARN] missing entries:"
  sed -n '1,120p' "${tmp_missing}"
  echo "[runtime-manifest] status=NOT_READY"
  exit 0
fi

if ! grep -q 'include(cmake/Sources.cmake)' "${CMAKE_FILE}"; then
  echo "[runtime-manifest][WARN] CMakeLists.txt does not include cmake/Sources.cmake"
  echo "[runtime-manifest] status=INCONSISTENT"
  exit 0
fi

echo "[runtime-manifest] status=READY"
