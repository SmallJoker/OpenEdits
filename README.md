# OpenEdits

![preview image v1.0.1-dev](screenshot.jpeg)

A 2D game block building inspired by Everybody Edits.
Code structure inspired by Minetest practices.

## Available archives

### Linux

1. Verify the archive checksum
2. Extract the archive to any location
3. Run `AppRun.sh`
4. In case of issues: run with `gdb`. Debug symbols are included.


## Compiling

### Dependencies

 * CMake (cmake-gui recommended for desktops)
 * [Irrlicht-Mt](https://github.com/minetest/irrlicht) : GUI/rendering library
 * [enet](http://enet.bespin.org/) : networking library
 * Threads (phthread?)

Debian/Ubuntu:

	sudo apt install libenet-dev

Project compiling:

	cmake -B build


## Licenses

Code: LGPL 2.1+

### Fonts

DejaVu Sans bitmaps (Bitstream Vera license, extended MIT)

 * Converted with https://github.com/kahrl/irrtum
