FROM ghcr.io/kagent-dev/kagent/acp-sandbox-hermes@sha256:9d8c7b4c94cb242a502ef8a763115f72b78f3c23079e71f6f07d5a0b76a03697

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
