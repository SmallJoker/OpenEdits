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


### Environment World API

Namespace: `env.world`. Associated to the world of the currently
active player of the callback.

 * `get_id()` -> string
    * Server only
 * `select(id)` -> boolean
    * Server only
 * `get_block(x, y)` -> `fg, tile, bg`
    * `x, y` (optional): if set to `nil`, the current player position is used.
 * `get_blocks_in_range(options, block_ids, PositionRange ...)`
    * `options` (table): All fields are optional. Defaults specified below.
        * `return_pos = false`
        * `return_tile = false`
        * `return_params = false`
    * `block_ids` (table/number): Block ID whitelist
        * Format: `{ block_id_1, block_id_2, ... }`
    * `PositionRange`: see `env.world.set_tile`
    * Return value: (table)
        * Key: integer-based
        * Value: `{ block_id [, x, y] [, tile] [, params ...] }`
 * `update_tiles(block_ids)`
    * Client only
    * `block_ids` (table/number): affected Block IDs
    * Calls to `blockdef.get_visuals`.
 * `set_block(x, y, ...)`
    * Server-side only
    * `...`: Block Parameters (variable)
 * `get_params(x, y)` -> (variable)
    * Retrieves the block parameters based on the type specified by the
      `params` Block Definition field.
 * `set_tile(block_id, tile, PositionRange ...)`
    * Sets the tile index of one or multiple blocks.
    * Returns `true` if blocks were modified.
    * `PositionRange` (optional): defines which blocks that are affected
        * arg 1: one of `env.world.PRT_*`. May be combined with `env.world.PROP_*`.
        * arg 2+: see `script_environment.cpp` / `Script::get_position_range`

Environment-related callbacks:

 * `.on_step(abstime)`
    * `abstime` (number), absolute timestamp


#### Script events

 * `env.register_event(event_id, nil, ...`)
    * `event_id` (integer)
    * `nil`: placeholder.
    * `...`: pattern of the event data (count >= 0)
        * Each index must be of `env.PARAMS_TYPE_*`
        * Example: `env.PARAMS_TYPE_U8U8U8, env.PARAMS_TYPE_STR16`
 * `env.send_event(event_id, ...)`
    * `event_id` (integer)
    * `...`: event data
        * Must match the pattern specified by `env.register_event`.
        * Example: `8, 5, 42, "test value"`
* `env.event_handlers[event_id] = function(...)`
    * Counterpart for `env.send_event`, run on the other peer
    * `...`: flat value list but without types
    * Return value: (none)


### Player API (PlayerRef)

In callbacks, `env.player` is assigned to the current active player.

A `userdata` object.

 * `:get_name()` -> string
 * `:hash()` -> (undefined)
    * Unique identifier (per connection) for faster array access
 * `:send_event()`
    * See `env.send_event`. For servers only.
 * `:next_prn()` -> integer
    * Gets the next pseudo-random number
 * `:get_pos()` -> `x, y`
    * The block underneath is at `(floor(x + 0.5), floor(y + 0.5))`.
 * `:set_pos(x, y)`
 * `:get_vel()` -> `x, y`
 * `:set_vel(x, y)`
 * `:get_acc()` -> `x, y`
 * `:set_acc(x, y)`
 * `:get_controls()` -> table
    * Table fields:
       * `jump` (bool)
       * `dir_x`, `dir_y` (number)
       * Note: `dir_(x|y)` may have any value range.
 * `env.is_me()` -> bool
    * Returns whether the controlled player is selected.

Player-related callbacks in `env`:

 * `.on_player_event(event, ...)`
    * Callback to overwrite at load time
    * List of events (`event`) and payload (`...`):
       * `"join"`
       * `"leave"`
       * `"godmode", status`


### Server API

Namespace: `env.server`. Only available for servers.

 * `.get_players_in_world()`
    * Returns a table of `PlayerRef` (ipairs).


### Client GUI API

Namespace: `gui`. Only available for GUI clients.

 * `change_hud(id, HUD Definition)`
 * `play_sound(asset_name)`
    * Depends on `env.require_asset`
 * `select_block(block_id, ...)`
    * `block_id` (nil/number): Block ID to select
    * `...`: Block Parameters (variable)
    * Generally to be used in the `gui_def.on_place` callback

Block Definition field `gui_def`:

 * (GUI Element Definition)
 * `values` (table): default values of the GUI elements
 * `on_input(values, k, v)`
    * Executed upon value change of a GUI element
    * `values` (table): reference to `gui_def.values`
    * `k` (string): changed GUI element name
    * `v` (variable): value of the GUI element
 * `on_place(values, x, y)`
    * Executed once per click (& drag)
    * `values` (table): reference to `gui_def.values`
    * `x`, `y`: The clicked block position
    * `gui.select_block` may be used to set the selected Block Parameters.

HUD Definition: (table)

 * `type` (string)
 * `value` (any, tbd.)


### Registration

These functions must be run at load time.

 * `env.include(asset_name, [scope])`
    * Adds another script to the required assets and executes it.
    * `scope` (string): optional. Default: `nil`
       * value `nil`: distributes and runs the script on server and client
       * value `"server"`: script is only available and run on server-side
       * Note for `scope ~= nil`: avoid registrations. There would be mismatches.
 * `env.require_asset(asset_name)`
    * To use for dynamically used assets, such as audio playback.
    * `asset_name` (string): file name without extension
 * `env.register_smileys({ def1, def2, ... })`
    * Registers the smileys as provided by `smileys.png`
    * `def` (table): One entry per smiley. Fields:
       * `description` (string): Human-readable name


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

**Block Draw Types**

| `DRAW_TYPE_*` | Collides? [1] | alpha OFF [2] | alpha ON [2] | Z [3] |
|---------------|---------------|---------------|--------------|-------|
| `*SOLID`      | Yes           | Opaque        | 1 bit        |  2    |
| `*ACTION`     | No            | 1 bit         | 8 bits       |  2    |
| `*DECORATION` | No            | 1 bit         | N/A          | -1    |
| `*BACKGROUND` | Never         | Opaque        | N/A          |  5    |

Notes:

 * [1]: By default. Can be overwritten by `on_collide` except for `*BACKGROUND`.
 * [2]: Render mode of the tile texture. Value of `(Tile Definition).alpha`.
 * [3]: Z-Index. Positive = away from the camera.
   The player is at `0` (default) or `-3` (godmode). (C++: `ZINDEX_LOOKUP`)

Block Definition - regular fields:

 * `gui_def`: See [Client GUI API]
 * `minimap_color` (optional, number)
    * Color in the format `0xAARRGGBB`
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
        * `override` (optional, table): format `{ id = Block ID, tile = number }`
          Replaces the appearance of the block with the tile of another block.
    * Tile limit: 8.
    * Only foreground blocks (`type != env.DRAW_TYPE_BACKGROUND`) can have >= 1 tile.
 * `viscosity` (optional, number)
    * Higher values slow the player down more
    * Default: `1.0`

Block Definition - callbacks:

 * `get_visuals(tile, ...)` -> `int`/`nil`
    * Used by client only
    * `tile` (number): current tile
    * `...`: Block Parameters (variable)
    * Must return the new tile number. Triggered by `env.world.update_tiles`.
 * `on_collide(bx, by, is_x)` <- `env.COLLISION_TYPE_*`/`nil`
    * Called when colliding with the block at position `(bx, by)`.
    * `is_x` (boolean): Indicates the direction of collision
 * `on_intersect()` <- `nil`
    * Called while the player's center position is within the block.
 * `on_intersect_once(tile)` <- `nil`
    * Called once when entering the block.
    * `tile` (number): The tile index of the block

**Guideline for tile changes**

If a block has multiple tiles, the following cases exist:

1. All tiles have the same *physical properties*, e.g. the player never collides.
2. The *physical properties* are different on at least one tile, i.e. the physics
   depend on the shown tile.

The *physical properties* depend on `(Tile Definition).type` and `on_collide`.
Hence, the following rule of thumb exists:

1. The same: The tiles may be changed freely by the client. (`env.world.set_tile`)
2. Differing: Tile changes must be initiated by the server. ([Script events])
