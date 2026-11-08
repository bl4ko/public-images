FROM docker.io/clickhouse/clickhouse-server:26.8.36.3@sha256:d426d2f612e1e189c90f9a3f8dcd85a03c6851273833817a296bb327e63aa7a7 AS clickhouse

FROM gcr.io/distroless/base-nossl-debian13:nonroot@sha256:e6b0f82595aa1edde72c8054de7a9dca30f0f8f54820c37a167c0ddfc21c5c67 AS base

FROM --platform=$BUILDPLATFORM docker.io/library/golang:1.27.3-trixie@sha256:af1bfc2614ebb1dbd31af514b78740fd678850e76498762b425ecee4c5c6ea80 AS build
ARG TARGETARCH
WORKDIR /src
COPY go.mod main.go ./
RUN CGO_ENABLED=0 GOOS=linux GOARCH=$TARGETARCH go build -trimpath -ldflags='-s -w' -o /out/usr/local/bin/clickhouse .
COPY --from=base /etc/passwd /etc/group /out/etc/
COPY --from=clickhouse /etc/clickhouse-server/config.xml /etc/clickhouse-server/users.xml /out/etc/clickhouse-server/
COPY --from=clickhouse /etc/clickhouse-client/config.xml /out/etc/clickhouse-client/
RUN echo 'clickhouse:x:101:101:clickhouse:/var/lib/clickhouse:/sbin/nologin' >> /out/etc/passwd \
 && echo 'clickhouse:x:101:' >> /out/etc/group \
 && mkdir -p /out/etc/clickhouse-server/config.d /out/etc/clickhouse-server/users.d /out/var/lib/clickhouse \
 && chown 0:0 /out/etc/clickhouse-server/*.xml && chmod 0644 /out/etc/clickhouse-server/*.xml /out/etc/clickhouse-client/config.xml \
 && chown 101:101 /out/var/lib/clickhouse && chmod 0750 /out/var/lib/clickhouse

FROM base
LABEL org.opencontainers.image.source=https://github.com/bl4ko/public-images
COPY --from=build /out/ /
COPY --from=clickhouse /usr/bin/clickhouse /usr/bin/clickhouse
ENV TZ=UTC
USER 101:101
WORKDIR /
EXPOSE 8123 9000
VOLUME /var/lib/clickhouse
ENTRYPOINT ["/usr/local/bin/clickhouse"]
CMD ["server", "--config-file=/etc/clickhouse-server/config.xml"]
