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
 * `get_params(x, y)` -> (variable)
    * Retrieves the block parameters based on the type specified by the
      `params` Block Definition field.
 * `set_tile(block_id, tile, PositionRange ...)`
    * Sets the block tile
    * `PositionRange` (optional): defines which blocks that are affected


#### Script events

 * `env.send_event(event_id, ...)`
    * `event_id` (integer)
	* `...`: flat type + value list
		* Example: `*_U8U8U8, 42, 32, 1, *_STR16, "test value"`,
		* `typeN`: one of the accepted `env.PARAMS_TYPE_*`
		* value N...: N values matching the specified type
* `env.event_handlers[event_id] = function(...)`
	* Counterpart for `env.send_event`, run on the other peer
	* `...`: flat value list but without types
	* Return value: (none)


### Player API

Namespace: `env.player`. Associated to the currently active player
of the callback.

 * `.get_pos()` -> `x, y`
    * The block underneath is at `(floor(x + 0.5), floor(y + 0.5))`.
 * `.set_pos(x, y)`
 * `.get_vel()` -> `x, y`
 * `.set_vel(x, y)`
 * `.get_name()` -> string
 * `.hash()` -> (undefined)
     * Unique identifier (per connection) for faster array access
 * `.send_event(...)` TODO WIP
     * See `env.send_event`


### Server API

Namespace: `env.server`. Only available for servers.

 * `.on_player_join()`
    * Callback
 * `.on_player_leave()`
    * Callback
 * `.get_players_in_world()`
    * Returns a table of players in the current world.
       * key: ID, value: name
    * Iterator. Updates `env.player`.
    * `reset` (boolean, optional): sets `env.player` to the curernt player.


### GUI API

Namespace: `env.gui`. Only available for clients.

 * `change_hud(id, HUD Definition)`

HUD Definition: (table)

 * `type` (string)
 * `value` (any, tbd.)


### Registration

 * `env.include(asset_name, [scope])`
    * Adds another script to the required assets and executes it.
    * `scope` (string): optional. Default: `nil`
       * value `nil`: distributes and runs the script on server and client
       * value `"server"`: script is only available and run on server-side
       * Note for `scope ~= nil`: avoid registrations. There would be mismatches.
 * `env.require_asset(asset_name)`
    * To use for dynamically used assets, such as audio playback.
    * `asset_name` (string): file name without extension


#### Packs

 * `env.load_hardcoded_packs()`
    * Loads the original packs
 * `env.register_pack(Pack Definition)`
    * Registers a new pack

Pack Definition: (table)

 * `name` (string): Internal name of the pack. A corresponding
   texture named `pack_{name}.png` is required.
 * `default_type`: one of `env.DRAW_TYPE_*`. Defines the default
   appearance and the tab in which the pack shows up.
 * `blocks` (table): List of block ID's contained in this pack


#### Blocks

 * `env.change_block(id, Block Definition)`
    * The block must first be registered by `env.register_pack`.

Block Definition - regular fields:

 * `params` (optional, number)
    * Defines what kind of data can be saved for this block.
    * Warning: Changing this type will truncate existing saved data.
    * Default: `env.PARAMS_TYPE_NONE`
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
 * `on_placed()` <- `nil`
    * Called when at least one such block was placed.
