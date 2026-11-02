#!/usr/bin/env bash
# stms_monitor.sh - health-check, auto-restart and log rotation for STMS services.
# Module 1 of the Smart Traffic Management System PBL report.
#
# Usage:
#   ./stms_monitor.sh            # run once
#   ./stms_monitor.sh --once     # same as above
#   ./stms_monitor.sh --daemon   # run under cron / as a background supervisor
#
# Services: sensor_reader, signal_ctrl, logger
# Logs:    ./logs/health.log, ${name}.log, rotated + gzipped logs
#
# Re-runnable and safe to invoke while a previous run is in flight:
#   - pgrep guard before restarting
#   - file-exists guard before rotating
set -u

LOG_DIR="${LOG_DIR:-./logs}"
MAX_KB="${MAX_KB:-512}"
SERVICES=(sensor_reader signal_ctrl logger)

usage() {
    echo "Usage: $0 [--once | --daemon]" >&2
}

if [ "${1:-}" = "--help" ]; then
    usage
    exit 0
fi

mkdir -p "$LOG_DIR" || { echo "FATAL: cannot create $LOG_DIR" >&2; exit 1; }

# Return 0 (true) if a service named $1 is running.
#   1. prefer `pgrep -x` (exact process name)
#   2. also match command lines like "./sensor_reader" (scripts launched via
#      their own filename)
#   3. fallback to reading /proc/[pid]/cmdline
#   4. last resort: `ps -e -o args`
svc_running() {
    local name="$1" p
    if command -v pgrep >/dev/null 2>&1; then
        pgrep -x "$name" >/dev/null 2>&1 && return 0
        pgrep -f "./$name" >/dev/null 2>&1 && return 0
        return 1
    fi
    if [ -d /proc ]; then
        for p in /proc/[0-9]*; do
            if [ -r "$p/cmdline" ]; then
                if tr '\0' ' ' < "$p/cmdline" 2>/dev/null | grep -q "\./$name"; then
                    return 0
                fi
            fi
        done
        return 1
    fi
    ps -e -o args= 2>/dev/null | grep -q "\./$name"
}

check_service() {
    local name="$1"

    if svc_running "$name"; then
        echo "$(date '+%F %T') OK   $name" >>"$LOG_DIR/health.log"
    else
        echo "$(date '+%F %T') DOWN $name - restarting" >>"$LOG_DIR/health.log"
        # Start the service detached; keep its stdout/stderr in its own log.
        nohup "./$name" >>"$LOG_DIR/$name.log" 2>&1 &
    fi
}

for s in "${SERVICES[@]}"; do
    check_service "$s"
done

# Log rotation: gzip any log that exceeds MAX_KB.
for f in "$LOG_DIR"/*.log; do
    [ -f "$f" ] || continue
    size=$(du -k "$f" | cut -f1)
    if [ "$size" -gt "$MAX_KB" ]; then
        local_base=$(basename "$f")
        rotated="$LOG_DIR/${local_base}.$(date +%Y%m%d)"
        mv "$f" "$rotated" && gzip "$rotated"
        echo "$(date '+%F %T') ROT  $local_base -> $rotated.gz (size=${size}KB)" >>"$LOG_DIR/health.log"
    fi
done
