vpack packs a directory tree into one archive file. Run `vpack pack DIR OUT.vp` to write the
archive. It keeps regular files, directories (empty ones too) and symbolic links, together with
their permission bits and modification times, and it compresses file contents when that makes
the archive smaller. This build cannot unpack. The archives in /app/samples were made with
`vpack pack /app/samples/NAME /app/samples/NAME.vp`.
