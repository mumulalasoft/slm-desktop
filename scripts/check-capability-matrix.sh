#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CAP_SRC="${ROOT_DIR}/src/core/permissions/Capability.cpp"
CAP_DOC="${ROOT_DIR}/docs/security/CAPABILITY_MATRIX.md"

if [[ ! -f "${CAP_SRC}" ]]; then
  echo "[cap-matrix] source not found: ${CAP_SRC}" >&2
  exit 2
fi

if [[ ! -f "${CAP_DOC}" ]]; then
  echo "[cap-matrix] doc not found: ${CAP_DOC}" >&2
  exit 2
fi

src_list="$(mktemp)"
doc_list="$(mktemp)"
cleanup() {
  rm -f "${src_list}" "${doc_list}"
}
trap cleanup EXIT

grep -oP 'QStringLiteral\("\K[^"]+' "${CAP_SRC}" \
  | grep -v '^Unknown$' \
  | sort -u > "${src_list}"

grep -oP '`[A-Za-z]+\.[A-Za-z]+`' "${CAP_DOC}" \
  | tr -d '`' \
  | sort -u > "${doc_list}"

missing_in_doc="$(comm -23 "${src_list}" "${doc_list}" || true)"
extra_in_doc="$(comm -13 "${src_list}" "${doc_list}" || true)"

if [[ -n "${missing_in_doc}" ]]; then
  echo "[cap-matrix] Missing capabilities in docs/security/CAPABILITY_MATRIX.md:" >&2
  echo "${missing_in_doc}" >&2
fi

if [[ -n "${extra_in_doc}" ]]; then
  echo "[cap-matrix] Extra/unknown capabilities in docs/security/CAPABILITY_MATRIX.md:" >&2
  echo "${extra_in_doc}" >&2
fi

if [[ -n "${missing_in_doc}" || -n "${extra_in_doc}" ]]; then
  exit 1
fi

count="$(wc -l < "${src_list}" | tr -d '[:space:]')"
echo "[cap-matrix] OK (${count} capabilities synced)"

