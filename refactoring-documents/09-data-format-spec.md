# Data.txt Format Spec

This document records the current `data.txt` contract as implemented in the active loader on 2026-04-13.

It is a runtime-compatibility spec for the existing positional format, not a design for a future schema.

## 1. Location and loader path

- Source file in content: `<object>/data.txt`
- Primary loader: `ObjectProfile::loadDataFile(...)`
- Source file: `egolib/library/src/egolib/Profiles/ObjectProfile.cpp`
- Related export path: `ObjectProfile::exportCharacterToFile(...)`

The loader reads the file sequentially. Field meaning depends on position. Later tagged expansions extend or override parts of the base record.

## 2. Base positional sections

The current loader reads these sections in order.

### Header and identity

- slot number
  Read first through `vfs_get_next_object_profile_ref(ctxt)`. `loadDataFile()` currently ignores the value for live profile construction, but slot semantics still matter elsewhere in the profile system and exported characters use this position.
- class name
  Loaded as a string literal, then capitalized in place on the first character.
- uniform lighting flag
- max ammo
- current ammo
- gender

### Primary attributes and growth

- life bar color
- mana bar color
- life base and gain
- mana base and gain
- mana regen base and gain
- spell power base and gain
- might base and gain
- deprecated wisdom base and gain
- intellect base and gain
- agility base and gain

Current loader quirks:

- max life is doubled on parse for historical damage-balance compatibility.
- wisdom is deprecated, but if present and non-zero it is averaged into intellect.

### Physics, movement, and rendering

- size
- size gain per level
- shadow size
- bump size
- bump height
- bump dampen
- weight
- jump power
- number of jumps
- sneak, walk, and run animation rates
- fly height
- flash rate
- alpha
- light
- transfer blending
- sheen
- phong mapping
- texture movement X and Y
- sticky-butt flag

### Defense and skin data

- invincibility flag
- normal frame facing and angle
- invincibility frame facing and angle
- four skin defense values
- per-damage-type resistance rows
- per-damage-type modifier rows
- max acceleration values for four skins

Current loader quirks:

- `nframeangle == 1` is normalized back to `0` because of old burn or stuck-arrow behavior.
- four-skin assumptions are still hardcoded in multiple places.

### XP, IDSZ, and item flags

- level XP thresholds
- starting experience
- XP worth
- XP exchange
- XP rates
- IDSZ tags
- item and usage flags
- damage target damage type
- weapon action

### Particle, hand, and combat extras

- attached-particle amount, damage type, and local particle reference
- left and right hand validity
- attack-particle attachment flag and profile
- go-poof particle amount, facing spread, and profile
- blud flag and profile

### Miscellaneous and legacy tail fields

- water-walking flag
- bounciness
- deprecated life-return value
- mana cost
- life regen
- stopped-by collision mask
- four skin names
- four skin costs
- deprecated strength bonus field
- inverted rider-can-attack flag
- dazed flag
- grogged flag
- two deprecated permanent-stat fields
- see-invisible flag
- kurse chance
- footfall sound
- jump sound

Current loader quirks:

- life regen is divided by `256.0f` on parse.
- rider-can-attack is intentionally inverted when read.
- see-invisible is reduced to a boolean-to-level-1 mapping in the loader.

## 3. Tagged expansions

After the positional payload, the loader scans `:<IDSZ> ...` expansions.

Supported expansions observed in current code:

- `DRES`
  Marks dressy skins.
- `GOLD`
  Money override.
- `STUK`
  Resist bump-spawn behavior.
- `PACK`
  Big-item behavior.
- `VAMP`
  Reflection toggle.
- `DRAW`
  Always-draw toggle.
- `RANG`
  Ranged-weapon flag.
- `HIDE`
  Hide-state override.
- `EQUI`
  Equipment flag.
- `SQUA`
  Force square bumper size.
- `ICON`
  Draw-icon override.
- `SHAD`
  Force-shadow flag.
- `SKIN`
  Saved-character skin override.
- `CONT`
  Saved-character content override.
- `STAT`
  Saved-character state override.
- `LEVL`
  Saved-character level override.
- `PLAT`
  Platform-usage override.
- `RIPP`
  Ripple behavior override.
- `VALU`
  Valuable-state override.
- `LIFE`
  Saved-character spawn-life override.
- `MANA`
  Saved-character spawn-mana override.
- `BOOK`
  Spellbook-generated effect type.
- `FAST`
  Fast-attack flag.
- `STRD`
  strength-based damage bonus.
- `INTD`
  intellect-based damage bonus.
- `DEXD`
  agility-based damage bonus.
- `MODL`
  bumper and model override flags. Current letters are `S`, `B`, `H`, and `C`.
- `BLOC`
  Block rating.
- `SEED`
  Level-up random seed override.
- `PERK`
  Starting perk.
- `POOL`
  Learnable perk pool.

The loader also preserves compatibility aliases for old skill-era IDSZ values such as `AWEP`, `POIS`, `CKUR`, `READ`, `WMAG`, `HMAG`, `TECH`, `DISA`, `STAB`, `DARK`, and `JOUS`.

Unknown IDSZ expansions are logged as warnings.

## 4. Export and save compatibility

- Exported characters write `-1` in the first slot-number position to mean flexible-load behavior.
- Export also writes runtime state back into tagged expansions such as `GOLD`, `SKIN`, `CONT`, `STAT`, `LEVL`, `SEED`, `LIFE`, `MANA`, `PERK`, and `POOL`.
- This means `data.txt` is both a base object-definition format and part of save or import/export compatibility behavior.

## 5. Validator contract

The validator currently treats `data.txt` as required for any resolved object profile and reports:

- `missing_required_file` when `data.txt` is absent
- `parse_failure` when `ObjectProfile::loadFromFile(...)` or `loadDataFile(...)` cannot produce a valid lightweight profile

The validator does not yet classify field-level semantic problems inside `data.txt` separately from general profile-parse failures.

## 6. Refactor implications

- The first positional field cannot be treated as dead data just because `loadDataFile()` ignores it locally; slot semantics still leak into import/export and profile loading.
- Migration work must preserve parser-time transforms such as doubled life, wisdom-to-intellect merging, life-regen scaling, and the inverted rider-can-attack field.
- Expansion tags carry both feature flags and save-state overrides, so a future schema must separate definition-time and instance-time semantics explicitly.
- Unknown expansions should remain observable during migration work rather than being silently dropped.
