FROM mcr.microsoft.com/dotnet/runtime-deps:10.0@sha256:e6702e45635272155c2aa35568bcbdb975595c0afa73e2656a973d0a87703282 AS base
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl unzip cabextract jq \
    && rm -rf /var/lib/apt/lists/* \
    && userdel --remove ubuntu \
    && groupadd --gid 1000 openso \
    && useradd --uid 1000 --gid 1000 --home-dir /app --no-create-home --shell /usr/sbin/nologin openso

FROM base AS fetch
ARG OPENSO_VERSION=v0.4.3
ARG OPENSO_SHA256=04db0eb7dd033e0c46fdf1749142a8e7eb04a49a6ddc70143b64d69d7b6b4ae2
RUN curl -fsSL --retry 5 "https://github.com/voicemxil/OpenSO/releases/download/${OPENSO_VERSION}/OpenSO-server-linux-x64.zip" -o /tmp/server.zip \
    && echo "${OPENSO_SHA256}  /tmp/server.zip" | sha256sum -c - \
    && unzip -q /tmp/server.zip -d /app \
    && test "$(cat /app/version.txt)" = "${OPENSO_VERSION}" \
    && chmod 0755 /app/FSO.Server.Core \
    && mkdir -p /app/log \
    && rm /tmp/server.zip

FROM base
COPY --from=fetch --chown=1000:1000 /app /app
COPY --chmod=0755 entrypoint.sh /usr/local/bin/openso-entrypoint
COPY --chmod=0755 openso-fetch-tso /usr/local/bin/openso-fetch-tso
COPY config.template.json /etc/openso/config.template.json
RUN install -d -o 1000 -g 1000 /game /nfs
WORKDIR /app
USER 1000:1000
EXPOSE 9000 33101 34101 35101
ENTRYPOINT ["/usr/local/bin/openso-entrypoint"]
