#!/usr/bin/env python3
"""Split script_functions.c into 7 categorized files.

This script parses the original file, extracts each function body,
categorizes it by name, and writes it to the appropriate split file.
"""

import re
import sys

SRC = "egolib/library/src/egolib/game/script_functions.c"
HEADER_END = 155  # Infrastructure ends at line 155; functions start at 163

# Explicit function-to-category mapping for all 404 functions.
# Categories: bitwise, state, target, movement, action, spawn, systems

BITWISE = {
    'scr_SetAlertBit', 'scr_ClearAlertBit', 'scr_TestAlertBit',
    'scr_SetAlert', 'scr_ClearAlert', 'scr_TestAlert',
    'scr_SetBit', 'scr_ClearBit', 'scr_TestBit',
    'scr_SetBits', 'scr_ClearBits', 'scr_TestBits',
}

MOVEMENT = {
    'scr_SetTurnModeToVelocity', 'scr_SetTurnModeToWatch', 'scr_SetTurnModeToSpin',
    'scr_SetTurnModeToWatchTarget',
    'scr_Run', 'scr_Walk', 'scr_Sneak', 'scr_Stop',
    'scr_SetSpeedPercent',
    'scr_ClearWaypoints', 'scr_AddWaypoint', 'scr_FindPath', 'scr_Compass',
    'scr_IfAtWaypoint', 'scr_IfAtLastWaypoint',
    'scr_SetXY', 'scr_GetXY', 'scr_AddXY',
    'scr_AccelerateTarget', 'scr_AccelerateUp', 'scr_AccelerateTargetUp',
    'scr_Teleport', 'scr_TeleportTarget',
    'scr_SetBumpHeight', 'scr_GetBumpHeight', 'scr_SetBumpSize',
    'scr_SetFlyHeight', 'scr_SetShadowSize',
    'scr_StopTargetMovement',
    'scr_SetFrame', 'scr_SetReloadTime', 'scr_SetTargetReloadTime',
    'scr_PressLatchButton', 'scr_PressTargetLatchButton',
}

TARGET = {
    # SetTargetTo* functions
    'scr_SetTargetToNearbyEnemy', 'scr_SetTargetToTargetLeftHand',
    'scr_SetTargetToTargetRightHand', 'scr_SetTargetToWhoeverAttacked',
    'scr_SetTargetToWhoeverBumped', 'scr_SetTargetToWhoeverCalledForHelp',
    'scr_SetTargetToOldTarget', 'scr_SetTargetToSelf', 'scr_SetTargetToRider',
    'scr_SetTargetToWhoeverIsHolding', 'scr_SetTargetToWhoeverWasHit',
    'scr_SetTargetToWideEnemy', 'scr_SetTargetToDistantEnemy',
    'scr_SetTargetToTargetOfLeader', 'scr_SetTargetToLeader',
    'scr_SetTargetToLowestTarget', 'scr_SetTargetToOwner',
    'scr_SetTargetToWideBlahID', 'scr_SetTargetToNearestBlahID',
    'scr_SetTargetToNearestEnemy', 'scr_SetTargetToNearestFriend',
    'scr_SetTargetToNearestLifeform',
    'scr_SetTargetToNearbyMeleeWeapon', 'scr_SetTargetToDistantFriend',
    'scr_SetTargetToChild', 'scr_SetTargetToLastItemUsed',
    'scr_SetTargetToWhoeverIsInPassage', 'scr_SetTargetToPassageID',
    'scr_SetTargetToBlahInPassage',
    # IfTarget* queries
    'scr_IfTargetKilled', 'scr_IfTargetIsHurt', 'scr_IfTargetIsAPlayer',
    'scr_IfTargetIsAlive', 'scr_IfTargetIsSelf', 'scr_IfTargetIsMale',
    'scr_IfTargetIsFemale',
    'scr_IfTargetHasID', 'scr_IfTargetHasItemID', 'scr_IfTargetHoldingItemID',
    'scr_IfTargetHasSkillID',
    'scr_IfTargetIsOnOtherTeam', 'scr_IfTargetIsOnHatedTeam',
    'scr_IfTargetIsOnSameTeam',
    'scr_IfTargetIsOldTarget', 'scr_IfTargetCanOpenStuff',
    'scr_IfTargetIsDefending', 'scr_IfTargetIsAttacking',
    'scr_IfTargetHasSpecialID', 'scr_IfTargetHasVulnerabilityID',
    'scr_IfTargetHasAnyID',
    'scr_IfTargetIsKursed', 'scr_IfTargetIsDressedUp',
    'scr_IfTargetIsMounted', 'scr_IfTargetIsFlying', 'scr_IfTargetIsSneaking',
    'scr_IfTargetIsAMount', 'scr_IfTargetIsAPlatform', 'scr_IfTargetIsOwner',
    'scr_IfTargetIsAWeapon', 'scr_IfTargetIsASpell',
    'scr_IfTargetCanSeeInvisible', 'scr_IfTargetCanSeeKurses',
    'scr_IfTargetIsFacingSelf', 'scr_IfFacingTarget',
    'scr_IfDistanceIsMoreThanTurn',
    'scr_IfTargetHasNotFullMana', 'scr_IfTargetHasQuest',
    'scr_IfTargetHasItemIDEquipped',
    'scr_SetOldTarget', 'scr_SetOwnerToTarget',
    'scr_GetTargetGrogTime', 'scr_GetTargetDazeTime',
    'scr_GetTargetState', 'scr_GetTargetContent', 'scr_GetTargetDamageType',
    'scr_GetAttackTurn', 'scr_GetDamageType',
    'scr_TranslateOrder', 'scr_OrderTarget', 'scr_CreateOrder',
    'scr_OrderSpecialID', 'scr_IssueOrder',
}

ACTION = {
    # Animation / action
    'scr_DoAction', 'scr_KeepAction', 'scr_UnkeepAction', 'scr_DoActionOverride',
    'scr_TargetDoAction', 'scr_TargetDoActionSetFrame', 'scr_ChildDoActionOverride',
    'scr_CorrectActionForHand',
    # Messaging
    'scr_SendMessage', 'scr_SendMessageNear', 'scr_CallForHelp',
    # Sound / music
    'scr_PlaySound', 'scr_PlaySoundLooped', 'scr_StopSound',
    'scr_PlaySoundVolume', 'scr_PlayFullSound',
    'scr_SetVolumeNearestTeammate',
    'scr_PlayMusic', 'scr_StopMusic', 'scr_SetMusicPassage', 'scr_ClearMusicPassage',
    # Speech
    'scr_SetSpeech', 'scr_SetMoveSpeech', 'scr_SetSecondMoveSpeech',
    'scr_SetAttackSpeech', 'scr_SetAssistSpeech', 'scr_SetTerrainSpeech',
    'scr_SetSelectSpeech',
    # Knowledge flags
    'scr_MakeNameKnown', 'scr_MakeUsageKnown', 'scr_MakeAmmoKnown',
    'scr_MakeSimilarNamesKnown', 'scr_MakeNameUnknown',
    # Visual effects
    'scr_SetRedShift', 'scr_SetGreenShift', 'scr_SetBlueShift',
    'scr_SetLight', 'scr_SetAlpha',
    'scr_FlashTarget', 'scr_BlackTarget',
    'scr_SparkleIcon', 'scr_UnsparkleIcon',
    'scr_DrawBillboard', 'scr_TakePicture',
    'scr_ShowTimer', 'scr_DisplayCharge',
}

SPAWN = {
    # Character spawning
    'scr_SpawnCharacter', 'scr_SpawnCharacterXYZ', 'scr_SpawnExactCharacterXYZ',
    'scr_SpawnAttachedCharacter',
    # Respawn
    'scr_RespawnCharacter', 'scr_RespawnTarget',
    'scr_EnableRespawn', 'scr_DisableRespawn',
    # Particle spawning
    'scr_SpawnParticle', 'scr_SpawnAttachedParticle', 'scr_SpawnExactParticle',
    'scr_SpawnAttachedSizedParticle', 'scr_SpawnAttachedFacedParticle',
    'scr_SpawnAttachedHolderParticle',
    'scr_SpawnExactChaseParticle', 'scr_SpawnExactParticleEndSpawn',
    'scr_SpawnPoofSpeedSpacingDamage', 'scr_SpawnPoof',
    # Destruction / poof
    'scr_GoPoof', 'scr_PoofTarget', 'scr_CleanUp',
    # Drop / detach
    'scr_DropWeapons', 'scr_DropItems', 'scr_DropKeys', 'scr_DropTargetKeys',
    'scr_DetachFromHolder',
    # Child management
    'scr_SetChildState', 'scr_SetChildAmmo', 'scr_SetChildContent',
    'scr_SetTargetToChild',  # also in target but fits spawn lifecycle better
    # Lifecycle / identity
    'scr_NotAnItem',
    'scr_DisaffirmCharacter', 'scr_ReaffirmCharacter',
    'scr_MakeCrushValid', 'scr_MakeCrushInvalid',
    'scr_SetDamageTime', 'scr_SetDamageThreshold',
    'scr_EnableInvictus', 'scr_DisableInvictus',
    'scr_EnableStealth', 'scr_DisableStealth',
    'scr_SetTargetSize',
    'scr_MorphToTarget', 'scr_IdentifyTarget',
}

# STATE: condition checks, comparisons, flow control, debug, simple queries
STATE = {
    # Basic event checks
    'scr_IfSpawned', 'scr_IfTimeOut', 'scr_IfAttacked', 'scr_IfBumped',
    'scr_IfOrdered', 'scr_IfCalledForHelp',
    # State management
    'scr_SetContent', 'scr_GetContent', 'scr_IfKilled',
    'scr_SetState', 'scr_GetState', 'scr_IfStateIs', 'scr_IfStateIsNot',
    'scr_IfStateIs0', 'scr_IfStateIs1', 'scr_IfStateIs2', 'scr_IfStateIs3',
    'scr_IfStateIs4', 'scr_IfStateIs5', 'scr_IfStateIs6', 'scr_IfStateIs7',
    'scr_IfStateIs8', 'scr_IfStateIs9', 'scr_IfStateIs10', 'scr_IfStateIs11',
    'scr_IfStateIs12', 'scr_IfStateIs13', 'scr_IfStateIs14', 'scr_IfStateIs15',
    'scr_IfStateIsOdd', 'scr_IfContentIs',
    # Comparisons
    'scr_IfXIsLessThanY', 'scr_IfYIsLessThanX', 'scr_IfXIsEqualToY',
    # Condition checks (self)
    'scr_IfGrabbed', 'scr_IfDropped', 'scr_IfNotDropped',
    'scr_IfReaffirmed', 'scr_IfDisaffirmed', 'scr_IfScoredAHit',
    'scr_IfChanged', 'scr_IfInWater', 'scr_IfBored', 'scr_IfTooMuchBaggage',
    'scr_IfGrogged', 'scr_IfDazed', 'scr_IfInvisible', 'scr_IfArmorIs',
    'scr_IfBlocked', 'scr_IfHitGround', 'scr_IfCrushed',
    'scr_IfNotPutAway', 'scr_IfTakenOut', 'scr_IfAmmoOut',
    'scr_IfHitFromBehind', 'scr_IfHitFromFront', 'scr_IfHitFromLeft', 'scr_IfHitFromRight',
    'scr_IfUsed', 'scr_IfCleanedUp', 'scr_IfSitting', 'scr_IfHealed',
    'scr_IfKursed', 'scr_IfOverWater', 'scr_IfThrown',
    'scr_IfNameIsKnown', 'scr_IfUsageIsKnown',
    'scr_IfEquipped', 'scr_IfUnarmed',
    'scr_IfHeldInLeftHand',
    'scr_IfHoldingItemID', 'scr_IfHoldingRangedWeapon',
    'scr_IfHoldingMeleeWeapon', 'scr_IfHoldingShield',
    'scr_IfHitVulnerable', 'scr_IfBackstabbed', 'scr_IfLevelUp',
    'scr_IfHolderBlocked', 'scr_IfStealthed',
    'scr_IfSomeoneIsStealing',
    # Platform / module checks
    'scr_IfOperatorIsLinux', 'scr_IfOperatorIsMacintosh', 'scr_IfModuleHasIDSZ',
    # Misc state
    'scr_SetWeatherTime', 'scr_SetTime',
    # Flow control & debug
    'scr_Else', 'scr_End', 'scr_DoNothing',
    'scr_DebugMessage', 'scr_FlashVariable', 'scr_FlashVariableHeight',
}

# SYSTEMS: everything else — passages, quests, commerce, teams, combat, enchant,
# inventory, stats, environment, module-level
# This is defined implicitly: anything not in the above categories goes here.

CATEGORIES = {
    'bitwise': BITWISE,
    'state': STATE,
    'target': TARGET,
    'movement': MOVEMENT,
    'action': ACTION,
    'spawn': SPAWN,
}

# Remove scr_SetTargetToChild from TARGET if it's in SPAWN (it's in SPAWN)
TARGET.discard('scr_SetTargetToChild')

def categorize(name):
    """Return the category for a function name."""
    for cat, names in CATEGORIES.items():
        if name in names:
            return cat
    return 'systems'  # default catch-all

def parse_functions(filepath):
    """Parse the file and return a list of (name, category, text) tuples."""
    with open(filepath, 'r') as f:
        lines = f.readlines()

    # Find all function start lines (1-based)
    func_pattern = re.compile(r'^uint8_t (scr_\w+)\s*\(')
    func_starts = []
    for i, line in enumerate(lines):
        m = func_pattern.match(line)
        if m:
            func_starts.append((i, m.group(1)))

    # Extract function bodies
    functions = []
    for idx, (start, name) in enumerate(func_starts):
        # Find where this function's text starts (include preceding comments/whitespace)
        # Walk backwards from start to find the beginning of the function's block
        text_start = start
        while text_start > 0 and text_start > (func_starts[idx-1][0] if idx > 0 else HEADER_END):
            prev_line = lines[text_start - 1].strip()
            if prev_line == '' or prev_line.startswith('//') or prev_line.startswith('/*') or prev_line.startswith('*') or prev_line.startswith('///'):
                text_start -= 1
            else:
                break

        # End is just before the next function's comment block starts, or EOF
        if idx + 1 < len(func_starts):
            next_start = func_starts[idx + 1][0]
            # Walk backwards from next function to find where its comment block starts
            text_end = next_start
            while text_end > start + 1:
                prev_line = lines[text_end - 1].strip()
                if prev_line == '' or prev_line.startswith('//') or prev_line.startswith('/*') or prev_line.startswith('*') or prev_line.startswith('///'):
                    text_end -= 1
                else:
                    # Include one trailing blank line after the function body
                    text_end += 1
                    break
            else:
                text_end = next_start
        else:
            text_end = len(lines)

        body = ''.join(lines[text_start:text_end])
        cat = categorize(name)
        functions.append((name, cat, body))

    return functions

def write_split_file(category, functions, outdir):
    """Write a split .c file for a category."""
    descriptions = {
        'bitwise': 'Bitwise alert and flag manipulation functions',
        'state': 'State checks, condition queries, comparisons, and flow control',
        'target': 'Target selection, target property queries, and order management',
        'movement': 'Movement, pathfinding, position, and physics property functions',
        'action': 'Animation, speech, sound, visual effects, and messaging functions',
        'spawn': 'Character/particle spawning, destruction, and lifecycle management',
        'systems': 'Passages, quests, commerce, teams, combat, enchantment, inventory, stats, and environment',
    }

    filepath = f"{outdir}/script_functions_{category}.c"
    with open(filepath, 'w') as f:
        f.write(f"/// @file egolib/game/script_functions_{category}.c\n")
        f.write(f"/// @brief {descriptions[category]}\n\n")
        f.write('#include "egolib/game/script_functions_internal.h"\n\n')

        for name, cat, body in functions:
            f.write(body)
            if not body.endswith('\n'):
                f.write('\n')

    count = len(functions)
    print(f"  {filepath}: {count} functions")
    return count

def main():
    functions = parse_functions(SRC)
    print(f"Parsed {len(functions)} functions total")

    # Group by category
    by_cat = {}
    for name, cat, body in functions:
        by_cat.setdefault(cat, []).append((name, cat, body))

    # Print summary
    for cat in ['bitwise', 'state', 'target', 'movement', 'action', 'spawn', 'systems']:
        funcs = by_cat.get(cat, [])
        print(f"  {cat}: {len(funcs)} functions")

    # Check for any functions that appear in multiple categories
    all_explicit = set()
    for cat_set in CATEGORIES.values():
        for name in cat_set:
            if name in all_explicit:
                print(f"WARNING: {name} appears in multiple categories!")
            all_explicit.add(name)

    # Check for any uncategorized functions (they go to 'systems')
    all_found = {name for name, _, _ in functions}
    uncategorized = all_found - all_explicit
    if uncategorized:
        print(f"\n  {len(uncategorized)} functions go to 'systems' (catch-all):")
        for name in sorted(uncategorized):
            print(f"    {name}")

    # Write files
    outdir = "egolib/library/src/egolib/game"
    print(f"\nWriting split files to {outdir}/")
    total = 0
    for cat in ['bitwise', 'state', 'target', 'movement', 'action', 'spawn', 'systems']:
        funcs = by_cat.get(cat, [])
        if funcs:
            total += write_split_file(cat, funcs, outdir)

    print(f"\nTotal functions written: {total}")
    if total != len(functions):
        print(f"ERROR: Mismatch! Parsed {len(functions)} but wrote {total}")
        sys.exit(1)
    print("SUCCESS: All functions accounted for.")

if __name__ == '__main__':
    main()
