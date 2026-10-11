#!/usr/bin/env bash

set -euo pipefail

ROOT=$(dirname "$0")
ROOT=$(realpath "$ROOT/../")
IRRMT_DIR="$ROOT/irrlicht-mt"

case "$1" in
	prep_ubuntu-24)
		sudo apt update
		sudo apt install -qyy g++ cmake libxi-dev libgl1-mesa-dev libpng-dev libjpeg-dev zlib1g-dev \
			libsqlite3-dev libenet-dev \
			libluajit-5.1-dev libopenal-dev
		git clone --depth 1 -- https://github.com/SmallJoker/irrlicht-mt "$IRRMT_DIR"
		;;

	build_linux_gui)
		cd "$IRRMT_DIR"
		cmake -S . -B build -DUSE_SDL2=0 -DUSE_SDL3=0 -DBUILD_EXAMPLES=0
		(cd build; make -j)

		cd "$ROOT"
		cmake -S . -B build -DIRRLICHTMT_BUILD_DIR="$IRRMT_DIR/build"
		(cd build; make -j)
		;;

	test_linux)
		mv build/OpenEdits .
		OE_DEBUG=+all ./OpenEdits --unittest
		;;

	*)
		echo "Unknown platform"
		;;
esac

# TODO: pack + upload
