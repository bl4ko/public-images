FROM moby/buildkit:v0.33.0-rootless@sha256:80b15f0735e87bab7bf59ec4d695dfb4a7cfb25521cf56dc75d6f256285b63ef

USER root
RUN chmod u+s /usr/bin/newuidmap /usr/bin/newgidmap
USER 1000:1000
