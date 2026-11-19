FROM moby/buildkit:v0.35.0-rootless@sha256:5987568f92dafbfb679879de59c1cd8d26597f5f6635c314e166164e2615870c

USER root
RUN apk upgrade --no-cache && \
    rm -f /usr/bin/buildkit-cni-* && \
    chmod u+s /usr/bin/newuidmap /usr/bin/newgidmap
RUN apk upgrade --no-cache && apk version -t "$(apk info -e -v libssl3 | sed 's/^libssl3-//')" 3.5.9-r1 | grep -q "[=>]" && apk version -t "$(apk info -e -v libcrypto3 | sed 's/^libcrypto3-//')" 3.5.9-r1 | grep -q "[=>]"
USER 1000:1000
