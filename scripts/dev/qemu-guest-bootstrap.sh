#!/usr/bin/env bash
# scripts/dev/qemu-guest-bootstrap.sh — Bootstrap guest Ubuntu untuk development SLM.

set -euo pipefail

HOSTSHARE_PATH="${SLM_QEMU_HOSTSHARE_PATH:-/mnt/hostshare}"
ENABLE_UNIVERSE=1
RESET_UBUNTU_SOURCES=1
SUDO=(sudo)
CORE_APT_PACKAGES=(
    build-essential
    ccache
    cmake
    git
    libdbus-1-dev
    libglib2.0-dev
    libgtk-3-dev
    libpipewire-0.3-dev
    libpulse-dev
    libqt6svg6-dev
    libspa-0.2-dev
    libsystemd-dev
    libwayland-dev
    libxkbcommon-dev
    ninja-build
    openssh-server
    pkg-config
    spice-vdagent
    qml6-module-qtquick
    qml6-module-qtquick-controls
    qml6-module-qtquick-dialogs
    qml6-module-qtquick-layouts
    qml6-module-qtquick-window
    qml6-module-qt-labs-folderlistmodel
    qt6-base-dev
    qt6-base-private-dev
    qt6-declarative-dev
    qt6-declarative-private-dev
    qt6-shadertools-dev
    qt6-tools-dev
    qt6-tools-dev-tools
    qt6-wayland-dev
    wayland-protocols
)
OPTIONAL_APT_PACKAGES=(
    libarchive-dev
    libpam0g-dev
    libpolkit-agent-1-dev
    libpolkit-gobject-1-dev
    libpoppler-qt6-dev
    qml6-module-qt5compat
    sway
)
MISSING_OPTIONAL_PACKAGES=()

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-guest-bootstrap.sh [options]

Options:
  --hostshare PATH   Mount point for shared repo. Default: $HOSTSHARE_PATH
  --no-universe      Do not try to enable Ubuntu universe repo
  --no-reset-sources Do not rewrite Ubuntu apt sources
  --skip-apt         Skip apt install/update
  --skip-mount       Skip mounting hostshare
  --apt-only         Only install apt dependencies
  --mount-only       Only mount hostshare
  --help             Show this help
EOF
}

SKIP_APT=0
SKIP_MOUNT=0
APT_ONLY=0
MOUNT_ONLY=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --hostshare)
            HOSTSHARE_PATH="$2"
            shift 2
            ;;
        --no-universe)
            ENABLE_UNIVERSE=0
            shift
            ;;
        --no-reset-sources)
            RESET_UBUNTU_SOURCES=0
            shift
            ;;
        --skip-apt)
            SKIP_APT=1
            shift
            ;;
        --skip-mount)
            SKIP_MOUNT=1
            shift
            ;;
        --apt-only)
            APT_ONLY=1
            SKIP_MOUNT=1
            shift
            ;;
        --mount-only)
            MOUNT_ONLY=1
            SKIP_APT=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-guest-bootstrap] Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ "$APT_ONLY" == "1" && "$MOUNT_ONLY" == "1" ]]; then
    echo "[qemu-guest-bootstrap] ERROR: --apt-only dan --mount-only tidak bisa dipakai bersamaan." >&2
    exit 1
fi

collect_available_packages() {
    local package
    local -n requested_ref="$1"
    local -n available_ref="$2"
    local -n missing_ref="$3"

    for package in "${requested_ref[@]}"; do
        if apt-cache show "$package" >/dev/null 2>&1; then
            available_ref+=("$package")
        else
            missing_ref+=("$package")
        fi
    done
}

ensure_sudo() {
    echo "[qemu-guest-bootstrap] Validating sudo session"
    sudo -v
    SUDO=(sudo -n)
}

disable_cdrom_sources() {
    echo "[qemu-guest-bootstrap] Disabling installer cdrom apt sources"
    if [[ -f /etc/apt/sources.list.d/cdrom.sources ]]; then
        "${SUDO[@]}" mv /etc/apt/sources.list.d/cdrom.sources \
            /etc/apt/sources.list.d/cdrom.sources.disabled-by-qemu-bootstrap
        echo "  disabled: /etc/apt/sources.list.d/cdrom.sources"
    fi
    "${SUDO[@]}" find /etc/apt -type f \( -name '*.list' -o -name '*.sources' \) -print0 | while IFS= read -r -d '' file; do
        if "${SUDO[@]}" grep -qE 'cdrom:|file:/cdrom' "$file"; then
            "${SUDO[@]}" mv "$file" "${file}.disabled-by-qemu-bootstrap"
            echo "  disabled: $file"
        fi
    done
}

reset_ubuntu_sources() {
    local codename
    codename="$(. /etc/os-release && printf '%s' "${VERSION_CODENAME:-}")"
    if [[ -z "$codename" ]]; then
        echo "[qemu-guest-bootstrap] ERROR: gagal mendeteksi VERSION_CODENAME dari /etc/os-release" >&2
        return 1
    fi

    echo "[qemu-guest-bootstrap] Rewriting Ubuntu apt sources for $codename"
    "${SUDO[@]}" mkdir -p /etc/apt/sources.list.d
    "${SUDO[@]}" rm -f /etc/apt/sources.list
    "${SUDO[@]}" tee /etc/apt/sources.list.d/ubuntu.sources >/dev/null <<EOF
Types: deb
URIs: http://archive.ubuntu.com/ubuntu
Suites: ${codename} ${codename}-updates ${codename}-backports
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu
Suites: ${codename}-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
}

install_packages() {
    local package
    local -n packages_ref="$1"
    local failures=()

    if [[ "${#packages_ref[@]}" -eq 0 ]]; then
        return 0
    fi

    echo "[qemu-guest-bootstrap] Installing package batch:"
    printf '  - %s\n' "${packages_ref[@]}"
    if "${SUDO[@]}" apt install -y "${packages_ref[@]}"; then
        return 0
    fi

    echo "[qemu-guest-bootstrap] Batch install gagal, retry per package"
    for package in "${packages_ref[@]}"; do
        echo "  -> $package"
        if ! "${SUDO[@]}" apt install -y "$package"; then
            failures+=("$package")
        fi
    done

    if [[ "${#failures[@]}" -gt 0 ]]; then
        echo "[qemu-guest-bootstrap] ERROR: paket berikut gagal diinstal:" >&2
        printf '  - %s\n' "${failures[@]}" >&2
        return 1
    fi
}

if [[ "$SKIP_MOUNT" != "1" || "$SKIP_APT" != "1" ]]; then
    ensure_sudo
fi

if [[ "$SKIP_MOUNT" != "1" ]]; then
    echo "[qemu-guest-bootstrap] Mounting hostshare at $HOSTSHARE_PATH"
    "${SUDO[@]}" mkdir -p "$HOSTSHARE_PATH"
    if mountpoint -q "$HOSTSHARE_PATH"; then
        echo "[qemu-guest-bootstrap] hostshare already mounted"
    else
        "${SUDO[@]}" mount -t 9p -o trans=virtio,version=9p2000.L hostshare "$HOSTSHARE_PATH"
    fi
fi

if [[ "$SKIP_APT" != "1" ]]; then
    echo "[qemu-guest-bootstrap] Installing Ubuntu packages"
    disable_cdrom_sources
    if [[ "$RESET_UBUNTU_SOURCES" == "1" ]]; then
        reset_ubuntu_sources
    fi
    "${SUDO[@]}" apt update

    AVAILABLE_CORE_PACKAGES=()
    MISSING_CORE_PACKAGES=()
    AVAILABLE_OPTIONAL_PACKAGES=()
    MISSING_OPTIONAL_PACKAGES=()
    collect_available_packages CORE_APT_PACKAGES AVAILABLE_CORE_PACKAGES MISSING_CORE_PACKAGES
    collect_available_packages OPTIONAL_APT_PACKAGES AVAILABLE_OPTIONAL_PACKAGES MISSING_OPTIONAL_PACKAGES

    if [[ "${#MISSING_CORE_PACKAGES[@]}" -gt 0 ]]; then
        echo "[qemu-guest-bootstrap] ERROR: paket core tidak tersedia di apt:" >&2
        printf '  - %s\n' "${MISSING_CORE_PACKAGES[@]}" >&2
        exit 1
    fi

    install_packages AVAILABLE_CORE_PACKAGES
    if [[ "${#AVAILABLE_OPTIONAL_PACKAGES[@]}" -gt 0 ]]; then
        install_packages AVAILABLE_OPTIONAL_PACKAGES || true
    fi
    "${SUDO[@]}" systemctl enable --now ssh
fi

echo ""
echo "[qemu-guest-bootstrap] Host repo tersedia di:"
echo "  $HOSTSHARE_PATH"
if [[ "${#MISSING_OPTIONAL_PACKAGES[@]}" -gt 0 ]]; then
    echo ""
    echo "[qemu-guest-bootstrap] Optional packages tidak tersedia dan dilewati:"
    printf '  - %s\n' "${MISSING_OPTIONAL_PACKAGES[@]}"
fi
echo ""
echo "[qemu-guest-bootstrap] Contoh langkah berikutnya:"
echo "  cd $HOSTSHARE_PATH"
echo "  cmake -S . -B ../build/dev -G Ninja -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build ../build/dev --target appSlm_Desktop -j\$(nproc)"
