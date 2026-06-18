#!/bin/sh
# Headless QEMU smoke test for GordOS.
#
# Boots the ISO with a FAT32 disk, types `exec HELLO.ELF` and
# `exec CRASH.ELF` into the shell via the QEMU monitor, and checks the
# serial log for the expected output:
#   - the boot banner and FAT32 mount
#   - "Hello from ring 3!"  (exec + syscalls + clean exit work)
#   - "User process killed" (ring-3 fault recovery works)
#
# Usage: tools/boot-test.sh  (run from the repo root, after
#        `make iso` and `make disk`)

set -e

SERIAL_LOG=$(mktemp /tmp/gordos-serial.XXXXXX)
MON_SOCK=$(mktemp -u /tmp/gordos-mon.XXXXXX)

qemu-system-i386 -boot order=d -cdrom GordOS.iso \
    -drive file=disk.img,format=raw,if=ide,index=0 \
    -serial "file:$SERIAL_LOG" -display none \
    -monitor "unix:$MON_SOCK,server,nowait" &
QEMU_PID=$!

cleanup() {
    kill "$QEMU_PID" 2>/dev/null || true
    rm -f "$SERIAL_LOG" "$MON_SOCK"
}
trap cleanup EXIT

# Wait for the shell prompt to appear in the serial log
i=0
while ! grep -q 'GordOS/>' "$SERIAL_LOG" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt 60 ]; then
        echo "FAIL: shell prompt never appeared"
        cat "$SERIAL_LOG"
        exit 1
    fi
    sleep 1
done

# Type commands into the guest via QEMU monitor sendkey
python3 - "$MON_SOCK" "fasterfetch" "exec HELLO.ELF" "exec CRASH.ELF" \
        "exec FORKTEST.ELF" "exec FDCAT.ELF" "exec REDIR.ELF" \
        "exec USH.ELF" "HELLO.ELF" "exit" "bg COUNTER.ELF" "ps" << 'EOF'
import socket, sys, time

KEYMAP = {' ': 'spc', '.': 'dot', '/': 'slash', '-': 'minus'}

def key_for(ch):
    if ch in KEYMAP: return KEYMAP[ch]
    if ch.isupper(): return 'shift-' + ch.lower()
    return ch

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(sys.argv[1])
sock.settimeout(2)
time.sleep(0.3)
try: sock.recv(4096)
except Exception: pass
for line in sys.argv[2:]:
    for ch in line:
        sock.sendall(('sendkey %s\n' % key_for(ch)).encode())
        time.sleep(0.08)
        try: sock.recv(4096)
        except Exception: pass
    sock.sendall(b'sendkey ret\n')
    time.sleep(2)
    try: sock.recv(4096)
    except Exception: pass
sock.close()
EOF

# Give the background COUNTER.ELF time to finish its run
sleep 5

status=0
# Fixed-string matches (-F): some expectations contain regex
# metacharacters like '[counter]' that must be taken literally.
for expect in 'FAT32 MOUNTED' 'GordOS (i686)' 'Hello from ring 3!' 'User process killed' \
              'running in background' '[counter] tick' '] done' \
              'forktest: reaped child pid' 'forktest: child exit code 0' \
              'FDSTART' 'FDEND' 'fdcat: read 1034 bytes' \
              'captured line one' 'redir: ok' \
              'ush - user-space shell' 'ush: bye'; do
    if grep -qF "$expect" "$SERIAL_LOG"; then
        echo "PASS: $expect"
    else
        echo "FAIL: missing '$expect'"
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    echo "--- serial log ---"
    cat "$SERIAL_LOG"
fi

exit "$status"
