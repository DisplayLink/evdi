#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2023 DisplayLink (UK) Ltd.

evdi_version='1.14.3'

EVDI_DIR=$(dirname "${BASH_SOURCE[0]}")
EVDI_REBOOT_RATIONALE=

if [[ -f /sys/devices/evdi/version ]]; then
  EVDI_REBOOT_RATIONALE="Another version of EVDI was loaded before the installation."
fi

copy_evdi_make_log()
{
  local {dkms,evdi_make}_log_path
  dkms_log_path=$(find "/var/lib/dkms/evdi/$evdi_version" -type f -name make.log)

  if [[ -f $dkms_log_path ]]; then
    evdi_make_log_path="/var/log/displaylink/evdi_install_make.$(date '+%F-%H%M').log"
    mkdir -p /var/log/displaylink
    cp "$dkms_log_path" "$evdi_make_log_path"
  fi
}

enroll_secureboot_key()
{
  if command -v mokutil >/dev/null && mokutil --sb-state | grep -i "SecureBoot enabled" > /dev/null; then
    update-secureboot-policy --enroll-key 2> /dev/null || return

    if [[ -z $EVDI_REBOOT_RATIONALE && $(mokutil --list-new | wc -l) -gt 0 ]]; then
      EVDI_REBOOT_RATIONALE="SecureBoot key was enrolled during the installation."
    fi
  fi
  return 0
}

error()
{
  echo >&2 "ERROR: $*"
}

evdi_dkms_install()
{
  if dkms status "evdi/$evdi_version" | grep installed &> /dev/null; then
    echo "Removing old evdi/$evdi_version module."
    dkms remove "evdi/$evdi_version"
  fi
  dkms install "$EVDI_DIR"
  local retval=$?

  if [[ $retval == 3 ]]; then
    echo "EVDI DKMS module already installed."
  elif [[ $retval != 0 ]]; then
    copy_evdi_make_log
    dkms remove "evdi/$evdi_version" --all
    error "Failed to install evdi to the kernel tree."
    return 1
  fi

  if ! enroll_secureboot_key; then
    error "Failed to enroll SecureBoot key."
    return 1
  fi

  evdi_requires_reboot || reboot_if_xorg_or_tty_running
}

evdi_add_mod_options()
{
  local module_file='/etc/modules-load.d/evdi.conf'
  echo 'evdi' > "$module_file"

  local conf_file='/etc/modprobe.d/evdi.conf'
  [[ -f $conf_file ]] && return

  echo "options evdi initial_device_count=4" > "$conf_file"

  local drm_deps
  drm_deps=$(sed -n '/^drm_kms_helper/p' /proc/modules | awk '{print $4}' | tr ',' ' ')
  drm_deps=${drm_deps/evdi/}
  [[ -z $drm_deps ]] && return 0

  echo "softdep evdi pre: $drm_deps" >> "$conf_file"
}

function notify-send2()
{
  local user uid program_path

  command -v logname >/dev/null \
    && user=$(logname) \
    && [[ $user ]] \
    && uid=$(id -u "$user") \
    && program_path=$(command -v notify-send) \
    && sudo -u "$user" "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$uid/bus" "$program_path" "$@" \
    || return 0
}

function reboot_if_xorg_or_tty_running()
{
  local session=${XDG_SESSION_TYPE-}
  if [[ -z $session ]]; then
    local session_id=${XDG_SESSION_ID-}
    if [[ -z $session_id ]]; then
      local user
      command -v logname >/dev/null \
        && user=$(logname) \
        && [[ -n $user ]] \
        && session_id=$(loginctl | awk "/$user/ {print \$1; exit}") \
        && [[ -n $session_id ]] \
        || return 0
    fi
    session=$(loginctl show-session "$session_id" -p Type)
    session=${session#*=}
  fi
  case $session in
    x11|tty)
      EVDI_REBOOT_RATIONALE="Detected user session type is: $session."
      ;;
  esac
}

function evdi_requires_reboot()
{
  [[ -n $EVDI_REBOOT_RATIONALE ]]
}

function evdi_success_message()
{
  printf '\n%s\n\n' "DisplayLink evdi module installed successfully."

  if evdi_requires_reboot; then
    notify-send2 -a "DisplayLinkManager" "Reboot required" \
          "DisplayLink evdi module installed successfully. $EVDI_REBOOT_RATIONALE Reboot your computer to ensure proper functioning of the software."

    if [[ -f /usr/share/update-notifier/notify-reboot-required ]]; then
      /usr/share/update-notifier/notify-reboot-required
    fi

    echo " Reboot required!"
    echo " $EVDI_REBOOT_RATIONALE"
    echo " Please, reboot your computer to ensure proper functioning of the software."
    echo
  fi
}

# if the script is NOT sourced
if ! (return 0 2>/dev/null); then
  set -e
  evdi_dkms_install
  evdi_add_mod_options
  evdi_success_message
fi
