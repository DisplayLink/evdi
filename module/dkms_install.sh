#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2023 DisplayLink (UK) Ltd.

evdi_version='1.14.1'

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
  if mokutil --sb-state | grep -i "SecureBoot enabled" > /dev/null; then
    local results
    results=$(update-secureboot-policy --enroll-key 2> /dev/null) || return

    if [[ -z $EVDI_REBOOT_RATIONALE && ! $results == *"Nothing to do."* ]]; then
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
  dkms install "$EVDI_DIR"
  local retval=$?

  if ! enroll_secureboot_key; then
    error "Failed to enroll SecureBoot key."
    return 1
  fi

  if [[ $retval == 3 ]]; then
    echo "EVDI DKMS module already installed."
  elif [[ $retval != 0 ]]; then
    copy_evdi_make_log
    dkms remove "evdi/$evdi_version" --all
    error "Failed to install evdi to the kernel tree."
    return 1
  fi

  if [[ -z $EVDI_REBOOT_RATIONALE ]] && xorg_or_tty_running; then
    EVDI_REBOOT_RATIONALE="The user session is running X11 (or tty)."
  fi
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

function xorg_or_tty_running()
{
  local SESSION_NO user
  command -v logname > /dev/null || return 0
  user=$(logname)
  [[ $user ]] || return 0
  SESSION_NO=$(loginctl | awk "/$user/ {print \$1; exit}")
  [[ $SESSION_NO ]] || return 1
  session=$(loginctl show-session "$SESSION_NO" -p Type)
  [[ $session == *=x11 || $session == *=tty ]]
}

function evdi_requires_reboot()
{
  [[ -n $EVDI_REBOOT_RATIONALE ]]
}

function evdi_success_message()
{
  printf '\n\n'

  if evdi_requires_reboot; then
    notify-send2 -a "DisplayLinkManager" "Reboot required" \
          "DisplayLink evdi module installed successfully. $EVDI_REBOOT_RATIONALE Reboot your computer to ensure proper functioning of the software."

    if [[ -f /usr/share/update-notifier/notify-reboot-required ]]; then
      /usr/share/update-notifier/notify-reboot-required
    fi

    echo "Reboot required"
    echo "DisplayLink evdi module installed successfully."
    echo "$EVDI_REBOOT_RATIONALE"
    echo "Please reboot your computer to ensure the proper functioning of the software."
  else
    echo "DisplayLink evdi module installed successfully."
  fi
  printf '\n\n'

}

# if the script is NOT sourced
if ! (return 0 2>/dev/null); then
  set -e
  evdi_dkms_install
  evdi_add_mod_options
  evdi_success_message
fi
