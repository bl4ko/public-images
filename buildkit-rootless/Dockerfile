FROM moby/buildkit:v0.34.2-rootless@sha256:c8252268760e1baba80780d63d738ef9f260fb7c7c0973c78494cb9ddd99eda1

USER root
RUN apk upgrade --no-cache brotli-libs && \
    chmod u+s /usr/bin/newuidmap /usr/bin/newgidmap
USER 1000:1000
