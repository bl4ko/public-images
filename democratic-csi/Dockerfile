FROM docker.io/democraticcsi/democratic-csi:v1.9.5@sha256:fc3b7d7ed3a616714139525075312758e23a5d425ffb539ad12c9bd20fb6001f AS upstream

FROM docker.io/library/busybox:1.37.0@sha256:bdf57e528e45e4433820e045b29b4597825a1c9e38353532d90a01445013f82e AS patch
COPY --from=upstream /home/csi/app/src/utils/nvmeof.js /src/src/utils/nvmeof.js
COPY nvme-native-multipath.patch /src/
RUN cd /src && patch -p1 < nvme-native-multipath.patch

FROM upstream
COPY --from=patch /src/src/utils/nvmeof.js /home/csi/app/src/utils/nvmeof.js
RUN node --check /home/csi/app/src/utils/nvmeof.js && grep -qF 'nvme\d+c\d+n\d+' /home/csi/app/src/utils/nvmeof.js
