FROM ghcr.io/kyverno/kyverno-cli:v1.19.2@sha256:322ec5044d600930cb3af0e56dc33154a2d1d1fba58f4cadad3f47be7e78b11c AS kyverno

FROM docker.io/alpine/k8s:1.37.2@sha256:7a82aeefea3d554a43a8676b137f2b88bae6c1fe11c13862b191b3a086cc28c0
LABEL org.opencontainers.image.source=https://github.com/bl4ko/public-images
RUN apk add --no-cache python3
COPY --from=kyverno /ko-app/kubectl-kyverno /usr/local/bin/kyverno
COPY lib/ /usr/local/lib/check-manifests/
COPY --chmod=755 check-manifests /usr/local/bin/check-manifests
RUN kyverno version \
 && kustomize version \
 && kubeconform -v \
 && helm version --short \
 && python3 -m py_compile /usr/local/lib/check-manifests/plan.py /usr/local/lib/check-manifests/split.py
USER 1000:1000
ENV HOME=/tmp
WORKDIR /tmp
