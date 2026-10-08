#!/usr/bin/env python3
"""Boot a tiny ISO headless in QEMU, press keys, and save PNG screenshots.

usage: tools/qemu-shot.py ISO OUTDIR [--uefi] [--usb-kbd] [STEP ...]

--usb-kbd: PS/2 off; keys go to a USB keyboard on an xHCI.

Waits for "UI READY" on the serial port, then runs each STEP in order:
  shot:NAME   save OUTDIR/NAME.png
  wait:SECS   sleep
  other       send as a QEMU key (qcode), e.g. right, down, pgdn, f2, r, esc
The serial log is printed when QEMU is stopped.
"""
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

OVMF_CODE = '/usr/share/edk2/OvmfX64/OVMF_CODE.fd'
OVMF_VARS = '/usr/share/edk2/OvmfX64/OVMF_VARS.fd'


class QMP:
    def __init__(self, path):
        for _ in range(100):
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.connect(path)
                break
            except OSError:
                time.sleep(0.1)
        else:
            sys.exit('could not connect to the QMP socket')
        self.file = self.sock.makefile('rw')
        self._read()  # greeting
        self.cmd('qmp_capabilities')

    def _read(self):
        while True:
            msg = json.loads(self.file.readline())
            if 'event' not in msg:
                return msg

    def cmd(self, name, **args):
        self.file.write(json.dumps({'execute': name, 'arguments': args}) + '\n')
        self.file.flush()
        reply = self._read()
        if 'error' in reply:
            sys.exit('QMP %s failed: %s' % (name, reply['error']))
        return reply.get('return')


def wait_for(path, text, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if os.path.exists(path) and text in open(path, errors='replace').read():
            return
        time.sleep(0.2)
    sys.exit('timed out waiting for %r on serial' % text)


def main():
    args = sys.argv[1:]
    uefi = '--uefi' in args
    usb = '--usb-kbd' in args
    args = [a for a in args if a not in ('--uefi', '--usb-kbd')]
    if len(args) < 2:
        sys.exit(__doc__)
    iso, outdir, steps = args[0], args[1], args[2:]
    os.makedirs(outdir, exist_ok=True)

    tmp = tempfile.mkdtemp(prefix='tiny-shot-')
    qmp_path = os.path.join(tmp, 'qmp.sock')
    serial_path = os.path.join(tmp, 'serial.log')
    cmd = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-display', 'none', '-no-reboot',
           '-serial', 'file:' + serial_path,
           '-qmp', 'unix:%s,server=on,wait=off' % qmp_path,
           '-cdrom', iso, '-boot', 'd']
    if usb:
        cmd[cmd.index('q35')] = 'q35,i8042=off'
        cmd += ['-device', 'qemu-xhci,id=xhci', '-device', 'usb-kbd,bus=xhci.0']
    if uefi:
        vars_copy = os.path.join(tmp, 'vars.fd')
        shutil.copy(OVMF_VARS, vars_copy)
        cmd += ['-drive', 'if=pflash,unit=0,format=raw,readonly=on,file=' + OVMF_CODE,
                '-drive', 'if=pflash,unit=1,format=raw,file=' + vars_copy]

    qemu = subprocess.Popen(cmd)
    try:
        qmp = QMP(qmp_path)
        wait_for(serial_path, 'UI READY', 60)
        time.sleep(0.5)
        for step in steps:
            if step.startswith('shot:'):
                out = os.path.abspath(os.path.join(outdir, step[5:] + '.png'))
                qmp.cmd('screendump', filename=out, format='png')
                print('saved', out)
            elif step.startswith('wait:'):
                time.sleep(float(step[5:]))
            else:
                qmp.cmd('send-key', keys=[{'type': 'qcode', 'data': step}])
                time.sleep(0.4)
    finally:
        qemu.kill()
        qemu.wait()
        if os.path.exists(serial_path):
            print('--- serial ---')
            print(open(serial_path, errors='replace').read())
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    main()
