#!/usr/bin/env bash
# Patch project(... VERSION ...) from a git tag (e.g. v0.2.1 → 0.2.1).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TAG="${1:-${GITHUB_REF_NAME:-}}"
VERSION="${TAG#v}"

if [[ -z "${VERSION}" ]]; then
  echo "Usage: set_version.sh <tag-or-version>" >&2
  exit 1
fi

# CMake project(VERSION) needs a dotted numeric version.
CMAKE_VERSION="$(echo "${VERSION}" | sed -E 's/^([0-9]+(\.[0-9]+){1,2}).*/\1/')"
if [[ ! "${CMAKE_VERSION}" =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
  echo "Unsupported version format: ${VERSION}" >&2
  exit 1
fi

CMAKE_FILE="${ROOT}/CMakeLists.txt"
if ! grep -qE '^\s*VERSION [0-9]' "${CMAKE_FILE}"; then
  echo "Could not find VERSION line in ${CMAKE_FILE}" >&2
  exit 1
fi

sed -i -E "s/^([[:space:]]*VERSION )[0-9]+(\.[0-9]+)*/\1${CMAKE_VERSION}/" "${CMAKE_FILE}"

echo "Set libgp_parser VERSION to ${CMAKE_VERSION} (from ${TAG})"
