#!/bin/bash

# Script to run clang-tidy or clang-format on Embedded C code
# Usage: ./run-checks.sh [tidy|format]

if [ $# -eq 0 ]; then
  echo "Usage: $0 [tidy|format]"
  echo "  tidy   - Run clang-tidy (analyzes code, gives warnings)"
  echo "  format - Run clang-format (formats code in-place)"
  exit 1
fi

MODE=$1

# Adjust this to your repo root if needed
cd /hil-rig-protocol || exit 1

SEARCH_DIRS=(
  "include"
  "src"
  "tests/c"
)

echo "Collecting source files in:"
for dir in "${SEARCH_DIRS[@]}"; do
  echo "  $dir/"
done

# Pick .c, .cpp, .h and .hpp files inside include/, src/ and tests/c/
FILES=$(find "${SEARCH_DIRS[@]}" -type f \( \
  -name "*.c" -o \
  -name "*.cpp" -o \
  -name "*.h" -o \
  -name "*.hpp" \
\) 2>/dev/null)

if [ -z "$FILES" ]; then
  echo "No C/C++ source or header files found."
  exit 0
fi

case "$MODE" in

  tidy)
    echo "Running clang-tidy on:"
    for file in $FILES; do
      echo "  $file"
      clang-tidy -p build "$file"
    done
    ;;

  format)
    echo "Running clang-format on:"
    for file in $FILES; do
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


## Run with commands:
# chmod +x run_checks.sh
# ./run_checks.sh tidy    # for clang-tidy
# ./run_checks.sh format  # for clang-format