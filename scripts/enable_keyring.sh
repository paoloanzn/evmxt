#!/usr/bin/env bash
# This prepares a Linux VM terminal to use lua-lib/wallet.lua safely.
#
# On a desktop, Linux usually opens the box for you when you sign
# in.  On a server or an SSH terminal, that automatic step is often missing.
# Then evmxt cannot find the keyring and reports that Secret Service is
# unavailable or access is denied.
#
# Run this before evmxt when you first connect to the VM, or whenever evmxt
# reports a keyring error:
#
#   source scripts/enable_keyring.sh --unlock
#   ./build/evmxt
#
# `source` is important: it runs these commands *in this terminal*, so the
# terminal remembers where the keyring service lives.  Running
# `scripts/enable_keyring.sh` by itself starts a separate short-lived shell;
# that shell forgets the setting as soon as it exits.
#
# --unlock asks for the password of ~/.local/share/keyrings/login.keyring and
# does not show what you type.  --check only tells you whether the keyring is
# already ready; it does not change anything.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "enable_keyring: source this file so its exports apply here:" >&2
    echo "  source scripts/enable_keyring.sh --unlock" >&2
    exit 2
fi

_keyring_fail() {
    echo "enable_keyring: $*" >&2
    return 1
}

_keyring_usage() {
    cat >&2 <<'EOF'
Usage: source scripts/enable_keyring.sh [--unlock|--check]

  --unlock  unlock the existing login keyring, then enable Secret Service.
  --check   make no changes; report whether Secret Service is usable.
EOF
}

case "${1:-}" in
    ""|--unlock|--check) ;;
    -h|--help) _keyring_usage; return 0 ;;
    *) _keyring_usage; _keyring_fail "unknown option: $1"; return 2 ;;
esac

command -v gnome-keyring-daemon >/dev/null 2>&1 || {
    _keyring_fail "gnome-keyring-daemon is missing; install gnome-keyring"; return 1;
}
command -v busctl >/dev/null 2>&1 || {
    _keyring_fail "busctl is missing; install systemd user-session tools"; return 1;
}

_keyring_uid="$(id -u)"
_keyring_runtime="/run/user/${_keyring_uid}"
_keyring_bus="unix:path=${_keyring_runtime}/bus"

# SSH shells often omit these even though systemd has created the user bus.
# Exporting them here is why this file must be sourced, not executed.
if ! timeout 5 busctl --address="${_keyring_bus}" list >/dev/null 2>&1; then
    _keyring_fail "no user D-Bus at ${_keyring_runtime}/bus; log in with a systemd user session"
    return 1
fi
export XDG_RUNTIME_DIR="${_keyring_runtime}"
export DBUS_SESSION_BUS_ADDRESS="${_keyring_bus}"

_keyring_collection_locked() {
    local collection locked
    collection="$(timeout 5 busctl --address="${DBUS_SESSION_BUS_ADDRESS}" \
        call org.freedesktop.secrets /org/freedesktop/secrets \
        org.freedesktop.Secret.Service ReadAlias s default 2>/dev/null \
        | sed -n 's/^o "\(.*\)"$/\1/p')" || return 1

    # A newly created account has no default collection until its first store.
    [[ -z "${collection}" || "${collection}" == "/" ]] && return 1

    locked="$(timeout 5 busctl --address="${DBUS_SESSION_BUS_ADDRESS}" \
        get-property org.freedesktop.secrets "${collection}" \
        org.freedesktop.Secret.Collection Locked 2>/dev/null)" || return 1
    [[ "${locked}" == "b true" ]]
}

if [[ "${1:-}" == "--check" ]]; then
    if ! timeout 5 busctl --address="${DBUS_SESSION_BUS_ADDRESS}" \
        status org.freedesktop.secrets >/dev/null 2>&1; then
        _keyring_fail "Secret Service is not running"
        return 1
    fi
    if _keyring_collection_locked; then
        _keyring_fail "the default keyring is locked; source this file with --unlock"
        return 1
    fi
    echo "enable_keyring: Secret Service is ready" >&2
    return 0
fi

if [[ "${1:-}" == "--unlock" ]]; then
    read -r -s -p "Keyring password: " _keyring_password
    echo >&2

    # An already activated daemon ignores a second --unlock request. Stop its
    # unit first, matching the recovery sequence used on this VM.
    systemctl --user stop gnome-keyring-daemon.service >/dev/null 2>&1 || true
    printf '%s' "${_keyring_password}" | gnome-keyring-daemon --unlock --components=secrets >/dev/null
    _keyring_unlock_status=$?
    unset _keyring_password
    if [[ ${_keyring_unlock_status} -ne 0 ]]; then
        _keyring_fail "could not unlock login.keyring"
        return 1
    fi
fi

# Start is idempotent and also initializes a daemon started by --unlock.
eval "$(gnome-keyring-daemon --start --components=secrets)" || {
    _keyring_fail "could not start Secret Service"; return 1;
}

if ! timeout 5 busctl --address="${DBUS_SESSION_BUS_ADDRESS}" \
    introspect org.freedesktop.secrets /org/freedesktop/secrets >/dev/null 2>&1; then
    _keyring_fail "Secret Service did not become available"
    return 1
fi
if _keyring_collection_locked; then
    _keyring_fail "the default keyring remains locked; verify the password"
    return 1
fi

echo "enable_keyring: Secret Service is ready" >&2
unset _keyring_uid _keyring_runtime _keyring_bus _keyring_unlock_status
