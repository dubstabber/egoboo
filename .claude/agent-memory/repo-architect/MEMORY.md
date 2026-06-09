# repo-architect Memory Index

- [SOLID Analysis 2026-04](solid_analysis_2026_04.md) — SOLID principles and design patterns assessment across core egolib subsystems
- [CMake Sub-Library Pattern](cmake_sublibrary_pattern.md) — idlib reference pattern + concrete delta for a first egolib sub-library carve (physics nucleus); gotchas for C_SOURCE_FILES loop, .mm glob, INTERFACE paths, circular deps
- [Foundation Closure: map_file set](foundation_closure_map_file.md) — {map_functions, Log, FileFormats/map_file, Mesh/Info, Math} confirmed game/-layer-free; closure analysis + one gratuitous fileutil.h include in map_file.c (non-blocking)
- [Physics TU Remaining game/ Edges](physics_tu_remaining_game_edges.md) — Complete edge inventory + classifications for CollisionSystem/ObjectPhysics/ParticlePhysics/particle_collision after IObjectWorld merge; 3 seamable edges (worldUpdateCount, physics.h relocation, graphic.h→IAudioSystem), 2 genuinely-game (Shop, CharacterMatrix)
