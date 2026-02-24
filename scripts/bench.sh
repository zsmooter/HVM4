#!/usr/bin/env bash
# scripts/bench.sh
# ================
# Benchmark runner for HVM4.
# Clones/caches the bench repo, builds the C binary, and runs
# benchmarks across multiple thread counts in a table format.

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CACHE_DIR="$ROOT_DIR/.cache/bench"
BENCH_DIR="$CACHE_DIR/bench"
MAIN="$ROOT_DIR/clang/main"
BENCH_REPO="https://github.com/HigherOrderCO/bench.git"
TIMEOUT=${TIMEOUT:-10}
THREADS=(1 2 4 8 12)
SHOW_IPS=0
HVM_TMP_DIR="${TMPDIR:-/tmp}"
if [ -n "${HVM_TMPDIR:-}" ]; then
  HVM_TMP_DIR="$HVM_TMPDIR"
fi
if ! mkdir -p "$HVM_TMP_DIR"; then
  echo "error: failed to create temp directory '$HVM_TMP_DIR'" >&2
  exit 1
fi

# Help
# ----

# Prints usage and exits
show_help() {
  echo "Usage: scripts/bench.sh [--interpreted | --compiled] [--ips] [-TN]"
  echo ""
  echo "  --interpreted  Run benchmarks via the C interpreter"
  echo "  --compiled     Run benchmarks via AOT compilation (--as-c)"
  echo "  --ips          Show interactions/s instead of time"
  echo "  -TN            Run only with N threads (example: -T1)"
  echo ""
  echo "The bench repo is cloned/cached at .cache/bench/."
  exit 0
}

# Args
# ----

# Parse mode from flags
MODE=""
for arg in "$@"; do
  case "$arg" in
    --interpreted ) MODE="interpreted" ;;
    --compiled    ) MODE="compiled" ;;
    --ips         ) SHOW_IPS=1 ;;
    -T[0-9]*      )
      n="${arg#-T}"
      if ! [[ "$n" =~ ^[1-9][0-9]*$ ]]; then
        echo "error: invalid thread count on '$arg'" >&2
        show_help
      fi
      THREADS=("$n")
      ;;
    --help | -h   ) show_help ;;
    * )
      echo "error: unknown flag '$arg'" >&2
      show_help
      ;;
  esac
done

if [ -z "$MODE" ]; then
  show_help
fi

# Timeout
# -------

# Runs a command with a timeout; stores output in the named variable
run_with_timeout() {
  local out_var="$1" t="$2" marker cap_file pid status
  shift 2

  marker="${HVM_TMP_DIR}/hvm4-bench-runner.timeout.marker"
  cap_file="${HVM_TMP_DIR}/hvm4-bench-runner.capture.log"

  rm -f "$marker"
  : > "$cap_file" || return 1
  "$@" >"$cap_file" 2>&1 & pid=$!
  ( sleep "$t"; kill -0 "$pid" 2>/dev/null || exit 0; : > "$marker"; kill -KILL "$pid" 2>/dev/null || true ) &
  wait "$pid" 2>/dev/null; status=$?
  [ -f "$marker" ] && status=124
  printf -v "$out_var" '%s' "$(cat "$cap_file")"
  rm -f "$cap_file" "$marker"
  return $status
}

# Cache
# -----

# Clones or updates the bench repo
sync_bench_repo() {
  if [ -d "$CACHE_DIR/.git" ]; then
    echo "Updating bench repo..."
    (cd "$CACHE_DIR" && git pull --quiet)
  else
    echo "Cloning bench repo..."
    mkdir -p "$(dirname "$CACHE_DIR")"
    git clone --quiet "$BENCH_REPO" "$CACHE_DIR"
  fi
}

# Build
# -----

# Compiles the C binary
build_main() {
  if [ ! -f "$MAIN.c" ]; then
    echo "error: expected C entrypoint at $MAIN.c" >&2
    exit 1
  fi
  echo "Building clang/main..."
  (cd "$ROOT_DIR/clang" && clang -O2 -o main main.c)
}

# Bench
# -----

# Extracts the benchmark metric from stats output
get_metric() {
  local out="$1" line num unit

  if [ "$SHOW_IPS" -eq 1 ]; then
    line=$(printf '%s\n' "$out" | awk -F': ' '/^- Perf:/{print $2; exit}')
    if [ -z "$line" ]; then
      echo "n/a"
      return
    fi
    num=$(printf '%s\n' "$line" | awk '{print $1}')
    unit=$(printf '%s\n' "$line" | awk '{print $2}')
    if [ -z "$num" ]; then
      echo "n/a"
    elif [ -z "$unit" ]; then
      echo "$num"
    else
      echo "${num}${unit}"
    fi
    return
  fi

  line=$(printf '%s\n' "$out" | awk -F': ' '/^- Time:/{print $2; exit}')
  if [ -z "$line" ]; then
    echo "n/a"
    return
  fi
  num=$(printf '%s\n' "$line" | awk '{print $1}')
  if [ -z "$num" ]; then
    echo "n/a"
  else
    echo "${num}s"
  fi
}

# Runs all benchmarks and prints a results table
run_benchmarks() {
  # Collect bench files: .cache/bench/bench/*/main.hvm
  shopt -s nullglob
  local bench_files=("$BENCH_DIR"/*/main.hvm)
  shopt -u nullglob

  if [ ${#bench_files[@]} -eq 0 ]; then
    echo "error: no benchmarks found under $BENCH_DIR/*/main.hvm" >&2
    exit 1
  fi

  IFS=$'\n' bench_files=($(printf '%s\n' "${bench_files[@]}" | sort))
  unset IFS

  # Compute column width from bench names
  local name_w=4
  for file in "${bench_files[@]}"; do
    local name
    name="$(basename "$(dirname "$file")")"
    local len=${#name}
    if [ $len -gt $name_w ]; then
      name_w=$len
    fi
  done
  name_w=$((name_w + 2))

  # Print header
  printf "%-*s" "$name_w" "bench"
  for t in "${THREADS[@]}"; do
    printf "%8s" "T$t"
  done
  echo

  # Build mode-specific flags
  local mode_flags=()
  if [ "$MODE" = "compiled" ]; then
    mode_flags+=("--as-c")
  fi

  # Run each benchmark across thread counts
  for file in "${bench_files[@]}"; do
    local name
    name="$(basename "$(dirname "$file")")"
    printf "%-*s" "$name_w" "$name"

    for t in "${THREADS[@]}"; do
      local extra_args=()
      case "$name" in
        gen_*      ) extra_args+=("-C1") ;;
        collapse_* ) extra_args+=("-C") ;;
      esac

      run_with_timeout out "$TIMEOUT" \
        "$MAIN" "$file" -s -S "-T$t" \
        "${mode_flags[@]+"${mode_flags[@]}"}" \
        "${extra_args[@]+"${extra_args[@]}"}"
      local status=$?

      local val
      if [ $status -eq 124 ]; then
        val="timeout"
      elif [ $status -ne 0 ]; then
        val="error"
      else
        val="$(get_metric "$out")"
      fi
      printf "%8s" "$val"
    done
    echo
  done
}

# Main
# ----

sync_bench_repo
build_main
run_benchmarks
