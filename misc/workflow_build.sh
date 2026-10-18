#!/usr/bin/env bash

set -eu

ROOT=$(dirname "$0")
ROOT=$(realpath "$ROOT/../")
IRRMT_DIR="$ROOT/irrlicht-mt"
INSTALL_DIR="build/build/OpenEdits"

cd "$ROOT"

clone_irrlichtmt()
{
	git clone --depth 1 -- https://github.com/SmallJoker/irrlicht-mt "$IRRMT_DIR"
}

pacman_install()
{
	pacman --noconfirm -S --needed --overwrite -- "$@"
}

case "$1" in
	# Windows
	prep_windows)
		PF="ucrt64/mingw-w64-ucrt-x86_64-"

		# https://github.com/msys2/setup-msys2#install
        # https://packages.msys2.org/base?repo=ucrt64
        # Bug: newlines cause the "error: no targets specified (use -h for help)" output
		pacman_install ${PF}cmake ${PF}make ${PF}gcc ${PF}git
		pacman_install ${PF}zlib ${PF}libpng ${PF}libjpeg-turbo ${PF}sdl3 ${PF}sqlite3 ${PF}enet ${PF}luajit ${PF}openal
		clone_irrlichtmt
		;;

	install_runtime_mingw64)
		mingw_bin_path=$(grep "CMAKE_C_COMPILER:" build/CMakeCache.txt | cut -d '=' -f 2)
		echo "MinGW compiler path: $mingw_bin_path"
		mingw_bin_path=$(dirname "$mingw_bin_path")

		# TODO This is hacky. The IrrlichtMt target should at least expose its dependency paths.
		libs=$(printf "%s\n%s" "$(ldd $INSTALL_DIR/OpenEdits.exe)" "$(ldd $INSTALL_DIR/IrrlichtMt.dll)")
		while read -r line; do
			# "\tmylib.dll => not found"
			# "\tmylib.dll => /path/to/mylib.dll (0xaddress)"
			lib_name=$(echo "$line" | cut -d '=' -f 1 | xargs)
			# Try copy, or ignore (tee)
			cp -fv "$mingw_bin_path/$lib_name" "$INSTALL_DIR" 2>/dev/null | tee
		done <<< "$libs"

		# CMake only knows of one *_LIBRARY, which sucks. Clean up the mess.
		rm -f "$INSTALL_DIR"/*.a
		;;

	test_windows)
		cd "$INSTALL_DIR"
		OE_DEBUG=+all ./OpenEdits.exe --unittest
		;;

	# Ubuntu
	prep_ubuntu-24)
		sudo apt update
		sudo apt install -qyy g++ cmake libsdl2-dev libpng-dev libjpeg-dev zlib1g-dev \
			libsqlite3-dev libenet-dev \
			libluajit-5.1-dev libopenal-dev
		clone_irrlichtmt
		;;

	build_linux_gui | build_windows_gui)
		cd "$IRRMT_DIR"
		# Try SDL3 first, if available.
		cmake -S . -B build -DUSE_SDL2=0 -DUSE_SDL3=1 -DBUILD_EXAMPLES=0 ||\
			cmake -S . -B build -DUSE_SDL2=1 -DUSE_SDL3=0 -DBUILD_EXAMPLES=0
		(cd build; cmake --build . --parallel)

		cd "$ROOT"
		cmake -S . -B build -DIRRLICHTMT_BUILD_DIR="$IRRMT_DIR/build"
		(cd build; cmake --build . --parallel)
		;;

	test_linux)
		cd "$INSTALL_DIR"
		OE_DEBUG=+all ./OpenEdits --unittest
		;;

	*)
		echo "Unknown platform"
		;;
esac

# TODO: pack + upload
