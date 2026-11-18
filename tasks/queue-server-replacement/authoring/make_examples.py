"""Produce the visible example transcripts from the real binary (authoring only).

python3 make_examples.py BINARY OUTDIR
"""
import os
import socket
import subprocess
import sys
import time

EXAMPLES = {
    "01-basics.txt": ["PING", "ECHO hello", "TIME", "WHOAMI"],
    "02-keys.txt": ["SET greeting hello", "GET greeting", "SET greeting world", "GETV greeting",
                    "DEL greeting", "GET greeting"],
    "03-queue.txt": ["PUSH jobs \"send email\"", "PUSH jobs report", "RESERVE jobs", "ACK 1", "TICK 5", "TIME"],
}


def main():
    binary, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    for i, (name, lines) in enumerate(EXAMPLES.items()):
        port = 24500 + i
        p = subprocess.Popen([binary, "--port", str(port)])
        time.sleep(0.3)
        s = socket.create_connection(("127.0.0.1", port))
        out = []
        for ln in lines:
            s.sendall((ln + "\n").encode())
            time.sleep(0.1)
            rep = s.recv(65536).decode()
            out.append("> " + ln)
            for r in rep.split("\r\n")[:-1]:
                out.append("< " + r)
        s.close()
        p.kill()
        p.wait()
        with open(os.path.join(outdir, name), "w") as f:
            f.write("# '>' lines are sent by the client (each ends with LF),\n")
            f.write("# '<' lines are received (each ends with CR LF).\n")
            f.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
