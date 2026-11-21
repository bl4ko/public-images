FROM docker.io/library/ubuntu:resolute@sha256:d66b366dac61b575bee2aef07e9c26e8070096fbe84a3d4d3eb0da069b5d76db
ARG FLUTTER_VERSION=3.47.7
LABEL org.opencontainers.image.source=https://github.com/bl4ko/public-images
RUN apt-get -o Acquire::Retries=3 update \
 && DEBIAN_FRONTEND=noninteractive apt-get -o Acquire::Retries=3 install -y --no-install-recommends ca-certificates clang curl git unzip xz-utils \
 && rm -rf /var/lib/apt/lists/* \
 && git config --system --add safe.directory '*' \
 && mkdir /opt/flutter \
 && chown 1001:1001 /opt/flutter
USER 1001:1001
ENV HOME=/tmp \
    PATH=/opt/flutter/bin:$PATH \
    FLUTTER_SUPPRESS_ANALYTICS=true \
    CI_FLUTTER_VERSION=$FLUTTER_VERSION
RUN curl -fsSL --retry 3 "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" | tar -xJ -C /opt \
 && flutter --version \
 && flutter precache --no-android --no-web --no-linux \
 && cd /tmp \
 && flutter create --platforms=linux --project-name smoke smoke >/dev/null \
 && cd smoke \
 && flutter test \
 && cd / \
 && rm -rf /tmp/smoke /tmp/.pub-cache /tmp/.dart-tool /tmp/.flutter /tmp/.config /tmp/.cache
WORKDIR /tmp
