# syntax=docker/dockerfile:1
FROM gcc:12.5.0-bookworm AS build
COPY vpack/vpack.c /src/vpack.c
RUN gcc -O2 -std=c11 -Wall -Wextra -o /src/vpack /src/vpack.c && strip --strip-all /src/vpack

FROM python:3.12-slim-bookworm
RUN pip install --no-cache-dir pytest==8.3.4
COPY --from=build /src/vpack /app/bin/vpack
COPY vpack/LICENSE /app/bin/LICENSE.vpack
COPY README.txt /app/README.txt
RUN --mount=type=bind,source=mk_samples.py,target=/tmp/mk_samples.py \
    python3 /tmp/mk_samples.py /app/samples
ENV PATH="/app/bin:${PATH}"
WORKDIR /app
