# Builds zeroclaw with memory-postgres feature for CNPG integration.
# Upstream: https://github.com/zeroclaw-labs/zeroclaw
# renovate: datasource=github-releases depName=zeroclaw-labs/zeroclaw
ARG ZEROCLAW_VERSION=v0.1.7

FROM rust:1.93-slim AS builder

WORKDIR /app
ARG ZEROCLAW_VERSION

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
        pkg-config git \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch "${ZEROCLAW_VERSION}" \
    https://github.com/zeroclaw-labs/zeroclaw.git . && \
    sed -i '/^dist\/$/d' web/.gitignore

RUN --mount=type=cache,id=zeroclaw-cargo-registry,target=/usr/local/cargo/registry,sharing=locked \
    --mount=type=cache,id=zeroclaw-cargo-git,target=/usr/local/cargo/git,sharing=locked \
    --mount=type=cache,id=zeroclaw-target,target=/app/target,sharing=locked \
    cargo build --release --features memory-postgres && \
    cp target/release/zeroclaw /app/zeroclaw && \
    strip /app/zeroclaw

RUN mkdir -p /zeroclaw-data/.zeroclaw /zeroclaw-data/workspace && \
    chown -R 65534:65534 /zeroclaw-data

FROM gcr.io/distroless/cc-debian13:nonroot

COPY --from=builder /app/zeroclaw /usr/local/bin/zeroclaw
COPY --from=builder /zeroclaw-data /zeroclaw-data

ENV ZEROCLAW_WORKSPACE=/zeroclaw-data/workspace
ENV HOME=/zeroclaw-data
ENV ZEROCLAW_GATEWAY_PORT=42617

WORKDIR /zeroclaw-data
USER 65534:65534
EXPOSE 42617
ENTRYPOINT ["zeroclaw"]
CMD ["daemon"]
