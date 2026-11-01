FROM docker.io/democraticcsi/democratic-csi:v1.9.5@sha256:fc3b7d7ed3a616714139525075312758e23a5d425ffb539ad12c9bd20fb6001f AS upstream

FROM docker.io/library/busybox:1.38.0@sha256:a46257e06c82ad85242d0d38077d0a52d9896389ca60a1a59d3463db586e5fa4 AS patch
COPY --from=upstream /home/csi/app/src/utils/nvmeof.js /src/src/utils/nvmeof.js
COPY nvme-native-multipath.patch /src/
RUN cd /src && patch -p1 < nvme-native-multipath.patch

FROM upstream
COPY --from=patch /src/src/utils/nvmeof.js /home/csi/app/src/utils/nvmeof.js
RUN node --check /home/csi/app/src/utils/nvmeof.js && grep -qF 'nvme\d+c\d+n\d+' /home/csi/app/src/utils/nvmeof.js
