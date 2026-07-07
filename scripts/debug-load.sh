#!/usr/bin/env bash
# =============================================================================
#  debug-load.sh - Launch BricsCAD under gdb, auto-load open-electrical, and
#  capture a backtrace if it crashes.
#
#  BricsCAD's GUI process (`bricscad`) forks/execs the CAD engine (`cadtask`),
#  which is where a faulty BRX module crashes. gdb is therefore told NOT to
#  detach from forked children, so the SIGSEGV in `cadtask` is caught.
#
#  Output: /tmp/oe-crash.txt (full log). The run is time-bounded so it cannot
#  hang the desktop; a scratch drawing is opened so a document context exists
#  and the on_doc_load auto-loader fires.
# =============================================================================
set +e
MODULE="${1:-$HOME/.local/share/open-electrical/open-electrical.lrx}"
TIMEOUT="${TIMEOUT:-120}"
OUT="${OUT:-/tmp/oe-crash.txt}"
SUP="$HOME/Bricsys/BricsCAD/V26x64/en_US/Support"
BCROOT="/opt/bricsys/bricscad/v26"

[ -f "$MODULE" ] || { echo "module not found: $MODULE"; exit 1; }
mkdir -p "$SUP"

# Auto-loader: runs at startup and per document.
LISP="(princ \"\\n[OE-DEBUG] arxloading open-electrical...\\n\")(vl-catch-all-apply (function arxload) (list \"$MODULE\"))(princ \"\\n[OE-DEBUG] arxload returned (NO crash)\\n\")"
printf '%s\n' "$LISP" > "$SUP/on_start.lsp"
printf '%s\n' "$LISP" > "$SUP/on_doc_load.lsp"

# Scratch drawing so a real document opens.
SCRATCH=/tmp/oe_scratch.dwg
[ -f "$SCRATCH" ] || cp -f "$HOME/Bricsys/BricsCAD/V26x64/en_US/Templates/Default-m.dwt" "$SCRATCH" 2>/dev/null

# BricsCAD runtime environment (LD_LIBRARY_PATH, GDK_BACKEND=x11, ...).
source "$BCROOT/bricscad.sh" --env-only >/dev/null 2>&1

cat > /tmp/oe.gdb <<'GEOF'
set auto-load safe-path /
set pagination off
set confirm off
set detach-on-fork off
set schedule-multiple on
set follow-fork-mode parent
set follow-exec-mode same
handle SIG32 SIG33 SIG34 SIG35 SIGPWR SIGXCPU nostop noprint pass
handle SIGSEGV stop print
handle SIGABRT stop print
run
echo \n\n======== CRASH CAUGHT ========\n
echo ---- inferiors ----\n
info inferiors
echo \n---- backtrace (current) ----\n
bt
echo \n---- all threads (current inferior) ----\n
thread apply all bt
quit
GEOF

echo "[debug-load] launching under gdb (<= ${TIMEOUT}s) ..."
timeout --signal=KILL "$TIMEOUT" gdb -q -nx -batch -x /tmp/oe.gdb \
    --args "$BSYS_APP_UTIL_APP" "$SCRATCH" > "$OUT" 2>&1
echo "[debug-load] gdb wrapper exit: $?"

rm -f "$SUP/on_start.lsp" "$SUP/on_doc_load.lsp"
echo "[debug-load] captured $(wc -l < "$OUT") lines -> $OUT"
