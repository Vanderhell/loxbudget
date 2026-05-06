#!/bin/bash
set -e

PROFILE=${1:-tiny}
YAML=ci/footprint_budget.yaml
OBJ=${2:-build/loxbudget_arm.o}

TEXT_MAX=$(awk -v p="${PROFILE}:" '$1==p{f=1} f&&$1=="text_max:"{print $2; exit}' "$YAML")
BSS_MAX=$(awk -v p="${PROFILE}:" '$1==p{f=1} f&&$1=="bss_max:"{print $2; exit}' "$YAML")

if [ -z "$TEXT_MAX" ] || [ -z "$BSS_MAX" ]; then
  echo "FAIL: missing budget for profile $PROFILE in $YAML"
  exit 2
fi

if [ ! -f "$OBJ" ]; then
  echo "FAIL: missing object/binary: $OBJ"
  exit 2
fi

SIZE_OUT=$(arm-none-eabi-size "$OBJ" | tail -n 1)
TEXT=$(echo "$SIZE_OUT" | awk '{print $1}')
DATA=$(echo "$SIZE_OUT" | awk '{print $2}')
BSS=$(echo "$SIZE_OUT" | awk '{print $3}')

if [ "$BSS" -gt "$BSS_MAX" ]; then
  echo "FAIL: bss $BSS > $BSS_MAX ($OBJ)"
  exit 1
fi

if [ "$TEXT" -gt "$TEXT_MAX" ]; then
  echo "FAIL: text $TEXT > $TEXT_MAX ($OBJ)"
  exit 1
fi

echo "PASS: $PROFILE footprint ok (text=$TEXT, bss=$BSS)"
