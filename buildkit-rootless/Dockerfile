FROM --platform=$BUILDPLATFORM tonistiigi/xx:1.10.0@sha256:40fe21f44bb8cddf1eb1226db190d0a984118f62ee02a91b77d99448d28264a6 AS xx

FROM --platform=$BUILDPLATFORM golang:1.27.4-alpine3.24@sha256:70bd7ddea8b6cbe5d6943cde279d6b67feb10cc924fb9100d1d8757c1575b280 AS gobuild
RUN apk add --no-cache clang lld musl-dev file git
COPY --from=xx / /
ENV GOFLAGS=-mod=mod GOTOOLCHAIN=local CGO_ENABLED=0

FROM gobuild AS buildkit
# renovate-commit: datasource=github-tags depName=moby/buildkit
ARG BUILDKIT_VERSION=v0.35.1
ARG BUILDKIT_COMMIT=412cc86ad3c4252b82e929f9b1b1ad167376ded8
ADD --checksum=${BUILDKIT_COMMIT} https://github.com/moby/buildkit.git?ref=${BUILDKIT_VERSION} /src
WORKDIR /src
ARG TARGETPLATFORM
RUN rm -rf vendor && \
    go get github.com/moby/go-archive@v0.3.3 && \
    ldflags="-X github.com/moby/buildkit/version.Version=${BUILDKIT_VERSION} -X github.com/moby/buildkit/version.Revision=${BUILDKIT_COMMIT} -X github.com/moby/buildkit/version.Package=github.com/moby/buildkit" && \
    xx-go build -tags grpcnotrace -ldflags "$ldflags" -o /out/buildctl ./cmd/buildctl && \
    xx-go build -tags "osusergo netgo static_build seccomp grpcnotrace" -ldflags "$ldflags -extldflags '-static'" -o /out/buildkitd ./cmd/buildkitd && \
    xx-verify --static /out/buildctl && xx-verify --static /out/buildkitd && \
    go version -m /out/buildkitd | grep -Eq '^[[:space:]]+dep[[:space:]]+github.com/moby/go-archive[[:space:]]+v0.3.3[[:space:]]' && \
    go version -m /out/buildkitd | grep -Eq '^/out/buildkitd: go1\.27\.'

FROM gobuild AS runc
# renovate-commit: datasource=github-tags depName=opencontainers/runc
ARG RUNC_VERSION=v1.5.3
ARG RUNC_COMMIT=46365c62967c76a765a5e347f48f0c7b3b571fe9
ADD --checksum=${RUNC_COMMIT} https://github.com/opencontainers/runc.git?ref=${RUNC_VERSION} /src
WORKDIR /src
ARG TARGETPLATFORM
RUN xx-apk add --no-cache musl-dev gcc libseccomp-dev libseccomp-static
RUN rm -rf vendor && \
    go get golang.org/x/net@v0.62.0 && \
    CGO_ENABLED=1 xx-go build -tags "apparmor seccomp netgo cgo static_build osusergo" -ldflags "-X main.gitCommit=${RUNC_COMMIT} -extldflags -static" -o /out/buildkit-runc . && \
    xx-verify --static /out/buildkit-runc && \
    go version -m /out/buildkit-runc | grep -Eq '^[[:space:]]+dep[[:space:]]+golang.org/x/net[[:space:]]+v0.62.0[[:space:]]' && \
    go version -m /out/buildkit-runc | grep -Eq '^/out/buildkit-runc: go1\.27\.'

FROM gobuild AS rootlesskit
# renovate-commit: datasource=github-tags depName=rootless-containers/rootlesskit
ARG ROOTLESSKIT_VERSION=v3.2.0
ARG ROOTLESSKIT_COMMIT=f0cc012b20fd6039fc0b73e83051a521a4690a2e
ADD --checksum=${ROOTLESSKIT_COMMIT} https://github.com/rootless-containers/rootlesskit.git?ref=${ROOTLESSKIT_VERSION} /src
WORKDIR /src
ARG TARGETPLATFORM
RUN rm -rf vendor && \
    go get golang.org/x/net@v0.62.0 && \
    xx-go build -o /out/rootlesskit ./cmd/rootlesskit && \
    xx-verify --static /out/rootlesskit && \
    go version -m /out/rootlesskit | grep -Eq '^[[:space:]]+dep[[:space:]]+golang.org/x/net[[:space:]]+v0.62.0[[:space:]]' && \
    go version -m /out/rootlesskit | grep -Eq '^/out/rootlesskit: go1\.27\.'

FROM moby/buildkit:v0.35.1-rootless@sha256:7d02292556e66feea6d2d89b7ec143503b57e4f59a4efc6b7ece8547e4d3e5be

USER root
COPY --from=buildkit /out/buildkitd /out/buildctl /usr/bin/
COPY --from=runc /out/buildkit-runc /usr/bin/
COPY --from=rootlesskit /out/rootlesskit /usr/bin/
RUN apk upgrade --no-cache && \
    rm -f /usr/bin/buildkit-cni-* && \
    chmod u+s /usr/bin/newuidmap /usr/bin/newgidmap
RUN apk upgrade --no-cache && apk version -t "$(apk info -e -v libssl3 | sed 's/^libssl3-//')" 3.5.9-r1 | grep -q "[=>]" && apk version -t "$(apk info -e -v libcrypto3 | sed 's/^libcrypto3-//')" 3.5.9-r1 | grep -q "[=>]"
USER 1000:1000
