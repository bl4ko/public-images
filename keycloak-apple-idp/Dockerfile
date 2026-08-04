FROM gradle:8.14-jdk17 AS builder

ARG EXTENSION_VERSION=1.17.0
# audit-pinned commit; re-audit source before bumping
ARG EXTENSION_COMMIT=665011f7ef021850ce5e7a92e33a8bf969c9a29f

RUN git clone --depth 1 --branch "$EXTENSION_VERSION" https://github.com/klausbetz/apple-identity-provider-keycloak.git /src \
    && cd /src \
    && [ "$(git rev-parse HEAD)" = "$EXTENSION_COMMIT" ]

WORKDIR /src
RUN gradle jar --no-daemon

FROM busybox:1.37
ARG EXTENSION_VERSION=1.17.0
COPY --from=builder /src/build/libs/apple-identity-provider-${EXTENSION_VERSION}.jar /apple-identity-provider.jar
