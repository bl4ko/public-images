FROM caddy:2.11.9-builder-alpine@sha256:646fee578e680989ca5c7fd1ce6f1de574d19fd71ef2221810cc0510a9f40c86 AS builder

RUN xcaddy build \
    --with github.com/hslatman/caddy-crowdsec-bouncer/http@v0.14.1 \
    --with github.com/hslatman/caddy-crowdsec-bouncer/appsec@v0.14.1

FROM caddy:2.11.9-alpine@sha256:0bbb055bda6793c999bde78b1b5ce3e89f829fb031af2cf50056ea1246559fee
COPY --from=builder /usr/bin/caddy /usr/bin/caddy
RUN apk upgrade --no-cache brotli-libs && apk version -t "$(apk info -e -v brotli-libs | sed 's/^brotli-libs-//')" 1.2.1-r0 | grep -q "[=>]"
