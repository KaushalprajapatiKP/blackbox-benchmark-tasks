# Stage 1: build the original tool from source. Only the stripped static binary
# is carried into the agent image; the source never reaches it.
FROM gcc:12-bookworm@sha256:c18df6babaae3d61789651ba987f767d05bcba481160301963673729d62666fa AS build
COPY src/tabula.c /src/tabula.c
RUN gcc -O2 -static -s -o /src/tabula /src/tabula.c

FROM python:3.12-slim-bookworm@sha256:bc01d00b6417e4b1b01c331db3a14054b4be954e571ef8bf5748aaf64b66e941
RUN pip install --no-cache-dir pytest==8.3.3
COPY --from=build /src/tabula /app/bin/tabula
COPY docs/tabula.1.md /app/docs/tabula.1.md
RUN chown root:root /app/bin/tabula && chmod 0111 /app/bin/tabula && mkdir -p /app/rebuild
ENV PATH="/app/bin:${PATH}"
WORKDIR /app
