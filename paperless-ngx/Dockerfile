FROM ghcr.io/paperless-ngx/paperless-ngx:2.20.13

RUN apt-get update \
    && apt-get install -y --no-install-recommends tesseract-ocr-slv \
    && rm -rf /var/lib/apt/lists/*
