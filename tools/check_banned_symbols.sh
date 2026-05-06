#!/bin/bash
# Fails if any banned symbol appears in the linked object/binary.
set -e

OBJ=${1:?usage: $0 <object-or-binary>}
BANNED='malloc|free|calloc|realloc|printf|fprintf|sprintf|fopen|exit|abort'
BANNED_FLOAT='__floatsi|__floatdi|__divdf|__muldf|__adddf|__subdf'

if nm --undefined-only "$OBJ" 2>/dev/null | grep -E "$BANNED|$BANNED_FLOAT"; then
  echo "FAIL: banned symbols referenced in $OBJ"
  exit 1
fi

echo "PASS: no banned symbols in $OBJ"

