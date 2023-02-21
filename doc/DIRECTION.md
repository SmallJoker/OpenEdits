# Project direction

1. Same gameplay style as Everybody Edits
2. Same block ID mapping. New blocks/packs must have an unique ID
3. Guest "accounts" without chat or owned worlds
4. Decentralised servers (community-hosted)

To-do:

 * Account registration/authentication
 * Per-world permission system
 * Blocks: one-way gates, teleportation, text
 * Animated coins (rotating mesh?)

Maybe:

 * Friends features, inclusing a listing in the lobby to join their world
 * Group/crew system for easier per-world permission management


### Code structure

1. No GUI dependencies in `Client` (use `GameEvent`)
2. No dependencies between `Client` and `Server`

Server-only build:

 * Irrlicht dependency limited to headers only


### Graphics

1. >= 32x32 px textures
2. Where it matters, use [color-blind friendly colors](https://www.nature.com/articles/nmeth.1618.pdf)
