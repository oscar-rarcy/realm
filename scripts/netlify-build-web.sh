#!/usr/bin/env bash
set -euo pipefail

BRANCH_NAME="${1:-${BRANCH:-edward}}"
if [[ "$BRANCH_NAME" != "edward" ]]; then
  echo "Only the edward branch web build is enabled in this pass. Requested: $BRANCH_NAME" >&2
  exit 1
fi

export REALM_INSTALL_EMSDK="${REALM_INSTALL_EMSDK:-1}"
exec bash scripts/build-web.sh
