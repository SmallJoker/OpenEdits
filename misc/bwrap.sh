#!/usr/bin/env bash

set -euo pipefail

WORKDIR=$(realpath "$1")
WORKDIRNAME=$(basename "$WORKDIR")

if [ $# -lt 2 ]; then
	echo "Usage: bwrap.sh /path/to/work/dir ./bin/executable args .."
	exit 1
fi

shift 1
USERDIR="/run/user/$(id -u)"

# TODO: PipeWire support

(exec bwrap \
	--ro-bind /usr /usr \
	--symlink /usr/lib /lib \
	--symlink /usr/lib64 /lib64 \
	--symlink /usr/bin /bin \
	--symlink /usr/sbin /sbin \
	--dir /tmp \
	--dir /var \
	--symlink ../tmp /var/tmp \
	--proc /proc \
	--dev /dev \
	--dev-bind /dev/dri /dev/dri \
	--ro-bind /sys /sys \
	--bind "$WORKDIR" "$USERDIR/$WORKDIRNAME" \
	--chdir "$USERDIR/$WORKDIRNAME" \
	--dir "$USERDIR" \
	--setenv HOME "$USERDIR" \
	--setenv XDG_RUNTIME_DIR "$USERDIR" \
	--unshare-all \
	--share-net \
	--ro-bind /etc/resolv.conf /etc/resolv.conf \
	--ro-bind /etc/alsa/conf.d /etc/alsa/conf.d \
	--ro-bind "$USERDIR/pulse" "$USERDIR/pulse" \
	-- $@)
