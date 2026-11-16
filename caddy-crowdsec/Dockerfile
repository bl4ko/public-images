FROM caddy:2.11.9-builder-alpine@sha256:646fee578e680989ca5c7fd1ce6f1de574d19fd71ef2221810cc0510a9f40c86 AS builder

RUN xcaddy build \
    --with github.com/hslatman/caddy-crowdsec-bouncer/http@v0.14.1 \
    --with github.com/hslatman/caddy-crowdsec-bouncer/appsec@v0.14.1

FROM caddy:2.11.9-alpine@sha256:003a97808e781aa9f7e75cc2f43d34d461099031faf5e20faefb9f61126836f0
COPY --from=builder /usr/bin/caddy /usr/bin/caddy
RUN apk upgrade --no-cache brotli-libs && apk version -t "$(apk info -e -v brotli-libs | sed 's/^brotli-libs-//')" 1.2.1-r0 | grep -q "[=>]"
