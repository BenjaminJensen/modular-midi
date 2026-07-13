#!/usr/bin/env bash
# Runs clang-tidy against only the lines changed relative to $1 (default:
# origin/main), using clang-tidy-diff.py. New/changed code must be clean;
# pre-existing findings elsewhere are not flagged (see CLAUDE.md's Linting
# section for why - the check set was deliberately kept lean and a repo-wide
# cleanup pass was explicitly deferred).
#
# Run from the repo root, inside the docker/Dockerfile image, after both
# build/ and build-tests/ have been configured (their compile_commands.json
# are what clang-tidy uses to find compile flags for changed files).
set -euo pipefail

BASE_REF="${1:-origin/main}"
SRC_BUILD_DIR="${2:-build}"
TESTS_BUILD_DIR="${3:-build-tests}"
CTD="/usr/lib/llvm-18/share/clang/clang-tidy-diff.py"
STATUS=0

TOOLCHAIN_SYSROOT="/opt/arm-gnu-toolchain/arm-none-eabi"
CXX_INC="$TOOLCHAIN_SYSROOT/include/c++/14.3.1"
CXX_TARGET_INC="$CXX_INC/arm-none-eabi/thumb/v8-m.main+fp/hard"

echo "Diffing against $BASE_REF"

echo "== src/shared, src/projects (firmware: pico-sdk/FreeRTOS) =="
git diff -U0 "$BASE_REF"...HEAD | python3 "$CTD" -p1 \
    -iregex '^src/(shared|projects)/.*\.(cpp|h|hpp)$' \
    -path "$SRC_BUILD_DIR" \
    -extra-arg=--target=arm-none-eabi \
    -extra-arg="--sysroot=$TOOLCHAIN_SYSROOT" \
    -extra-arg="-isystem$CXX_INC" \
    -extra-arg="-isystem$CXX_TARGET_INC" \
    -quiet | tee /tmp/lint-src.txt
if grep -qE "warning:|error:" /tmp/lint-src.txt; then STATUS=1; fi

echo "== tests/ (host-testable) =="
git diff -U0 "$BASE_REF"...HEAD | python3 "$CTD" -p1 \
    -iregex '^tests/.*\.(cpp|h|hpp)$' \
    -path "$TESTS_BUILD_DIR" \
    -quiet | tee /tmp/lint-tests.txt
if grep -qE "warning:|error:" /tmp/lint-tests.txt; then STATUS=1; fi

exit $STATUS
