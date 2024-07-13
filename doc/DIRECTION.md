# Project direction

1. Same gameplay style as Everybody Edits
2. Same block ID mapping. New blocks/packs must have an unique ID
3. Guest "accounts" without chat or owned worlds
4. Decentralised servers (community-hosted)

To-do:

 * Transaction IDs for packets
    * `status_t Server::pkt_*(peer_t, Packet &pkt)`
    * The return value is sent in an ACK-like packet when `pkt[1] != 0`
    * Useful for chat commands, bot programming and saner client communication
 * Private worlds
 * Achievements: death counter, timer
 * Modding:
    * `on_trigger` server-side callbacks
    * Player-specific gravity and speed modifiers
    * Ensure correct client-side prediction
    * Preserve unknown blocks + data
    * Movable objects, e.g. for mobs.
 * Moderator tools in the main menu
    * Event logger + "Inbox"
    * IP/user bans or mutes
 * Chat commands
    * `/pm`
    * `/ignore` (server-wide)
 * Audiovisuals
    * Animated coins (rotating mesh?)
    * More sounds: on collide, jump (also of other players)
 * Anti-Cheat
    * Opt-in by chat commands
    * Confirmation by server-side physics simulation
    * Abort simulation near modified blocks

Maybe:

 * Group/crew system for easier per-world permission management


### Code structure

1. No GUI dependencies in `Client` (use `GameEvent`)
2. No dependencies between `Client` and `Server`

Server-only build:

 * Irrlicht dependency limited to headers only


### Graphics

1. >= 32x32 px textures
2. Where it matters, use [color-blind friendly colors](https://www.nature.com/articles/nmeth.1618.pdf)

Helpful GIMP tools for texture creation:

 * Block borders:
     1. Select -> Shrink (1 px) : Adds a border
     2. Select -> Feather (5 px) : Smoother selection
     3. Select -> Invert : Actually selects the border
     4. Bucket Fill Tool with 100% threshold to fill the border (black?)
 * For rounded borders (e.g. candy pack)
     1. Select -> Rounded Rectangle (35%)
     2. Select -> Invert
     3. Delete (needs alpha channel)
 * Basic shading
     1. New layer, Mode "Multiply"
     2. Gradient Tool : White to grey gradient on new layer
