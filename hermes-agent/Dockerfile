FROM ghcr.io/kagent-dev/kagent/acp-sandbox-hermes@sha256:9d8c0a4eff9b4fb20a50b5722f8b6f68dd919e26414d8f70d12fe2d94cc24085

USER root
RUN apt-get update \
    && apt-get install -y --no-install-recommends gh \
    && rm -rf /var/lib/apt/lists/* \
    && mv /home/agent/.venv/bin/hermes /home/agent/.venv/bin/hermes-real
COPY --chmod=755 hermes /home/agent/.venv/bin/hermes
RUN HOME=/tmp/hermes-test HERMES_AUTH_JSON_B64=e30= /home/agent/.venv/bin/hermes --version \
    && test -s /tmp/hermes-test/.hermes/auth.json \
    && rm -r /tmp/hermes-test
USER agent
