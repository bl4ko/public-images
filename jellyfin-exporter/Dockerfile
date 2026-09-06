FROM --platform=$BUILDPLATFORM golang:1.27.1-bookworm@sha256:648f440f42a0958804efb24df176f806f9d353b41f1c0627f666428e40310f6b AS builder

WORKDIR /src
COPY go.mod main.go main_test.go ./
ARG TARGETOS TARGETARCH
RUN go test ./... && CGO_ENABLED=0 GOOS=$TARGETOS GOARCH=$TARGETARCH go build -trimpath -ldflags="-s -w" -o /jellyfin-exporter .

FROM scratch
COPY --from=builder /jellyfin-exporter /jellyfin-exporter
USER 65532:65532
EXPOSE 9594
ENTRYPOINT ["/jellyfin-exporter"]
