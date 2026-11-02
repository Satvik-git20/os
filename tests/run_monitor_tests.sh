#!/usr/bin/env bash
# run_monitor_tests.sh - smoke tests for stms_monitor.sh (Module 1).
#
# Builds fake services, runs the monitor against them, and records PASS/FAIL.
# Runs on any bash+coreutils (git-bash / Linux / macOS).
set -u

MONITOR_SRC="${MONITOR_SRC:-/c/Users/satvi/Desktop/STMS/src/stms_monitor.sh}"
RESULTS="${RESULTS:-/c/Users/satvi/Desktop/STMS/evidence/monitor_tests.log}"
mkdir -p "$(dirname "$RESULTS")"

 PASS=0
 FAIL=0
log(){ echo "$1" | tee -a "$RESULTS"; }
note(){ echo "  -> $1" | tee -a "$RESULTS"; }

record(){
    local name="$1" expected="$2" got="$3"
    if [ "$expected" = "$got" ]; then PASS=$((PASS+1)); log "PASS: $name"; else FAIL=$((FAIL+1)); log "FAIL: $name (expected [$expected], got [$got])"; fi
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
STAGE="$tmp/stage"
mkdir -p "$STAGE/logs"
cp "$MONITOR_SRC" "$STAGE/stms_monitor.sh"
chmod +x "$STAGE/stms_monitor.sh"

# Stealth cleanup: the test spawns background "bash ./<service>" processes that
# leak across runs (re-parented to init). Kill them before the test starts and
# on exit so every run is clean and reproducible.
cleanup() {
    # Kill every test-spawned service robustly (this git-bash's `ps` has no -o
    # and cut -d' ' eats multi-space padding, so use ps -ef + awk).
    ps -ef | grep -E '\./(sensor_reader|signal_ctrl|logger)' | grep -v grep | awk '{print $2}' | while read -r p; do
        kill "$p" 2>/dev/null
    done
}
trap 'cleanup' EXIT

# Fake services: an alive bash script whose cmdline is "./<name>" so that the
# monitor's svc_running (pgrep -f "./name" / ps fallback) can see it.
for svc in sensor_reader signal_ctrl logger; do
    printf '#!/usr/bin/env bash\nwhile :; do sleep 1; done\n' > "$STAGE/$svc"
    chmod +x "$STAGE/$svc"
done

run_monitor(){
    ( cd "$STAGE" && LOG_DIR="$1" bash ./stms_monitor.sh )
}

: > "$RESULTS"

log "=== stms_monitor.sh (Module 1) smoke tests ==="
log "monitor: $MONITOR_SRC"
log "results: $RESULTS"
log "workdir: $STAGE"

# --- Case 1: all services running -> all report OK, nothing restarted -----------
(
    cd "$STAGE" || exit 1
    for svc in sensor_reader signal_ctrl logger; do
        ./"$svc" &
    done
    sleep 0.4
)
run_monitor "$tmp/logs"
for svc in sensor_reader signal_ctrl logger; do
    c=$(grep -c " $svc" "$tmp/logs/health.log")
    record "case1-$svc-ok-count" 1 "$c"
done
# nothing should have been restarted (no DOWN lines)
down=$(grep -c "DOWN" "$tmp/logs/health.log")
record "case1-no-down" 0 "$down"

# --- Case 2: one service stopped -> reported DOWN and restarted -----------------
(
    cd "$STAGE" || exit 1
    sleep 0.2
    # Kill the first ./signal_ctrl process we can see (no pgrep on this host).
    local_pid=$(ps -f | grep '\./signal_ctrl' | head -n1 | cut -d' ' -f2)
    if [ -n "$local_pid" ]; then
        kill "$local_pid" 2>/dev/null
        sleep 0.3
    fi
)
run_monitor "$tmp/logs2"
alive=$(ps -f | grep '\./signal_ctrl' | grep -v grep | wc -l)
record "case2-signal_ctrl-alive-after-restart" 1 "$alive"

# --- Case 3: log rotation (> MAX_KB) -------------------------------------------
mkdir -p "$tmp/logs3"
dd if=/dev/urandom of="$tmp/logs3/big.log" bs=1024 count=612 status=none
(
    cd "$STAGE" || exit 1
    LOG_DIR="$tmp/logs3" bash ./stms_monitor.sh
)
ret=$?
record "case3-rotation-exit" 0 "$ret"
gz=$(ls "$tmp/logs3" | grep -c '\.log\.[0-9]\{8\}\.gz$')
record "case3-rotated-gz" 1 "$gz"

# --- Case 4: empty glob does not error ----------------------------------------
(
    cd "$STAGE" || exit 1
    LOG_DIR="$tmp/logs4" bash ./stms_monitor.sh >/dev/null 2>&1
)
ret=$?
record "case4-empty-glob-exit" 0 "$ret"

# --- Case 5: idempotent re-run (second run also exits 0) -----------------------
(
    cd "$STAGE" || exit 1
    LOG_DIR="$tmp/logs2" bash ./stms_monitor.sh >/dev/null 2>&1
)
ret=$?
record "case5-idempotent-exit" 0 "$ret"

# --- Summary -------------------------------------------------------------------
log ""
log "SUMMARY: PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -eq 0 ]; then
    log "ALL MODULE-1 TESTS PASSED"
    exit 0
else
    log "SOME MODULE-1 TESTS FAILED"
    exit 1
fi
