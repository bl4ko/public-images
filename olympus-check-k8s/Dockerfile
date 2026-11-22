FROM ghcr.io/kyverno/kyverno-cli:v1.19.2@sha256:322ec5044d600930cb3af0e56dc33154a2d1d1fba58f4cadad3f47be7e78b11c AS kyverno

FROM docker.io/alpine/k8s:1.35.10@sha256:6b9b607247ab8f0d0bd2d6173a0b842b4d173c652475c9a715eec0c7ef27eac5
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
