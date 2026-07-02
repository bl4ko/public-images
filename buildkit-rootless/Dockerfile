FROM moby/buildkit:v0.31.1-rootless@sha256:f5a131a50b4dd414d1847904603d0f19a28ddd413f43a703c5c657196ca429a0

USER root
RUN chmod u+s /usr/bin/newuidmap /usr/bin/newgidmap
USER 1000:1000
