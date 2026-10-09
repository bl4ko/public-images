FROM caddy:2.11.6-builder-alpine@sha256:096ec6e825e219175bd5be64b5e7a4000977f701a2bcaab825621d9fb1e1154b AS builder

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

FROM caddy:2.11.7-alpine@sha256:d8542f48d34a9cf4e4c11a478865229840e87e4c96ea3f439101f31a5d35f75f
COPY --from=builder /usr/bin/caddy /usr/bin/caddy
RUN caddy list-modules | grep -qx dns.providers.cloudflare && caddy validate --config /etc/caddy/Caddyfile
