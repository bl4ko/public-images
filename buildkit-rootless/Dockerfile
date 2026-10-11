FROM moby/buildkit:v0.34.0-rootless@sha256:65eca3c01cd1180c8f64565c4e3c5983af5145818b0e6fa863385d099eb73855

USER root
RUN chmod u+s /usr/bin/newuidmap /usr/bin/newgidmap
USER 1000:1000
