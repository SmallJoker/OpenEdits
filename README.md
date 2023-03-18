# OpenEdits

![preview image v1.0.1-dev](screenshot.jpeg)

A 2D game block building inspired by Everybody Edits.
Code structure inspired by Minetest practices.

## Gameplay

**Hotkeys**

 * W/A/S/D or arrow keys: move player
 * Space: Jump
 * Left click: Place the selected block
 * Right click or Shift+Left click: Block eraser
 * 1-9: Hotbar block selector
 * `/` or T: Focus chat
 * Enter: Send chat message
 * E: Toggle block selector
 * G: Toggle god mode
 * M: Toggle minimap


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
 * SQLite3 : auth & world saving

Debian/Ubuntu:

	sudo apt install libenet-dev libsqlite3-dev

Project compiling:

	cmake -B build

**Headless server compiling**

1. Install the required libraries
2. Put the IrrlichtMt headers (include directory) somewhere
3. `cmake -B build -DBUILD_CLIENT=0 -DIRRLICHTMT_BUILD_DIR="/path/to/irrlicht/include/"`
4. `cd build` -> build `make -j` -> start `./OpenEdits-server`


## Licenses

Code: LGPL 2.1+

### Fonts

DejaVu Sans bitmaps (Bitstream Vera license, extended MIT)

 * Converted with https://github.com/kahrl/irrtum

### Images

Images that are not listed below were created by SmallJoker (CC BY 4.0).

DailyYouth (CC BY 3.0)

 * [`icon_chat.png`](https://www.iconfinder.com/icons/3643728/balloon_chat_conversation_speak_word_icon)

IconMarket (CC BY 3.0)

 * [`icon_minimap.png`](https://www.iconfinder.com/icons/6442794/compass_direction_discover_location_navigation_icon) (desaturated)
