# Build stage: the server source never reaches the final image.
FROM gcc:12.5.0-bookworm AS build
COPY src/qsrv.c /build/qsrv.c
RUN gcc -O2 -static -o /build/qsrv /build/qsrv.c && strip --strip-all /build/qsrv && rm /build/qsrv.c

FROM python:3.12.15-slim-bookworm
RUN pip install --no-cache-dir pytest==8.3.4
COPY --from=build /build/qsrv /app/bin/qsrv
COPY docs/ /app/docs/
RUN chmod 0755 /app/bin/qsrv && mkdir -p /app/replacement
WORKDIR /app
