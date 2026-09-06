FROM golang:1.27.0-bookworm@sha256:ded31c68586d2e49e760acc2e65a884b23d032e9bbbed0ae0c55abd3fcaf4452 AS builder

WORKDIR /src
COPY go.mod main.go main_test.go ./
RUN go test ./... && CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /jellyfin-exporter .

FROM scratch
COPY --from=builder /jellyfin-exporter /jellyfin-exporter
USER 65532:65532
EXPOSE 9594
ENTRYPOINT ["/jellyfin-exporter"]
