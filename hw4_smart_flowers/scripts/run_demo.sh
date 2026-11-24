#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
config="${1:-tests/balanced.cfg}"
port="${2:-19014}"

if [[ ! -f "$config" ]]; then
    echo "Configuration not found: $config" >&2
    exit 1
fi

make -s all
flowers=$(sed -n 's/^flower_count=//p' "$config" | tr -d '[:space:]')
gardeners=$(sed -n 's/^gardener_count=//p' "$config" | tr -d '[:space:]')
if ! [[ "$flowers" =~ ^[0-9]+$ && "$gardeners" =~ ^[0-9]+$ ]] ||
   (( flowers < 1 || flowers > 100 || gardeners < 1 || gardeners > 10 )); then
    echo 'Invalid flower_count or gardener_count in configuration.' >&2
    exit 1
fi

pids=()
cleanup() {
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    for pid in "${pids[@]}"; do wait "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT INT TERM

./build/server 127.0.0.1 "$port" "$config" &
server_pid=$!
pids+=("$server_pid")
sleep 0.05
for ((i=1; i<=flowers; i++)); do
    ./build/flower 127.0.0.1 "$port" "$i" >/dev/null &
    pids+=("$!")
done
for ((i=1; i<=gardeners; i++)); do
    ./build/gardener 127.0.0.1 "$port" "$i" >/dev/null &
    pids+=("$!")
done
./build/observer 127.0.0.1 "$port" &
pids+=("$!")
wait "$server_pid"
