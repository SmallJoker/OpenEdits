#!/usr/bin/env sh

set -e

ostype=""
case "$OSTYPE" in
	darwin*)  ostype="macOS"   ;;
	linux*)   ostype="Linux"   ;;
	bsd*)     ostype="BSD"     ;;
	msys*)    ostype="Windows" ;;
	cygwin*)  ostype="Windows" ;;
esac

[ -z "$ostype" ] && echo "Unhandled platform." && exit 1


DIR="$PWD/build/OpenEdits"

echo "--- Removing testing files"
rm -f "$DIR"/*.sqlite
rm -f "$DIR/client_servers.txt"

# zipping
version=$(strings "$DIR/OpenEdits" | grep -Eo "v[0-9]+\.[0-9]+\.[0-9]+[^ ]*")
zipfile="OpenEdits-$version-$ostype-x86_64.7z"

rm -f "$zipfile" # old archive
7z a -t7z -scrcSHA1 "$zipfile" "$DIR"

echo ""
echo "--- sha256 checksum"
sha256sum "$zipfile"
