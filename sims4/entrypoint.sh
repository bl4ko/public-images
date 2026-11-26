#!/usr/bin/env bash
set -euo pipefail

export HOME=/data/home
export XDG_RUNTIME_DIR=/tmp/runtime
export XDG_CONFIG_HOME=/data/config
export XDG_CACHE_HOME=/data/cache
export __GL_SHADER_DISK_CACHE_PATH=/data/cache/nvidia
export __GL_SHADER_DISK_CACHE_SKIP_CLEANUP=1
export PULSE_SERVER="unix:${XDG_RUNTIME_DIR}/pulse/native"
export PROTON=/opt/proton/current
export STEAM_COMPAT_DATA_PATH=/data/proton
export STEAM_COMPAT_CLIENT_INSTALL_PATH=/data/steam-stub

mkdir -p "${HOME}" "${XDG_RUNTIME_DIR}" "${XDG_CONFIG_HOME}" "${XDG_CACHE_HOME}" "${__GL_SHADER_DISK_CACHE_PATH}" \
    /data/sunshine/credentials "${STEAM_COMPAT_DATA_PATH}" "${STEAM_COMPAT_CLIENT_INSTALL_PATH}" /data/installers /data/screenshots
rm -rf "${XDG_RUNTIME_DIR:?}"/* /tmp/.X11-unix /tmp/.X[0-9]*-lock
chmod 700 "${XDG_RUNTIME_DIR}"

if [[ -d /dev/host-input && -w /dev/input ]]; then
    find /dev/input -mindepth 1 -maxdepth 1 -exec rm -rf {} +
fi

cp -f /opt/sims4/sunshine.conf /data/sunshine/sunshine.conf
cp -f /opt/sims4/apps.json /data/sunshine/apps.json

pulseaudio -n --daemonize=yes --use-pid-file=no --exit-idle-time=-1 --disallow-exit --log-target=stderr \
    --load="module-native-protocol-unix auth-anonymous=1 socket=${XDG_RUNTIME_DIR}/pulse/native" \
    --load="module-null-sink sink_name=sunshine-stereo sink_properties=device.description=SunshineStereo" \
    --load="module-always-sink"
pactl set-default-sink sunshine-stereo || true

export GBM_BACKENDS_PATH=/usr/lib64/gbm
export WLR_BACKENDS=headless,libinput
export WLR_LIBINPUT_NO_DEVICES=1
export LIBSEAT_BACKEND=noop
export SDL_JOYSTICK_DISABLE_UDEV=1
export WLR_NO_HARDWARE_CURSORS=1
export WLR_RENDER_DRM_DEVICE=/dev/dri/renderD128
export GBM_BACKEND=nvidia-drm
export __GLX_VENDOR_LIBRARY_NAME=nvidia
export VK_ICD_FILENAMES=/etc/vulkan/icd.d/nvidia_icd.json
if [[ -f /usr/share/vulkan/icd.d/nvidia_icd.x86_64.json ]]; then
    export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json
fi

LD_PRELOAD=/opt/sims4/lib/libsims4-input.so sway --unsupported-gpu -c /opt/sims4/sway.conf >/tmp/sway.log 2>&1 &
SWAY_PID=$!

SWAYSOCK=""
WAYLAND_SOCK=""
for _ in $(seq 1 50); do
    SWAYSOCK=$(find "${XDG_RUNTIME_DIR}" -maxdepth 1 -name 'sway-ipc.*.sock' 2>/dev/null | head -n1 || true)
    WAYLAND_SOCK=$(find "${XDG_RUNTIME_DIR}" -maxdepth 1 -name 'wayland-*' ! -name '*.lock' 2>/dev/null | head -n1 || true)
    if [[ -n "${SWAYSOCK}" && -n "${WAYLAND_SOCK}" ]]; then
        break
    fi
    sleep 0.2
done

if [[ -z "${SWAYSOCK}" || -z "${WAYLAND_SOCK}" ]]; then
    echo "Sway failed to start:" >&2
    cat /tmp/sway.log >&2 || true
    exit 1
fi

export SWAYSOCK
export WAYLAND_DISPLAY="$(basename "${WAYLAND_SOCK}")"
export DISPLAY=:0

cat > "${XDG_RUNTIME_DIR}/session.env" <<ENV
export HOME="${HOME}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR}"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME}"
export __GL_SHADER_DISK_CACHE_PATH="${__GL_SHADER_DISK_CACHE_PATH}"
export __GL_SHADER_DISK_CACHE_SKIP_CLEANUP=1
export SWAYSOCK="${SWAYSOCK}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY}"
export DISPLAY="${DISPLAY}"
export PULSE_SERVER="${PULSE_SERVER}"
export VK_ICD_FILENAMES="${VK_ICD_FILENAMES}"
export __GLX_VENDOR_LIBRARY_NAME="${__GLX_VENDOR_LIBRARY_NAME}"
export GBM_BACKENDS_PATH="${GBM_BACKENDS_PATH}"
export SDL_JOYSTICK_DISABLE_UDEV=1
export PROTON="${PROTON}"
export STEAM_COMPAT_DATA_PATH="${STEAM_COMPAT_DATA_PATH}"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_COMPAT_CLIENT_INSTALL_PATH}"
ENV

trap 'kill "${SWAY_PID}" 2>/dev/null || true; pulseaudio --kill 2>/dev/null || true' EXIT TERM INT

MAIN_PID=$$
(
    while [[ -r /proc/${SWAY_PID}/stat && "$(cut -d' ' -f3 "/proc/${SWAY_PID}/stat" 2>/dev/null)" != Z ]]; do sleep 2; done
    echo "sway exited, stopping sunshine" >&2
    kill -TERM "${MAIN_PID}"
) &

exec sunshine-nice -n -15 sunshine /data/sunshine/sunshine.conf
