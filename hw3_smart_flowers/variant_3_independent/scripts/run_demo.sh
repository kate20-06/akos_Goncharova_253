#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
config="${1:-tests/balanced.cfg}"
if [[ ! -f "$config" ]]; then echo "Config not found: $config" >&2; exit 1; fi
flowers=$(sed -n 's/^flower_count[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$config" | head -1)
gardeners=$(sed -n 's/^gardener_count[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$config" | head -1)
if [[ -z "$flowers" || -z "$gardeners" ]]; then echo "Missing process counts" >&2; exit 1; fi
session="g${$}"
pids=()
cleanup() {
    trap - INT TERM EXIT
    if [[ ${#pids[@]} -gt 0 ]]; then
        kill -INT "${pids[0]}" 2>/dev/null || true
        sleep 0.25
        kill "${pids[@]}" 2>/dev/null || true
        wait "${pids[@]}" 2>/dev/null || true
    fi
}
trap cleanup INT TERM EXIT
./build/garden "$config" "$session" & pids+=("$!")
sleep 0.1
for ((i=1; i<=flowers; i++)); do ./build/flower "$session" "$i" & pids+=("$!"); done
for ((i=1; i<=gardeners; i++)); do ./build/gardener "$session" "$i" & pids+=("$!"); done
result=0
wait "${pids[0]}" || result=1
for ((i=1; i<${#pids[@]}; i++)); do wait "${pids[i]}" || result=1; done
trap - INT TERM EXIT
exit "$result"
