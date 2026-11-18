"""Run the rebuild under an audit hook that blocks process creation and native
code loading. Invoked as: python3 -I -S sandbox_run.py /app/rebuild/tabula ARGS...
MIT License."""
import sys

BLOCKED_EVENTS = {
    "subprocess.Popen", "os.exec", "os.posix_spawn", "os.spawn", "os.system",
    "os.fork", "os.forkpty", "os.startfile", "pty.spawn", "ctypes.dlopen",
    "ctypes.dlsym", "ctypes.cdata", "ctypes.call_function",
}
BLOCKED_MODULES = {"_posixsubprocess", "_ctypes", "ctypes", "cffi", "_cffi_backend"}


def _hook(event, args):
    if event in BLOCKED_EVENTS:
        raise RuntimeError("blocked by grader sandbox: " + event)
    if event == "import" and args and args[0] in BLOCKED_MODULES:
        raise ImportError("blocked by grader sandbox: import " + str(args[0]))


def main():
    target = sys.argv[1]
    sys.argv = sys.argv[1:]
    sys.path.insert(0, "/app/rebuild")
    sys.addaudithook(_hook)
    import runpy
    runpy.run_path(target, run_name="__main__")


main()
