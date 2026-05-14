#!/usr/bin/env bash

qemu_dev_state_dir() {
    printf '%s' "${SLM_QEMU_STATE_DIR:-$HOME/.local/state/slm-qemu}"
}

qemu_dev_known_hosts() {
    printf '%s/known_hosts' "$(qemu_dev_state_dir)"
}

qemu_dev_identity_file() {
    printf '%s' "${SLM_QEMU_SSH_IDENTITY_FILE:-$(qemu_dev_state_dir)/id_ed25519}"
}

qemu_dev_ensure_state_dir() {
    mkdir -p "$(qemu_dev_state_dir)"
}

qemu_dev_ssh_usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-ssh.sh [--user USER] [--port PORT] [--tty] [ssh args...]

Defaults:
  user : ${SLM_QEMU_SSH_USER:-garis}
  port : ${SLM_QEMU_SSH_PORT:-2222}
  key  : ${SLM_QEMU_SSH_IDENTITY_FILE:-$(qemu_dev_state_dir)/id_ed25519}
EOF
}

qemu_dev_scp_usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-scp.sh [--user USER] [--port PORT] <src> <dst>

Defaults:
  user : ${SLM_QEMU_SSH_USER:-garis}
  port : ${SLM_QEMU_SSH_PORT:-2222}
  key  : ${SLM_QEMU_SSH_IDENTITY_FILE:-$(qemu_dev_state_dir)/id_ed25519}
EOF
}

qemu_dev_ssh_main() {
    local ssh_user="${SLM_QEMU_SSH_USER:-garis}"
    local ssh_port="${SLM_QEMU_SSH_PORT:-2222}"
    local force_tty=0
    local extra_args=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --user)
                ssh_user="$2"
                shift 2
                ;;
            --port)
                ssh_port="$2"
                shift 2
                ;;
            --tty|-t)
                force_tty=1
                shift
                ;;
            --help|-h)
                qemu_dev_ssh_usage
                return 0
                ;;
            *)
                extra_args+=("$1")
                shift
                ;;
        esac
    done

    qemu_dev_ensure_state_dir

    local identity_file
    identity_file="$(qemu_dev_identity_file)"
    local -a ssh_args=(
        -F /dev/null
        -o BatchMode=yes
        -o IdentitiesOnly=yes
        -o StrictHostKeyChecking=accept-new
        -o UserKnownHostsFile="$(qemu_dev_known_hosts)"
        -p "$ssh_port"
    )
    if [[ -f "$identity_file" ]]; then
        ssh_args+=(-i "$identity_file")
    fi
    ssh_args+=("$ssh_user@127.0.0.1")
    if [[ "$force_tty" == "1" ]]; then
        ssh_args=(-tt "${ssh_args[@]}")
    fi

    exec ssh "${ssh_args[@]}" "${extra_args[@]}"
}

qemu_dev_wait_ssh() {
    # Cek ketersediaan sshd di guest via ssh-keyscan (TCP+banner, tanpa auth).
    # Tidak bergantung pada SSH key atau password sehingga aman untuk semua setup.
    local ssh_port="${1:-${SLM_QEMU_SSH_PORT:-2222}}"
    local max_attempts="${2:-90}"

    local attempt=0
    while ((attempt < max_attempts)); do
        if ssh-keyscan -p "$ssh_port" -T 2 127.0.0.1 2>/dev/null | grep -q .; then
            return 0
        fi
        ((attempt++)) || true
        sleep 2
    done
    return 1
}

qemu_dev_scp_main() {
    local ssh_user="${SLM_QEMU_SSH_USER:-garis}"
    local ssh_port="${SLM_QEMU_SSH_PORT:-2222}"
    local positional=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --user)
                ssh_user="$2"
                shift 2
                ;;
            --port)
                ssh_port="$2"
                shift 2
                ;;
            --help|-h)
                qemu_dev_scp_usage
                return 0
                ;;
            *)
                positional+=("$1")
                shift
                ;;
        esac
    done

    if [[ "${#positional[@]}" -ne 2 ]]; then
        qemu_dev_scp_usage >&2
        return 1
    fi

    qemu_dev_ensure_state_dir

    local identity_file
    identity_file="$(qemu_dev_identity_file)"

    local -a scp_args=(
        -F /dev/null
        -o BatchMode=yes
        -o IdentitiesOnly=yes
        -o StrictHostKeyChecking=accept-new
        -o UserKnownHostsFile="$(qemu_dev_known_hosts)"
        -P "$ssh_port"
    )
    if [[ -f "$identity_file" ]]; then
        scp_args+=(-i "$identity_file")
    fi

    exec scp \
        "${scp_args[@]}" \
        "${positional[0]}" \
        "$ssh_user@127.0.0.1:${positional[1]}"
}
