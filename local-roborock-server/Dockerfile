FROM python:3.13-slim AS build

ARG LOCAL_ROBOROCK_SERVER_VERSION

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    mkdir /src; \
    curl -fsSL "https://github.com/Python-roborock/local_roborock_server/archive/refs/tags/v${LOCAL_ROBOROCK_SERVER_VERSION}.tar.gz" \
      | tar -xz --strip-components=1 -C /src; \
    sed -i 's/"cryptography>=42,<47"/"cryptography>=50.0.0,<51"/; s/^\(\s*\)"python-roborock==\(.*\)",/&\n\1"Pillow>=13,<14",/' /src/pyproject.toml; \
    grep -q 'cryptography>=50.0.0' /src/pyproject.toml; \
    grep -q 'Pillow>=13' /src/pyproject.toml; \
    python -m venv /opt/venv; \
    /opt/venv/bin/pip install --no-cache-dir /src; \
    /opt/venv/bin/python -m pip uninstall -y pip; \
    /opt/venv/bin/python -c 'import importlib.util, sys; sys.exit(importlib.util.find_spec("pip") is not None)'

FROM python:3.13-slim

LABEL org.opencontainers.image.source="https://github.com/Python-roborock/local_roborock_server" \
      org.opencontainers.image.description="Roborock local server rebuilt on a current base with patched dependencies" \
      org.opencontainers.image.licenses="MIT"

RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y --no-install-recommends mosquitto \
    && rm -rf /var/lib/apt/lists/* \
    && python -m pip uninstall -y pip \
    && python -c 'import importlib.util, sys; sys.exit(importlib.util.find_spec("pip") is not None)'

COPY --from=build /opt/venv /opt/venv

ENV PATH=/opt/venv/bin:$PATH

WORKDIR /app

EXPOSE 555 8881

CMD ["python", "-m", "roborock_local_server.container_entrypoint"]
