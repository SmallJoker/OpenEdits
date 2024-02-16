#!/usr/bin/env bash
# To be executed in a separate directory for packing

set -e
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ="OpenEdits"

# fake install location from CMake
GAMEDIR="$SCRIPT_DIR/../../packages/$PROJ"

# ----------------

binary="$PROJ"
[ ! -e "$GAMEDIR/$binary" ] && binary="$binary.exe"
filestr=$(file "$GAMEDIR/$binary")

ostype=""
arch=""
case "$filestr" in
	*"MS Windows"*) ostype="Windows" ;;
	*"Linux"*)      ostype="Linux"   ;;
esac
case "$filestr" in
	*" x86-64"*) arch="x86_64" ;;
esac

[ -z "$ostype" ] && echo "Unhandled platform." && exit 1
[ -z "$arch" ]   && echo "Unhandled architecture." && exit 1

echo "--- Removing testing files"
rm -vf "$GAMEDIR"/*.sqlite*
rm -vf "$GAMEDIR/client_servers.txt"
rm -vrf "$GAMEDIR/worlds"

# zipping
version=$(grep -Eoa "v[0-9]+\.[0-9]+\.[0-9]+[^ ]*" "$GAMEDIR/$binary")
echo "--- Found version: $version"
#version=$(strings "$GAMEDIR/$binary" | grep -Eo "v[0-9]+\.[0-9]+\.[0-9]+[^ ]*")
zipfile="$GAMEDIR/../$PROJ-$version-$ostype-x86_64.7z"

rm -f "$zipfile" # old archive
7z a -t7z -scrcSHA256 "$zipfile" "$GAMEDIR"

echo ""
echo "--- sha256 checksum"
sha256sum "$zipfile"
