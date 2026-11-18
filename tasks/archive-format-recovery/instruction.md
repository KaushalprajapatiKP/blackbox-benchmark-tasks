`/app/bin/vpack` is an archiver. `vpack pack DIR OUT` packs the directory tree `DIR` into the archive file `OUT`. This build cannot unpack, and the archive format is not documented anywhere. `/app/samples` holds a few archives next to the directories they were packed from. `/app/README.txt` describes the tool.

Write `/app/unvpack`, an executable program, that unpacks these archives:

```
/app/unvpack ARCHIVE OUTDIR
```

`OUTDIR` does not exist before the call; your program creates it. On success it exits with status 0 and `OUTDIR` is an exact copy of the directory that was packed:

- the same entries (regular files, directories including empty ones, and symbolic links) with byte-identical names, and nothing else;
- byte-identical regular file contents;
- byte-identical symbolic link targets (links are recreated as links, not followed);
- the same permission bits (`st_mode & 0o7777`, including setuid, setgid and sticky) on every regular file and directory, and on `OUTDIR` itself;
- the same modification time, to the nanosecond, on every entry (files, directories and symbolic links) and on `OUTDIR` itself.

Ownership, access times and extended attributes are not checked.

Your program will be graded on many archives that this same `vpack` produced from other directory trees; anything `vpack` can pack may appear. It runs as root. `vpack` itself will not be installed at that point, so your program must not depend on it or on the contents of `/app/samples`. Each run must finish within 60 seconds; archives are at most a few megabytes.

If `ARCHIVE` is damaged (for example truncated, with bytes appended, or with bytes altered), `unvpack` must exit with status 1 and print a message containing the word `corrupt` on standard error. Only damage that can be detected from the archive itself (its structure and the checksums `vpack` stores in it) is tested. What is left in `OUTDIR` after a damaged archive does not matter.

Python 3.12 is installed. Passing requires every graded archive to be handled correctly.
