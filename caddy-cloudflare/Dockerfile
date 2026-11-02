FROM caddy:2.11.7-builder-alpine@sha256:95a5e7e2692b5eb9545aadc5b90a39f6f509bee59c9508a493f83ee902eaef63 AS builder

RUN xcaddy build \
    --with github.com/caddy-dns/cloudflare \
    --replace golang.org/x/crypto=golang.org/x/crypto@v0.56.0 \
    --replace golang.org/x/net=golang.org/x/net@v0.56.0 \
    --replace golang.org/x/text=golang.org/x/text@v0.39.0 \
    --replace google.golang.org/grpc=google.golang.org/grpc@v1.83.2 \
    --replace go.opentelemetry.io/otel/exporters/otlp/otlplog/otlploggrpc=go.opentelemetry.io/otel/exporters/otlp/otlplog/otlploggrpc@v0.21.0 \
    --replace go.opentelemetry.io/otel/sdk/log=go.opentelemetry.io/otel/sdk/log@v0.21.0 \
    --replace go.opentelemetry.io/otel/exporters/otlp/otlplog/otlploghttp=go.opentelemetry.io/otel/exporters/otlp/otlplog/otlploghttp@v0.21.0 \
    --replace go.opentelemetry.io/otel/exporters/stdout/stdoutlog=go.opentelemetry.io/otel/exporters/stdout/stdoutlog@v0.21.0 \
    --replace go.opentelemetry.io/otel/exporters/otlp/otlptrace=go.opentelemetry.io/otel/exporters/otlp/otlptrace@v1.45.0 \
    --replace go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc=go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc@v1.45.0 \
    --replace go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracehttp=go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracehttp@v1.45.0 \
    --replace go.opentelemetry.io/otel/sdk=go.opentelemetry.io/otel/sdk@v1.45.0

FROM caddy:2.11.7-alpine@sha256:eb4d35b7452d1ab6171fbdbe8d9275b2301dad6dad92115d91e19fb9b24d510f
COPY --from=builder /usr/bin/caddy /usr/bin/caddy
RUN apk upgrade --no-cache brotli-libs && apk version -t "$(apk info -e -v brotli-libs | sed 's/^brotli-libs-//')" 1.2.1-r0 | grep -q "[=>]"
RUN apk upgrade --no-cache curl libcurl && apk version -t "$(apk info -e -v curl | sed 's/^curl-//')" 8.23.0-r0 | grep -q "[=>]"
RUN caddy list-modules | grep -qx dns.providers.cloudflare && caddy validate --config /etc/caddy/Caddyfile
