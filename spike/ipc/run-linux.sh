#!/usr/bin/env bash
set -u

root="$(cd "$(dirname "$0")" && pwd)"
bun="$HOME/.bun/bin/bun"
server="$root/ipc_spike_server"

clang++ -std=c++17 -O2 "$root/server.cpp" -o "$server"
if [ ! -x "$server" ]; then
  echo "build failed" >&2
  exit 1
fi

run_case() {
  mode="$1"
  shift
  log="$root/server-$mode.log"
  err="$root/server-$mode.err.log"
  out="$root/client-$mode.json"
  cerr="$root/client-$mode.err.log"
  "$server" "$@" >"$log" 2>"$err" &
  pid=$!
  sleep 0.5
  "$bun" "$root/client.ts" "$@" >"$out" 2>"$cerr"
  code=$?
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  echo "MODE=$mode EXIT=$code"
  cat "$out"
  if [ -s "$cerr" ]; then
    echo "STDERR=$(cat "$cerr")"
  fi
}

run_case unix --mode unix --path /tmp/gda-ipc-spike.sock
run_case tcp --mode tcp --port 39528
