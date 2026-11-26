FROM --platform=$BUILDPLATFORM docker.io/library/alpine:3.22@sha256:49340b346798875b5592e5d420241f2f9cfba5f47e62ebb1446a1b34734e898f AS proton
ARG PROTON_VERSION=GE-Proton11-14
ARG PROTON_SHA512=047fe613ad000f6fac0b6795350680edc7dd78790ee1a588bb8ff2705317186acbe43736fc7a8dbf8dabc59ed64d8177a35c5f713d0104d25d40c08b318960de
RUN apk add --no-cache curl \
    && curl -fsSL "https://github.com/GloriousEggroll/proton-ge-custom/releases/download/${PROTON_VERSION}/${PROTON_VERSION}-x86_64.tar.gz" -o /tmp/proton.tar.gz \
    && echo "${PROTON_SHA512}  /tmp/proton.tar.gz" | sha512sum -c - \
    && mkdir -p /opt/proton \
    && tar -xzf /tmp/proton.tar.gz -C /opt/proton \
    && mv "/opt/proton/${PROTON_VERSION}-x86_64" /opt/proton/current \
    && rm /tmp/proton.tar.gz

FROM docker.io/library/ubuntu:24.04@sha256:73b999393d07f2bf65a40373778a44dc9a6d54ad7718d29247d8ff9413c4d6a8 AS native
ENV DEBIAN_FRONTEND=noninteractive
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources \
    && apt-get update && apt-get install -y --no-install-recommends dpkg-dev build-essential fakeroot \
    && apt-get build-dep -y wlroots=0.17.1-2.1build1 \
    && mkdir /build && cd /build && apt-get source wlroots=0.17.1-2.1build1
COPY headless-frame-clock.patch /build/
RUN cd /build/wlroots-0.17.1 && cp ../headless-frame-clock.patch debian/patches/ \
    && echo headless-frame-clock.patch >> debian/patches/series \
    && DEB_BUILD_OPTIONS="nocheck nodoc parallel=4" dpkg-buildpackage -b -uc -us
COPY sims4-input.c /build/
RUN gcc -O2 -Wall -Wextra -Werror -Wno-format-truncation -fPIC -shared -DSIMS4_UINPUT -o /build/libsims4-uinput.so /build/sims4-input.c -ldl -lpthread \
    && gcc -O2 -Wall -Wextra -Werror -Wno-format-truncation -fPIC -shared -o /build/libsims4-input.so /build/sims4-input.c -ldl -lpthread

FROM docker.io/library/ubuntu:24.04@sha256:73b999393d07f2bf65a40373778a44dc9a6d54ad7718d29247d8ff9413c4d6a8

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    sway \
    xwayland \
    wlr-randr \
    grim \
    libvulkan1 \
    mesa-vulkan-drivers \
    libegl1 \
    libgbm1 \
    libgl1 \
    libgles2 \
    vulkan-tools \
    mesa-utils \
    mangohud \
    pulseaudio \
    pulseaudio-utils \
    libasound2-plugins \
    python3 \
    fonts-dejavu-core \
    procps \
    iproute2 \
    libudev1 \
    jq \
    tini \
    libx11-6 \
    libx11-xcb1 \
    libxext6 \
    libxrandr2 \
    libxi6 \
    libxcursor1 \
    libxcomposite1 \
    libxfixes3 \
    libxrender1 \
    libxinerama1 \
    libxxf86vm1 \
    libxkbcommon0 \
    libfreetype6 \
    libfontconfig1 \
    libgnutls30t64 \
    libdbus-1-3 \
    libpulse0 \
    libasound2t64 \
    libglib2.0-0t64 \
    libgstreamer1.0-0 \
    libgstreamer-plugins-base1.0-0 \
    gstreamer1.0-plugins-base \
    libsdl2-2.0-0 \
    libusb-1.0-0 \
    libunwind8 \
    libwayland-client0 \
    libwayland-egl1 \
    libkrb5-3 \
    libgssapi-krb5-2 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

ARG SUNSHINE_VERSION=2026.914.233613
ARG SUNSHINE_SHA256=c38e9c705f650f8705f61702717e99bec34044c5028fcb8f23a08fe041292c21
RUN curl -fsSL "https://github.com/LizardByte/Sunshine/releases/download/v${SUNSHINE_VERSION}/sunshine_${SUNSHINE_VERSION}-1+ubuntu24.04_amd64.deb" -o /tmp/sunshine.deb \
    && echo "${SUNSHINE_SHA256}  /tmp/sunshine.deb" | sha256sum -c - \
    && apt-get update && apt-get install -y --no-install-recommends /tmp/sunshine.deb \
    && rm -f /tmp/sunshine.deb \
    && setcap cap_sys_nice+ep /usr/bin/sunshine \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /etc/vulkan/icd.d /usr/share/glvnd/egl_vendor.d /usr/share/egl/egl_external_platform.d \
    && printf '{"file_format_version":"1.0.1","ICD":{"library_path":"libGLX_nvidia.so.0","api_version":"1.4.357"}}\n' > /etc/vulkan/icd.d/nvidia_icd.json \
    && printf '{"file_format_version":"1.0.0","ICD":{"library_path":"libEGL_nvidia.so.0"}}\n' > /usr/share/glvnd/egl_vendor.d/10_nvidia.json \
    && printf '{"file_format_version":"1.0.0","ICD":{"library_path":"libnvidia-egl-gbm.so.1"}}\n' > /usr/share/egl/egl_external_platform.d/15_nvidia_gbm.json \
    && printf '{"file_format_version":"1.0.0","ICD":{"library_path":"libnvidia-egl-wayland.so.1"}}\n' > /usr/share/egl/egl_external_platform.d/10_nvidia_wayland.json \
    && (id -u ubuntu >/dev/null 2>&1 && userdel -r ubuntu || true) \
    && groupadd -g 1000 gamer \
    && useradd -u 1000 -g 1000 -m -s /bin/bash gamer \
    && mkdir -p /data /tmp/runtime /opt/sims4/bin \
    && chown -R 1000:1000 /data /tmp/runtime

COPY --from=native /build/libwlroots12t64_0.17.1-2.1build1_amd64.deb /tmp/
RUN dpkg -i /tmp/libwlroots12t64_0.17.1-2.1build1_amd64.deb && rm /tmp/libwlroots12t64_0.17.1-2.1build1_amd64.deb \
    && apt-mark hold libwlroots12t64
COPY --from=native /build/libsims4-uinput.so /build/libsims4-input.so /opt/sims4/lib/
RUN echo /opt/sims4/lib/libsims4-uinput.so > /etc/ld.so.preload

COPY --from=proton --chown=1000:1000 /opt/proton /opt/proton
COPY sway.conf sunshine.conf apps.json dxvk.conf /opt/sims4/
COPY --chmod=755 entrypoint.sh sims4-run /opt/sims4/bin/
RUN install -m 0755 /usr/bin/nice /opt/sims4/bin/sunshine-nice \
    && setcap cap_sys_nice+ep /opt/sims4/bin/sunshine-nice

ENV PATH="/opt/sims4/bin:${PATH}"

USER 1000:1000
WORKDIR /home/gamer

EXPOSE 47984/tcp 47989/tcp 47990/tcp 48010/tcp 47998/udp 47999/udp 48000/udp 48002/udp 48010/udp

ENTRYPOINT ["/usr/bin/tini", "-g", "--", "/opt/sims4/bin/entrypoint.sh"]
