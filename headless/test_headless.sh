#!/bin/bash
#
# Integration tests for tn5250-headless
# Connects to pub400.com (public IBM i server) for live testing.
#
# Usage: ./test_headless.sh
#
# Requires: the headless binary to be built (run make first)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HEADLESS="$SCRIPT_DIR/.libs/tn5250-headless"
export LD_LIBRARY_PATH="$TOP_DIR/lib5250/.libs:$LD_LIBRARY_PATH"

PASS=0
FAIL=0
TOTAL=0

# Colors (if terminal supports it)
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[0;33m'
    NC='\033[0m'
else
    GREEN=''
    RED=''
    YELLOW=''
    NC=''
fi

assert_contains() {
    local desc="$1"
    local haystack="$2"
    local needle="$3"
    TOTAL=$((TOTAL + 1))
    if echo "$haystack" | grep -qF "$needle"; then
        PASS=$((PASS + 1))
        printf "${GREEN}  PASS${NC}: %s\n" "$desc"
    else
        FAIL=$((FAIL + 1))
        printf "${RED}  FAIL${NC}: %s\n" "$desc"
        printf "    Expected to contain: %s\n" "$needle"
        printf "    Got: %.200s\n" "$haystack"
    fi
}

assert_not_contains() {
    local desc="$1"
    local haystack="$2"
    local needle="$3"
    TOTAL=$((TOTAL + 1))
    if echo "$haystack" | grep -qF "$needle"; then
        FAIL=$((FAIL + 1))
        printf "${RED}  FAIL${NC}: %s\n" "$desc"
        printf "    Expected NOT to contain: %s\n" "$needle"
    else
        PASS=$((PASS + 1))
        printf "${GREEN}  PASS${NC}: %s\n" "$desc"
    fi
}

assert_equals() {
    local desc="$1"
    local actual="$2"
    local expected="$3"
    TOTAL=$((TOTAL + 1))
    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS + 1))
        printf "${GREEN}  PASS${NC}: %s\n" "$desc"
    else
        FAIL=$((FAIL + 1))
        printf "${RED}  FAIL${NC}: %s\n" "$desc"
        printf "    Expected: %s\n" "$expected"
        printf "    Got:      %s\n" "$actual"
    fi
}

# Helper: run headless with commands, capture output lines into an array
# Usage: run_headless "cmd1" "cmd2" ...
# Output stored in RESPONSES array
run_headless() {
    local input=""
    for cmd in "$@"; do
        input="${input}${cmd}\n"
    done
    input="${input}quit\n"

    local output
    output=$(printf "$input" | timeout 60 "$HEADLESS" 2>&1) || true
    # Split output into array by newlines
    IFS=$'\n' read -r -d '' -a RESPONSES <<< "$output" || true
}

# ============================================================
# Check binary exists
# ============================================================
if [ ! -x "$HEADLESS" ]; then
    printf "${RED}ERROR${NC}: Binary not found at %s\n" "$HEADLESS"
    printf "Run 'make' first to build.\n"
    exit 1
fi

# ============================================================
echo ""
echo "=== Test Suite: Offline Tests (no network) ==="
echo ""
# ============================================================

echo "--- --help flag ---"
HELP_OUT=$("$HEADLESS" --help 2>&1) || true
assert_contains "--help shows usage" "$HELP_OUT" "tn5250-headless"
assert_contains "--help lists connect command" "$HELP_OUT" "connect"
assert_contains "--help lists getscreen command" "$HELP_OUT" "getscreen"
assert_contains "--help lists quit command" "$HELP_OUT" "quit"

echo ""
echo "--- --version flag ---"
VERSION_OUT=$("$HEADLESS" --version 2>&1) || true
assert_contains "--version shows version" "$VERSION_OUT" "tn5250-headless"

echo ""
echo "--- quit command ---"
run_headless
assert_contains "quit returns ok" "${RESPONSES[0]}" '"status":"ok"'

echo ""
echo "--- commands before connect ---"
run_headless "getscreen" "sendkey enter" "type hello" "getfield 0 0" "movecursor 0 0" "waitfor test 1"
assert_contains "getscreen before connect gives error" "${RESPONSES[0]}" '"status":"error"'
assert_contains "getscreen error says not connected" "${RESPONSES[0]}" 'not connected'
assert_contains "sendkey before connect gives error" "${RESPONSES[1]}" 'not connected'
assert_contains "type before connect gives error" "${RESPONSES[2]}" 'not connected'
assert_contains "getfield before connect gives error" "${RESPONSES[3]}" 'not connected'
assert_contains "movecursor before connect gives error" "${RESPONSES[4]}" 'not connected'
assert_contains "waitfor before connect gives error" "${RESPONSES[5]}" 'not connected'

echo ""
echo "--- unknown command ---"
run_headless "boguscommand"
assert_contains "unknown command gives error" "${RESPONSES[0]}" '"status":"error"'
assert_contains "unknown command message" "${RESPONSES[0]}" 'unknown command'

echo ""
echo "--- missing arguments ---"
run_headless "connect" "sendkey" "getfield" "getfield 0" "movecursor" "movecursor 0"
assert_contains "connect without host gives error" "${RESPONSES[0]}" '"status":"error"'
assert_contains "sendkey without key gives error" "${RESPONSES[1]}" '"status":"error"'
assert_contains "getfield without args gives error" "${RESPONSES[2]}" '"status":"error"'
assert_contains "getfield with one arg gives error" "${RESPONSES[3]}" '"status":"error"'
assert_contains "movecursor without args gives error" "${RESPONSES[4]}" '"status":"error"'
assert_contains "movecursor with one arg gives error" "${RESPONSES[5]}" '"status":"error"'

echo ""
echo "--- connect to invalid host ---"
run_headless "connect invalid.host.that.does.not.exist.example"
assert_contains "connect to invalid host gives error" "${RESPONSES[0]}" '"status":"error"'

# ============================================================
echo ""
echo "=== Test Suite: Live Tests (pub400.com) ==="
echo ""
# ============================================================

# Check network connectivity first
echo "--- connectivity check ---"
if ! timeout 5 bash -c 'echo > /dev/tcp/pub400.com/23' 2>/dev/null; then
    printf "${YELLOW}  SKIP${NC}: Cannot reach pub400.com:23, skipping live tests\n"
    echo ""
    echo "============================================"
    printf "Results: ${GREEN}%d passed${NC}, ${RED}%d failed${NC}, %d total\n" "$PASS" "$FAIL" "$TOTAL"
    if [ "$FAIL" -gt 0 ]; then exit 1; fi
    exit 0
fi
printf "${GREEN}  OK${NC}: pub400.com:23 is reachable\n"

echo ""
echo "--- connect and getscreen ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "getscreen"
assert_contains "connect returns ok" "${RESPONSES[0]}" '"status":"ok"'
assert_contains "waitfor PUB400 succeeds" "${RESPONSES[1]}" '"status":"ok"'
assert_contains "getscreen returns ok" "${RESPONSES[2]}" '"status":"ok"'
assert_contains "screen has PUB400 content" "${RESPONSES[2]}" 'PUB400'
assert_contains "screen has Welcome text" "${RESPONSES[2]}" 'Welcome'
assert_contains "screen has IBM" "${RESPONSES[2]}" 'IBM'

echo ""
echo "--- getscreen json format ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "getscreen json"
assert_contains "json getscreen returns ok" "${RESPONSES[2]}" '"status":"ok"'
assert_contains "json has cursor field" "${RESPONSES[2]}" '"cursor"'
assert_contains "json has rows field" "${RESPONSES[2]}" '"rows":24'
assert_contains "json has cols field" "${RESPONSES[2]}" '"cols":80'
assert_contains "json has indicators" "${RESPONSES[2]}" '"indicators"'
assert_contains "json has inhibit indicator" "${RESPONSES[2]}" '"inhibit"'
assert_contains "json has insert indicator" "${RESPONSES[2]}" '"insert"'

echo ""
echo "--- getfield on sign-on screen ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "getfield 4 24" "getfield 5 24"
assert_contains "username field returns ok" "${RESPONSES[2]}" '"status":"ok"'
assert_contains "username field has row" "${RESPONSES[2]}" '"row":4'
assert_contains "username field has col" "${RESPONSES[2]}" '"col":24'
assert_contains "username field has length" "${RESPONSES[2]}" '"length"'
assert_contains "username field has type" "${RESPONSES[2]}" '"type"'
assert_contains "username field has bypass" "${RESPONSES[2]}" '"bypass":false'
assert_contains "password field returns ok" "${RESPONSES[3]}" '"status":"ok"'
assert_contains "password field has length 128" "${RESPONSES[3]}" '"length":128'

echo ""
echo "--- getfield on non-field position ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "getfield 0 0"
assert_contains "getfield on non-field returns error" "${RESPONSES[2]}" '"status":"error"'
assert_contains "getfield error says no field" "${RESPONSES[2]}" 'no field'

echo ""
echo "--- type text ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "type TESTUSER" "getscreen"
assert_contains "type returns ok" "${RESPONSES[2]}" '"status":"ok"'
# The typed text should appear on screen
assert_contains "typed text appears on screen" "${RESPONSES[3]}" 'TESTUSER'

echo ""
echo "--- movecursor ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "movecursor 10 5" "getscreen json"
assert_contains "movecursor returns ok" "${RESPONSES[2]}" '"status":"ok"'
assert_contains "cursor moved to row 10" "${RESPONSES[3]}" '"cursor":[10,5]'

echo ""
echo "--- sendkey ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "sendkey enter"
assert_contains "sendkey enter returns ok" "${RESPONSES[2]}" '"status":"ok"'

echo ""
echo "--- waitfor timeout ---"
run_headless "connect pub400.com" "waitfor NONEXISTENTTEXT12345 3"
assert_contains "waitfor nonexistent text times out" "${RESPONSES[1]}" '"status":"error"'
assert_contains "waitfor timeout message" "${RESPONSES[1]}" 'timeout'

echo ""
echo "--- double connect ---"
run_headless "connect pub400.com" "waitfor PUB400 30" "connect pub400.com"
assert_contains "double connect gives error" "${RESPONSES[2]}" '"status":"error"'
assert_contains "double connect says already connected" "${RESPONSES[2]}" 'already connected'

echo ""
echo "--- empty and whitespace lines ---"
run_headless "" "   " "connect pub400.com" "waitfor PUB400 30" "getscreen"
# Empty/whitespace lines should be silently ignored, connect should be RESPONSES[0]
assert_contains "connect after empty lines succeeds" "${RESPONSES[0]}" '"status":"ok"'

# ============================================================
echo ""
echo "============================================"
printf "Results: ${GREEN}%d passed${NC}" "$PASS"
if [ "$FAIL" -gt 0 ]; then
    printf ", ${RED}%d failed${NC}" "$FAIL"
else
    printf ", %d failed" "$FAIL"
fi
printf ", %d total\n" "$TOTAL"
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
