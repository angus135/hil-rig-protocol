#!/bin/bash

# Script to run clang-tidy or clang-format on HIL-RIG protocol C/C++ code.
#
# Usage:
#   ./run_checks.sh tidy
#   ./run_checks.sh format
#
# Checked directories:
#   include/
#   src/
#   tests/c/
#   examples/c/

set -e

if [ $# -eq 0 ]; then
  echo "Usage: $0 [tidy|format]"
  echo "  tidy   - Run clang-tidy on C/C++ source and header files"
  echo "  format - Run clang-format in-place"
  exit 1
fi

MODE=$1

# Move to repository root.
# This assumes the script is located in the repository root.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

SEARCH_DIRS=(
  "include"
  "src"
  "tests/c"
  "examples/c"
)

EXISTING_SEARCH_DIRS=()

for dir in "${SEARCH_DIRS[@]}"; do
  if [ -d "$dir" ]; then
    EXISTING_SEARCH_DIRS+=("$dir")
  fi
done

if [ ${#EXISTING_SEARCH_DIRS[@]} -eq 0 ]; then
  echo "No search directories exist."
  exit 0
fi

echo "Collecting source files in:"
for dir in "${EXISTING_SEARCH_DIRS[@]}"; do
  echo "  $dir/"
done

mapfile -d '' FILES < <(
  find "${EXISTING_SEARCH_DIRS[@]}" -type f \( \
    -name "*.c"   -o \
    -name "*.cc"  -o \
    -name "*.cpp" -o \
    -name "*.cxx" -o \
    -name "*.h"   -o \
    -name "*.hh"  -o \
    -name "*.hpp" -o \
    -name "*.hxx" \
  \) -print0
)

if [ ${#FILES[@]} -eq 0 ]; then
  echo "No C/C++ source or header files found."
  exit 0
fi

case "$MODE" in

  tidy)
    if [ ! -d "build" ]; then
      echo "Error: build/ directory not found."
      echo "Run CMake first so clang-tidy can use build/compile_commands.json:"
      echo "  cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
      exit 1
    fi

    if [ ! -f "build/compile_commands.json" ]; then
      echo "Error: build/compile_commands.json not found."
      echo "Reconfigure with:"
      echo "  cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
      exit 1
    fi

    echo "Running clang-tidy on:"
    for file in "${FILES[@]}"; do
      echo "  $file"
      clang-tidy -p build "$file"
    done
    ;;

  format)
    echo "Running clang-format on:"
    for file in "${FILES[@]}"; do
      echo "  $file"
      clang-format -i "$file"
    done
    ;;

  *)
    echo "Error: Unknown mode '$MODE'"
    echo "Usage: $0 [tidy|format]"
    exit 1
    ;;

esac

echo "Done!"