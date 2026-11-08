#!/bin/sh
# Boot an ISO headless in QEMU and check its serial log.
# usage: tests/smoke.sh ISO bios|uefi ok|panic|shutdown [MACHINE] [PATTERN...]
#   ok        log has "PROBE OK" and "UI READY"; exit via isa-debug-exit 0x10 (status 33)
#   panic     log has "PANIC: #UD"; exit via isa-debug-exit 0x11 (status 35)
#   shutdown  log has "power: ACPI S5"; QEMU powered off (status 0)
# Every extra PATTERN must also appear in the log; a PATTERN starting with ! must not.
#   QEMU_EXTRA (env) extra QEMU arguments; default "-device qemu-xhci,id=xhci"
set -u
iso=$1 fw=$2 expect=$3
machine=q35
if [ $# -ge 4 ]; then machine=$4; shift 4; else shift 3; fi

OVMF_CODE=${OVMF_CODE:-/usr/share/edk2/OvmfX64/OVMF_CODE.fd}
OVMF_VARS=${OVMF_VARS:-/usr/share/edk2/OvmfX64/OVMF_VARS.fd}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
log=$tmp/serial.log
: > "$log"

fwflags=""
if [ "$fw" = uefi ]; then
    cp "$OVMF_VARS" "$tmp/vars.fd"
    fwflags="-drive if=pflash,unit=0,format=raw,readonly=on,file=$OVMF_CODE -drive if=pflash,unit=1,format=raw,file=$tmp/vars.fd"
fi

# shellcheck disable=SC2086
timeout 90 qemu-system-x86_64 -M "$machine" -m 512M -display none -no-reboot \
    -serial "file:$log" -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    $fwflags ${QEMU_EXTRA--device qemu-xhci,id=xhci} -cdrom "$iso" -boot d
status=$?

fail() {
    echo "SMOKE FAIL [$fw $expect $machine]: $1"
    echo "--- serial log ---"
    cat "$log"
    exit 1
}

case $expect in
    ok)       want_status=33; want_text="UI READY" ;;
    panic)    want_status=35; want_text="PANIC: #UD" ;;
    shutdown) want_status=0;  want_text="power: ACPI S5" ;;
    *) echo "unknown expectation: $expect"; exit 2 ;;
esac

[ "$status" -ne 124 ] || fail "timed out"
[ "$status" -eq "$want_status" ] || fail "QEMU exit status $status, expected $want_status"
grep -q "$want_text" "$log" || fail "missing '$want_text'"
[ "$expect" != ok ] || grep -q "PROBE OK" "$log" || fail "missing 'PROBE OK'"
[ "$expect" = panic ] || ! grep -q "PANIC" "$log" || fail "unexpected panic"
for pat in "$@"; do
    case $pat in
        !*) ! grep -q "${pat#!}" "$log" || fail "unexpected '${pat#!}'" ;;
        *)  grep -q "$pat" "$log" || fail "missing '$pat'" ;;
    esac
done
echo "SMOKE OK [$fw $expect $machine]"
