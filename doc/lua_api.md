# Lua API

Client and server should generally load the same code in
order to provide a lag-free experience by client-side prediction.
By this, the server should be able to reproduce the player's movements
and interactions, which serves as an anti-cheat mechanism.

Main entry point: `main.lua`

 * Applies to client and server


### Compatibility

 * `api.version` (integer)
    * To be increased if the game is ever in a usable state.


### Helper functions

 * `print(...)`
    * Prints the given values to the terminal/console
 * `error(...)`
    * Logs an error but does NOT abort script execution!
    * Includes detailed argument dumping


### Environment API

Namespace: `env.world`. Associated to the world of the currently
active player of the callback.

 * `get_block(x, y)` -> `fg, tile, bg`
    * `x, y` (optional): if set to `nil`, the current player position is used.
 * `set_tile(x, y, tile)`
    * Sets the block tile
    * `x, y` (optional): if set to `nil`, the current player position is used.


### Player API

Namespace: `env.player`. Associated to the currently active player
of the callback.

 * `.get_pos()` -> `x, y`
    * The block underneath is at `(floor(x + 0.5), floor(y + 0.5))`.
 * `.set_pos(x, y)`
 * `.get_vel()` -> `x, y`
 * `.set_vel(x, y)`


### Packs

 * `env.register_pack(Pack Definition)`

Pack Definition:

 * `name` (string): Internal name of the pack. A corresponding
   texture named `pack_{name}.png` is required.
 * `default_type`: one of `env.DRAW_TYPE_*`. Defines the default
   appearance and the tab in which the pack shows up.
 * `blocks` (table): List of block ID's contained in this pack


### Blocks

 * `env.change_block(id, Block Definition)`
    * The block must first be registered by `env.register_pack`.

Block Definition - regular fields:

 * `tiles = { Tile Definition, ... }` (optional)
    * Tile Definition (table)
        * `type` (optional): one of `env.DRAW_TYPE_*`.
          Defaults to the draw type specified by the pack.
        * `alpha` (optional, boolean): whether to force-draw the alpha channel.
          By default, this is inherited from the draw type of the tile.
    * Tile limit: 8.
    * Only foreground blocks (`type != env.DRAW_TYPE_BACKGROUND`) can have >= 1 tile.
 * `viscosity` (optional, number)
    * Higher values slow the player down more
    * Default: `1.0`

Block Definition - callbacks:

 * `on_collide(bx, by, is_x)` <- `env.COLLISION_TYPE_*`
    * Called when colliding with the block at position `(bx, by)`.
    * `is_x` (boolean): Indicates the direction of collision
 * `on_intersect()` <- `nil`
    * Called while the player's center position is within the block.
 * `on_intersect_once(tile)` <- `nil`
    * Called once when entering the block.
    * `tile` (number): The tile index of the block

