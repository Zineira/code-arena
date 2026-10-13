#!/usr/bin/env sh
# Compiles and runs the engine-independent core tests (no Unreal needed).
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CORE="$DIR/../Source/CodeArena/Private/Core"
OUT="${TMPDIR:-/tmp}/codearena_tests"
${CXX:-c++} -std=c++17 -O1 -Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti \
  -I"$CORE" "$DIR/CoreTests.cpp" "$CORE/FightScript.cpp" "$CORE/FightSim.cpp" "$CORE/FightLevels.cpp" -o "$OUT"
"$OUT"
