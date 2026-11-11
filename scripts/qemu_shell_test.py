"""Drive the Tinx shell in QEMU over stdio QMP: spawn QEMU, type commands
with sendkey (via human-monitor-command), screendump after each step, and
finish with a reboot test. Usage: python -I qemu_shell_test.py <outdir>"""
import json
import os
import subprocess
import sys
import time

QEMU = r"C:\Program Files\qemu\qemu-system-i386"

KEYMAP = {
    'a':'a','b':'b','c':'c','d':'d','e':'e','f':'f','g':'g','h':'h','i':'i',
    'j':'j','k':'k','l':'l','m':'m','n':'n','o':'o','p':'p','q':'q','r':'r',
    's':'s','t':'t','u':'u','v':'v','w':'w','x':'x','y':'y','z':'z',
    '0':'0','1':'1','2':'2','3':'3','4':'4','5':'5','6':'6','7':'7','8':'8',
    '9':'9',' ':'spc','/':'slash','.':'dot','-':'minus','=':'equal',
    ',':'comma',';':'semicolon','\\':'backslash',
}
SHIFTED = {':':'semicolon','"':'apostrophe','_':'minus'}


class QMPStdio:
    def __init__(self, kernel_path, serial_log):
        self.p = subprocess.Popen(
            [QEMU, "-kernel", kernel_path, "-serial", "file:" + serial_log,
             "-display", "none", "-no-reboot", "-qmp", "stdio"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding="utf-8")
        time.sleep(2)            # QEMU's stdio QMP flushes slowly
        self.cmd("qmp_capabilities")

    def _readmsg(self):
        """Read messages until one carries a return or error (skips the
        QMP greeting / async events)."""
        while True:
            line = self.p.stdout.readline()
            if not line:
                err = self.p.stderr.read() if self.p.stderr else b""
                raise RuntimeError(
                    f"QEMU exited (rc={self.p.poll()}) stderr={err!r}")
            try:
                msg = json.loads(line)
            except json.JSONDecodeError:
                continue
            if "return" in msg or "error" in msg:
                return msg

    def cmd(self, name, **args):
        msg = {"execute": name}
        if args:
            msg["arguments"] = args
        self.p.stdin.write(json.dumps(msg) + "\n")
        self.p.stdin.flush()
        resp = self._readmsg()
        if "error" in resp:
            raise RuntimeError(f"QMP error: {resp['error']}")
        return resp["return"]

    def key(self, k):
        self.cmd("human-monitor-command", **{"command-line": f"sendkey {k}"})

    def type_text(self, text):
        for ch in text:
            if ch in KEYMAP:
                self.key(KEYMAP[ch])
            elif ch in SHIFTED:
                self.key(f"shift-{SHIFTED[ch]}")
            elif ch == '\n':
                self.key("ret")
            else:
                raise ValueError(f"no key mapping for {ch!r}")
            time.sleep(0.03)

    def dump(self, path):
        self.cmd("screendump", filename=path)
        time.sleep(0.3)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    kernel = os.path.join(os.path.dirname(__file__), "..", "build", "tinx.bin")
    serial_log = os.path.join(outdir, f"serial_{os.getpid()}.log")

    q = QMPStdio(os.path.abspath(kernel), os.path.abspath(serial_log))
    time.sleep(2)

    script = [
        ("01_help", "help\n"),
        ("02_date", "date\n"),
        ("03_mkdir", "mkdir /home\n"),
        ("04_touch", "touch /home/t.txt\n"),
        ("05_write", "write /home/t.txt hello world foo\n"),
        ("06_write2", "write /home/t.txt a second test line\n"),
        ("07_cat", "cat /home/t.txt\n"),
        ("08_wc", "wc /home/t.txt\n"),
        ("09_grep", "grep second /home/t.txt\n"),
        ("10_grepmiss", "grep zebra /home/t.txt\n"),
        ("11_history", "history\n"),
        ("12_reboot", "reboot\n"),
    ]
    for name, text in script:
        print(f"typing: {text!r}")
        q.type_text(text)
        time.sleep(0.8)
        try:
            q.dump(os.path.join(outdir, name + ".ppm"))
        except (RuntimeError, OSError):
            if name.endswith("reboot"):
                print("QEMU exited during/after reboot - OK")
                break
            raise

    time.sleep(3)
    try:
        q.cmd("query-status")
        print("NOTE: QEMU still running after reboot")
        q.p.kill()
    except (RuntimeError, OSError):
        print("QEMU exited after reboot (-no-reboot), as expected")


if __name__ == "__main__":
    main()
