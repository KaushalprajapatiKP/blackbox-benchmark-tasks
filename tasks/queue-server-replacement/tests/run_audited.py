"""Runs a Python server script under an audit hook that forbids starting processes,
ctypes and native extension modules from outside the Python installation."""
import os
import runpy
import sys

_STDLIB_ROOTS = tuple(os.path.realpath(p) + os.sep for p in {sys.base_prefix, sys.base_exec_prefix})
_BLOCKED = {
    "subprocess.Popen", "os.system", "os.exec", "os.posix_spawn", "os.spawn", "os.fork",
    "os.forkpty", "os.startfile", "pty.spawn", "os.add_dll_directory",
}


def _hook(event, args):
    if event in _BLOCKED or event.startswith("ctypes."):
        raise RuntimeError("blocked by grader: " + event)
    if event == "import" and len(args) > 1 and args[1]:
        fn = str(args[1])
        if fn.endswith((".so", ".pyd")) and not os.path.realpath(fn).startswith(_STDLIB_ROOTS):
            raise RuntimeError("blocked by grader: native module " + fn)


def main():
    script = os.path.abspath(sys.argv[1])
    sys.argv = sys.argv[1:]
    sys.path.insert(0, os.path.dirname(script))
    sys.addaudithook(_hook)
    runpy.run_path(script, run_name="__main__")


if __name__ == "__main__":
    main()
