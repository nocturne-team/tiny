#!/usr/bin/env python3
"""Boot a tiny SMOKE_KEYS ISO in QEMU with PS/2 off and an xHCI, drive USB
devices over QMP, and check that a key switched tabs.

usage: tools/qemu-keytest.py ISO [--uefi] [QEMU_ARG ...] -- STEP ...

The machine is q35,i8042=off with -device qemu-xhci,id=xhci; QEMU_ARGs add
devices (e.g. -device usb-kbd,bus=xhci.0). Steps run in order:
  wait:REGEX       wait until REGEX matches on serial once more than at the
                   previous wait for the same REGEX (30 s timeout)
  key:QCODE        press and release a key (goes to the USB keyboard)
  add:DRIVER,K=V…  device_add
  del:ID           device_del
  mouse:DX:DY      relative mouse motion (to the USB mouse)
  click:BUTTON     press and release a mouse button (left, right, middle)
  count:N:REGEX    after QEMU exits, REGEX must match exactly N times
QEMU must then exit through isa-debug-exit 0x10 (status 33): the SMOKE_KEYS
build does that on the first tab change.
"""
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True  # importing qemu-shot.py must not leave tools/__pycache__
HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('qemu_shot', os.path.join(HERE, 'qemu-shot.py'))
shot = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(shot)


def serial(path):
    return open(path, errors='replace').read() if os.path.exists(path) else ''


def main():
    args = sys.argv[1:]
    if '--' not in args:
        sys.exit(__doc__)
    split = args.index('--')
    head, steps = args[:split], args[split + 1:]
    uefi = '--uefi' in head
    head = [a for a in head if a != '--uefi']
    if not head:
        sys.exit(__doc__)
    iso, extra = head[0], head[1:]

    tmp = tempfile.mkdtemp(prefix='tiny-keys-')
    qmp_path = os.path.join(tmp, 'qmp.sock')
    serial_path = os.path.join(tmp, 'serial.log')
    cmd = ['qemu-system-x86_64', '-M', 'q35,i8042=off', '-m', '512M', '-display', 'none',
           '-no-reboot', '-serial', 'file:' + serial_path,
           '-qmp', 'unix:%s,server=on,wait=off' % qmp_path,
           '-device', 'isa-debug-exit,iobase=0xf4,iosize=0x04',
           '-device', 'qemu-xhci,id=xhci', '-cdrom', iso, '-boot', 'd'] + extra
    if uefi:
        vars_copy = os.path.join(tmp, 'vars.fd')
        shutil.copy(shot.OVMF_VARS, vars_copy)
        cmd += ['-drive', 'if=pflash,unit=0,format=raw,readonly=on,file=' + shot.OVMF_CODE,
                '-drive', 'if=pflash,unit=1,format=raw,file=' + vars_copy]

    qemu = subprocess.Popen(cmd)
    ok, qmp = False, None
    try:
        qmp = shot.QMP(qmp_path)
        shot.wait_for(serial_path, 'UI READY', 60)
        seen, counts = {}, []
        for step in steps:
            kind, _, rest = step.partition(':')
            if kind == 'wait':
                want = seen.get(rest, 0) + 1
                deadline = time.time() + 30
                while len(re.findall(rest, serial(serial_path))) < want:
                    exited = qemu.poll() is not None
                    if exited or time.time() > deadline:
                        if len(re.findall(rest, serial(serial_path))) >= want:
                            break  # QEMU exited right after printing it
                        raise SystemExit('KEYTEST FAIL: %s waiting for %r'
                                         % ('QEMU exited' if exited else 'timed out', rest))
                    time.sleep(0.2)
                seen[rest] = want
            elif kind == 'key':
                try:
                    qmp.cmd('send-key', keys=[{'type': 'qcode', 'data': rest}])
                except (OSError, ValueError):
                    pass  # QEMU may exit before answering: that is the expected outcome
            elif kind == 'add':
                driver, *opts = rest.split(',')
                qmp.cmd('device_add', driver=driver, **dict(o.split('=', 1) for o in opts))
            elif kind == 'mouse':
                dx, _, dy = rest.partition(':')
                shot.send_mouse(qmp, int(dx), int(dy))
            elif kind == 'click':
                for down in (True, False):
                    try:
                        qmp.cmd('input-send-event', events=[
                            {'type': 'btn', 'data': {'down': down, 'button': rest}}])
                    except (OSError, ValueError):
                        pass  # QEMU may exit on the click: the expected outcome
                    time.sleep(0.2)
            elif kind == 'del':
                qmp.cmd('device_del', id=rest)
            elif kind == 'count':
                n, _, pattern = rest.partition(':')
                counts.append((int(n), pattern))
            else:
                raise SystemExit('KEYTEST FAIL: unknown step %r' % step)
        try:
            status = qemu.wait(timeout=20)
        except subprocess.TimeoutExpired:
            raise SystemExit('KEYTEST FAIL: QEMU did not exit (no tab change?)')
        if status != 33:
            raise SystemExit('KEYTEST FAIL: QEMU exit status %d, expected 33' % status)
        log = serial(serial_path)
        for n, pattern in counts:
            got = len(re.findall(pattern, log))
            if got != n:
                raise SystemExit('KEYTEST FAIL: %r matched %d times, expected %d' % (pattern, got, n))
        ok = True
        print('KEYTEST OK', ' '.join(head[1:]), '--', ' '.join(steps))
    finally:
        if qmp:
            try:
                qmp.file.close()  # QEMU may be gone: drop the unflushed buffer quietly
            except OSError:
                pass
        if qemu.poll() is None:
            qemu.kill()
            qemu.wait()
        if not ok:
            print('--- serial ---')
            print(serial(serial_path))
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    main()
