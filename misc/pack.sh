#!/usr/bin/env sh
# To be executed in a separate directory for packing

# change me if needed
GAMEDIR="$PWD/OpenEdits"

# ----------------

set -e

ostype=""
binary="OpenEdits"
case "$OSTYPE" in
	darwin*)  ostype="macOS"   ;;
	linux*)   ostype="Linux"   ;;
	bsd*)     ostype="BSD"     ;;
	msys*)    ostype="Windows" ;;
	cygwin*)  ostype="Windows" ;;
esac

[ -z "$ostype" ] && echo "Unhandled platform." && exit 1
[ "$ostype" = "Windows" ] && binary="OpenEdits.exe"

echo "--- Removing testing files"
rm -vf "$GAMEDIR"/*.sqlite*
rm -vf "$GAMEDIR/client_servers.txt"
rm -vrf "$GAMEDIR/worlds"

# zipping
version=$(grep -Eoa "v[0-9]+\.[0-9]+\.[0-9]+[^ ]*" "$GAMEDIR/$binary")
#version=$(strings "$GAMEDIR/$binary" | grep -Eo "v[0-9]+\.[0-9]+\.[0-9]+[^ ]*")
zipfile="OpenEdits-$version-$ostype-x86_64.7z"

rm -f "$zipfile" # old archive
7z a -t7z -scrcSHA1 "$zipfile" "$GAMEDIR"

echo ""
echo "--- sha256 checksum"
sha256sum "$zipfile"
