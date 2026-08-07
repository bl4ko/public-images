FROM moby/buildkit:v0.32.2-rootless@sha256:504731e577c20559c00f968f33219f30115e70be29ab96728d1d06e963fc494b

USER root
RUN chmod u+s /usr/bin/newuidmap /usr/bin/newgidmap
USER 1000:1000
