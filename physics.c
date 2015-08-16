/*

  physics.c

  Copyright 2015 Matthew T. Pandina. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY MATTHEW T. PANDINA "AS IS" AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHEW T. PANDINA OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <stdlib.h>

#include "entity.h"
#include "data/tileset.inc"
#include "stackmon.h"

extern const char mysprites[] PROGMEM;
extern const struct PatchStruct patches[] PROGMEM;

__attribute__(( optimize("Os") ))
static bool PgmBitArray_readBit(const uint8_t* const array, const uint16_t index)
{
  return (bool)(pgm_read_byte(array + (index >> 3)) & (1 << (index & 7)));
}

__attribute__(( optimize("Os") ))
static uint8_t PgmPacked5Bit_read(const uint8_t* const packed, const uint16_t position) // having 'const uint8_t* const packed' really helps optimization here!
{
  uint8_t value;
  const uint16_t i = position * 5 / 8; // 5 bits packed into 8
  switch (position % 8) {
  case 0: // bits: 4 3 2 1 0 x x x
    value = ((pgm_read_byte(packed + i)) >> 3) & 0x1F;
    break;
  case 1: // bits: x x x x x 4 3 2
          // bits: 1 0 x x x x x x
    value = (((pgm_read_byte(packed + i)) << 2) | (((pgm_read_byte(packed + i + 1)) >> 6) & 0x03)) & 0x1F;
    break;
  case 2: // bits: x x 4 3 2 1 0 x
    value = ((pgm_read_byte(packed + i)) >> 1) & 0x1F;
    break;
  case 3: // bits: x x x x x x x 4
          // bits: 3 2 1 0 x x x x
    value = (((pgm_read_byte(packed + i)) << 4) | (((pgm_read_byte(packed + i + 1)) >> 4) & 0x0F)) & 0x1F;
    break;
  case 4: // bits: x x x x 4 3 2 1
          // bits: 0 x x x x x x x
    value = (((pgm_read_byte(packed + i)) << 1) | (((pgm_read_byte(packed + i + 1)) >> 7) & 0x01)) & 0x1F;
    break;
  case 5: // bits: x 4 3 2 1 0 x x
    value = ((pgm_read_byte(packed + i)) >> 2) & 0x1F;
    break;
  case 6: // bits: x x x x x x 4 3
          // bits: 2 1 0 x x x x x
    value = (((pgm_read_byte(packed + i)) << 3) | (((pgm_read_byte(packed + i + 1)) >> 5) & 0x07)) & 0x1F;
    break;
  default: // case 7
           // bits: x x x 4 3 2 1 0
    value = (pgm_read_byte(packed + i)) & 0x1F;
    break;
  }
  return value;
}

enum INPUT_FUNCTION;
typedef enum INPUT_FUNCTION INPUT_FUNCTION;

enum INPUT_FUNCTION {
  NULL_INPUT = 0,
  PLAYER_INPUT = 1,
  AI_WALK_UNTIL_BLOCKED = 2,
  AI_HOP_UNTIL_BLOCKED = 3,
  AI_WALK_UNTIL_BLOCKED_OR_LEDGE = 4,
  AI_HOP_UNTIL_BLOCKED_OR_LEDGE = 5,
  AI_FLY_VERTICAL = 6,
  AI_FLY_HORIZONTAL = 7,
};

typedef void (*inputFnPtr)(ENTITY*);
__attribute__(( optimize("Os") ))
static inputFnPtr inputFunc(const INPUT_FUNCTION i)
{
  switch (i) {
  case PLAYER_INPUT:
    return player_input;
  case AI_WALK_UNTIL_BLOCKED:
    return ai_walk_until_blocked;
  case AI_HOP_UNTIL_BLOCKED:
    return ai_hop_until_blocked;
  case AI_WALK_UNTIL_BLOCKED_OR_LEDGE:
    return ai_walk_until_blocked_or_ledge;
  case AI_HOP_UNTIL_BLOCKED_OR_LEDGE:
    return ai_hop_until_blocked_or_ledge;
  case AI_FLY_VERTICAL:
    return ai_fly_vertical;
  case AI_FLY_HORIZONTAL:
    return ai_fly_horizontal;
  default: // NULL_INPUT
    return null_input;
  }
}

enum UPDATE_FUNCTION;
typedef enum UPDATE_FUNCTION UPDATE_FUNCTION;

enum UPDATE_FUNCTION {
  NULL_UPDATE = 0,
  ENTITY_UPDATE = 1,
  ENTITY_UPDATE_FLYING = 2,
  ENTITY_UPDATE_LADDER = 3,
};

typedef void (*updateFnPtr)(ENTITY*);
__attribute__(( optimize("Os") ))
static updateFnPtr updateFunc(const UPDATE_FUNCTION u)
{
  switch (u) {
  case ENTITY_UPDATE:
    return entity_update;
  case ENTITY_UPDATE_FLYING:
    return entity_update_flying;
  case ENTITY_UPDATE_LADDER:
    return entity_update_ladder;
  default: // NULL_UPDATE
    return null_update;
  }
}

enum RENDER_FUNCTION;
typedef enum RENDER_FUNCTION RENDER_FUNCTION;

enum RENDER_FUNCTION {
  NULL_RENDER = 0,
  PLAYER_RENDER = 1,
  LADYBUG_RENDER = 2,
  ANT_RENDER = 3,
  CRICKET_RENDER = 4,
  GRASSHOPPER_RENDER = 5,
  FRUITFLY_RENDER = 6,
  BEE_RENDER = 7,
  SPIDER_RENDER = 8,
};

typedef void (*renderFnPtr)(ENTITY*);
__attribute__(( optimize("Os") ))
static renderFnPtr renderFunc(const RENDER_FUNCTION r)
{
  switch (r) {
  case PLAYER_RENDER:
    return player_render;
  case LADYBUG_RENDER:
    return ladybug_render;
  case ANT_RENDER:
    return ant_render;
  case CRICKET_RENDER:
    return cricket_render;
  case GRASSHOPPER_RENDER:
    return grasshopper_render;
  case FRUITFLY_RENDER:
    return fruitfly_render;
  case BEE_RENDER:
    return bee_render;
  case SPIDER_RENDER:
    return spider_render;
  default: // NULL_RENDER:
    return null_render;
  }
}

enum INITIAL_FLAGS;
typedef enum INITIAL_FLAGS INITIAL_FLAGS;

enum INITIAL_FLAGS {
  IFLAG_LEFT = 1,
  IFLAG_RIGHT = 2,
  IFLAG_UP = 4,
  IFLAG_DOWN = 8,
  IFLAG_AUTORESPAWN = 16,
  IFLAG_NOINTERACT = 32,
  IFLAG_INVINCIBLE = 64,
  IFLAG_SPRITE_FLIP_X = 128,
};

enum GAME_FLAGS;
typedef enum GAME_FLAGS GAME_FLAGS;

enum GAME_FLAGS {
  GFLAG_1P = 1,
  GFLAG_2P = 2,
  GFLAG_P1_VS_P2 = 4,
};

// Parenthesis cannot be placed around this macro expansion
#define LE(i) LO8((i)), HI8((i))

const uint8_t levelData[] PROGMEM = {
  LEVELS,      // uint8_t numLevels

// Include the auto-generated table of level offsets (uint16_t levelOffsets[numLevels])
#include "editor/levels/level_offsets.inc"

/*           // ---------- start of level 0 data */
  1,      // uint8_t theme;

  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP),   // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT,       // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE,     // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER,     // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_RIGHT, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT,  IFLAG_LEFT,      // uint8_t monsterFlags[6]
  LE(WORLD_METER * 6), LE(WORLD_METER * 1), LE(WORLD_METER * 3), LE(WORLD_METER * 2), LE(WORLD_METER * 1), LE(WORLD_METER * 1), // int16_t monsterMaxDX[6]
  3, 25, LE(WORLD_JUMP >> 1), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t monsterImpulse[6]
  AI_FLY_HORIZONTAL, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE_FLYING, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  BEE_RENDER, CRICKET_RENDER, LADYBUG_RENDER, GRASSHOPPER_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]
#include "editor/levels/0000-title_level.xcf.png.inc"


  0,      // uint8_t theme;
  0, 0, // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP),   // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT,       // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE,     // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER,     // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_LEFT, IFLAG_LEFT|IFLAG_AUTORESPAWN, IFLAG_RIGHT, IFLAG_RIGHT, IFLAG_RIGHT|IFLAG_AUTORESPAWN, IFLAG_RIGHT,      // uint8_t monsterFlags[6]
  LE(WORLD_METER * 2), LE(WORLD_METER * 2), LE(WORLD_METER * 3), LE(WORLD_METER * 2), LE(WORLD_METER * 1), LE(WORLD_METER * 2), // int16_t monsterMaxDX[6]
  LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP),    // int16_t monsterImpulse[6]
  AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  ANT_RENDER, ANT_RENDER, ANT_RENDER, ANT_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]
#include "editor/levels/0010-prototype_level.xcf.png.inc"

  1,      // uint8_t theme;
  0, 0, // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP),   // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT,       // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE,     // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER,     // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_DOWN, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT,  IFLAG_LEFT,      // uint8_t monsterFlags[6]
  LE(WORLD_METER * 12), LE(WORLD_METER * 1), LE(WORLD_METER * 3), LE(WORLD_METER * 2), LE(WORLD_METER * 1), LE(WORLD_METER * 1), // int16_t monsterMaxDX[6]
  16, 23, LE(WORLD_JUMP >> 1), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t monsterImpulse[6]
  AI_FLY_VERTICAL, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE_FLYING, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  BEE_RENDER, CRICKET_RENDER, LADYBUG_RENDER, GRASSHOPPER_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]
#include "editor/levels/0020-test_level.xcf.png.inc"

  2,      // uint8_t theme;
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP),   // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT,       // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE,     // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER,     // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_DOWN|IFLAG_SPRITE_FLIP_X, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT,  IFLAG_LEFT,      // uint8_t monsterFlags[6]
  LE(WORLD_METER * 5), LE(WORLD_METER * 1), LE(WORLD_METER * 3), LE(WORLD_METER * 2), LE(WORLD_METER * 1), LE(WORLD_METER * 1), // int16_t monsterMaxDX[6]
  16, 23, LE(WORLD_JUMP >> 1), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t monsterImpulse[6]
  AI_FLY_VERTICAL, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE_FLYING, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  BEE_RENDER, CRICKET_RENDER, LADYBUG_RENDER, GRASSHOPPER_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]
#include "editor/levels/0030-space_level.xcf.png.inc"

 };

#define LEVEL_HEADER_SIZE 1
#define LEVEL_THEME_START 0
#define LEVEL_THEME_SIZE 1
#define LEVEL_PLAYER_INITIAL_FLAGS_START (LEVEL_THEME_START + LEVEL_THEME_SIZE)
#define LEVEL_PLAYER_INITIAL_FLAGS_SIZE 2
#define LEVEL_PLAYER_MAXDX_START (LEVEL_PLAYER_INITIAL_FLAGS_START + LEVEL_PLAYER_INITIAL_FLAGS_SIZE)
#define LEVEL_PLAYER_MAXDX_SIZE (2 * sizeof(int16_t))
#define LEVEL_PLAYER_IMPULSE_START (LEVEL_PLAYER_MAXDX_START + LEVEL_PLAYER_MAXDX_SIZE)
#define LEVEL_PLAYER_IMPULSE_SIZE (2 * sizeof(int16_t))
#define LEVEL_PLAYER_INPUT_START (LEVEL_PLAYER_IMPULSE_START + LEVEL_PLAYER_IMPULSE_SIZE)
#define LEVEL_PLAYER_INPUT_SIZE 2
#define LEVEL_PLAYER_UPDATE_START (LEVEL_PLAYER_INPUT_START + LEVEL_PLAYER_INPUT_SIZE)
#define LEVEL_PLAYER_UPDATE_SIZE 2
#define LEVEL_PLAYER_RENDER_START (LEVEL_PLAYER_UPDATE_START + LEVEL_PLAYER_UPDATE_SIZE)
#define LEVEL_PLAYER_RENDER_SIZE 2
#define LEVEL_MONSTER_INITIAL_FLAGS_START (LEVEL_PLAYER_RENDER_START + LEVEL_PLAYER_RENDER_SIZE)
#define LEVEL_MONSTER_INITIAL_FLAGS_SIZE 6
#define LEVEL_MONSTER_MAXDX_START (LEVEL_MONSTER_INITIAL_FLAGS_START + LEVEL_MONSTER_INITIAL_FLAGS_SIZE)
#define LEVEL_MONSTER_MAXDX_SIZE (6 * sizeof(int16_t))
#define LEVEL_MONSTER_IMPULSE_START (LEVEL_MONSTER_MAXDX_START + LEVEL_MONSTER_MAXDX_SIZE)
#define LEVEL_MONSTER_IMPULSE_SIZE (6 * sizeof(int16_t))
#define LEVEL_MONSTER_INPUT_START (LEVEL_MONSTER_IMPULSE_START + LEVEL_MONSTER_IMPULSE_SIZE)
#define LEVEL_MONSTER_INPUT_SIZE 6
#define LEVEL_MONSTER_UPDATE_START (LEVEL_MONSTER_INPUT_START + LEVEL_MONSTER_INPUT_SIZE)
#define LEVEL_MONSTER_UPDATE_SIZE 6
#define LEVEL_MONSTER_RENDER_START (LEVEL_MONSTER_UPDATE_START + LEVEL_MONSTER_UPDATE_SIZE)
#define LEVEL_MONSTER_RENDER_SIZE 6
#define LEVEL_MAP_START (LEVEL_MONSTER_RENDER_START + LEVEL_MONSTER_RENDER_SIZE)
#define LEVEL_MAP_SIZE 105
#define LEVEL_TREASURE_COUNT_START (LEVEL_MAP_START + LEVEL_MAP_SIZE)
#define LEVEL_TREASURE_COUNT_SIZE 1
#define LEVEL_ONE_WAY_COUNT_START (LEVEL_TREASURE_COUNT_START + LEVEL_TREASURE_COUNT_SIZE)
#define LEVEL_ONE_WAY_COUNT_SIZE 1
#define LEVEL_LADDER_COUNT_START (LEVEL_ONE_WAY_COUNT_START + LEVEL_ONE_WAY_COUNT_SIZE)
#define LEVEL_LADDER_COUNT_SIZE 1
#define LEVEL_FIRE_COUNT_START (LEVEL_LADDER_COUNT_START + LEVEL_LADDER_COUNT_SIZE)
#define LEVEL_FIRE_COUNT_SIZE 1
#define LEVEL_PACKED_COORDINATES_START (LEVEL_FIRE_COUNT_START + LEVEL_FIRE_COUNT_SIZE)

#define numLevels() ((uint8_t)pgm_read_byte(&levelData[0]))
#define levelOffset(level) ((uint16_t)pgm_read_word(&levelData[LEVEL_HEADER_SIZE + ((level) * sizeof(uint16_t))]))
#define theme(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_THEME_START]))
#define playerFlags(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INITIAL_FLAGS_START + (i)]))
#define playerMaxDX(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_PLAYER_MAXDX_START + ((i) * sizeof(int16_t))]))
#define playerImpulse(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_PLAYER_IMPULSE_START + ((i) * sizeof(int16_t))]))
#define playerInput(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INPUT_START + (i)]))
#define playerUpdate(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_UPDATE_START + (i)]))
#define playerRender(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_RENDER_START + (i)]))
#define monsterFlags(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INITIAL_FLAGS_START + (i)]))
#define monsterMaxDX(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_MONSTER_MAXDX_START + ((i) * sizeof(int16_t))]))
#define monsterImpulse(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_MONSTER_IMPULSE_START + ((i) * sizeof(int16_t))]))
#define monsterInput(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INPUT_START + (i)]))
#define monsterUpdate(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_UPDATE_START + (i)]))
#define monsterRender(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_RENDER_START + (i)]))
#define treasureCount(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TREASURE_COUNT_START]))
#define onewayCount(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_ONE_WAY_COUNT_START]))
#define ladderCount(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_LADDER_COUNT_START]))
#define fireCount(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_FIRE_COUNT_START]))

const uint8_t MapTileToLadderTop[] PROGMEM = {
  // If a ladder top overlaps a treasure tile, the treasure gets replaced with open sky and gets a ladder top overlaid
  0 + TREASURE_TO_LADDER_TOP_OFFSET, 1 + TREASURE_TO_LADDER_TOP_OFFSET,
  2 + TREASURE_TO_LADDER_TOP_OFFSET, 3 + TREASURE_TO_LADDER_TOP_OFFSET,
  4 + TREASURE_TO_LADDER_TOP_OFFSET, 5 + TREASURE_TO_LADDER_TOP_OFFSET,
  6 + TREASURE_TO_LADDER_TOP_OFFSET, 7 + TREASURE_TO_LADDER_TOP_OFFSET,
  8 + TREASURE_TO_LADDER_TOP_OFFSET, 9 + TREASURE_TO_LADDER_TOP_OFFSET,
  10 + TREASURE_TO_LADDER_TOP_OFFSET, 11 + TREASURE_TO_LADDER_TOP_OFFSET,
  12 + TREASURE_TO_LADDER_TOP_OFFSET, 13 + TREASURE_TO_LADDER_TOP_OFFSET,
  14 + TREASURE_TO_LADDER_TOP_OFFSET,

  // Open sky tiles get a ladder top overlaid
  15 + SKY_TO_LADDER_TOP_OFFSET, 16 + SKY_TO_LADDER_TOP_OFFSET,
  17 + SKY_TO_LADDER_TOP_OFFSET, 18 + SKY_TO_LADDER_TOP_OFFSET,
  19 + SKY_TO_LADDER_TOP_OFFSET, 20 + SKY_TO_LADDER_TOP_OFFSET,
  21 + SKY_TO_LADDER_TOP_OFFSET, 22 + SKY_TO_LADDER_TOP_OFFSET,
  23 + SKY_TO_LADDER_TOP_OFFSET, 24 + SKY_TO_LADDER_TOP_OFFSET,
  25 + SKY_TO_LADDER_TOP_OFFSET, 26 + SKY_TO_LADDER_TOP_OFFSET,
  27 + SKY_TO_LADDER_TOP_OFFSET, 28 + SKY_TO_LADDER_TOP_OFFSET,
  29 + SKY_TO_LADDER_TOP_OFFSET,

  // Solid tiles get a ladder top overlaid
  30 + SOLID_TO_LADDER_TOP_OFFSET, 31 + SOLID_TO_LADDER_TOP_OFFSET,
  32 + SOLID_TO_LADDER_TOP_OFFSET, 33 + SOLID_TO_LADDER_TOP_OFFSET,
  34 + SOLID_TO_LADDER_TOP_OFFSET, 35 + SOLID_TO_LADDER_TOP_OFFSET,

  // Solid ladder tiles remain solid ladder tiles
  36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,

  // One way tiles get a ladder top overlaid
  48 + ONE_WAY_TO_LADDER_TOP_OFFSET, 49 + ONE_WAY_TO_LADDER_TOP_OFFSET,
  50 + ONE_WAY_TO_LADDER_TOP_OFFSET,
};

const uint8_t MapTileToLadderMiddle[] PROGMEM = {
  // If a ladder middle overlaps a treasure tile, the treasure gets replaced with open sky and gets a ladder middle overlaid
  0 + TREASURE_TO_LADDER_MIDDLE_OFFSET, 1 + TREASURE_TO_LADDER_MIDDLE_OFFSET,
  2 + TREASURE_TO_LADDER_MIDDLE_OFFSET, 3 + TREASURE_TO_LADDER_MIDDLE_OFFSET,
  4 + TREASURE_TO_LADDER_MIDDLE_OFFSET, 5 + TREASURE_TO_LADDER_MIDDLE_OFFSET,
  6 + TREASURE_TO_LADDER_MIDDLE_OFFSET, 7 + TREASURE_TO_LADDER_MIDDLE_OFFSET,
  8 + TREASURE_TO_LADDER_MIDDLE_OFFSET, 9 + TREASURE_TO_LADDER_MIDDLE_OFFSET,
  10 + TREASURE_TO_LADDER_MIDDLE_OFFSET, 11 + TREASURE_TO_LADDER_MIDDLE_OFFSET,
  12 + TREASURE_TO_LADDER_MIDDLE_OFFSET, 13 + TREASURE_TO_LADDER_MIDDLE_OFFSET,
  14 + TREASURE_TO_LADDER_MIDDLE_OFFSET,

  // Open sky tiles get a ladder middle overlaid
  15 + SKY_TO_LADDER_MIDDLE_OFFSET, 16 + SKY_TO_LADDER_MIDDLE_OFFSET,
  17 + SKY_TO_LADDER_MIDDLE_OFFSET, 18 + SKY_TO_LADDER_MIDDLE_OFFSET,
  19 + SKY_TO_LADDER_MIDDLE_OFFSET, 20 + SKY_TO_LADDER_MIDDLE_OFFSET,
  21 + SKY_TO_LADDER_MIDDLE_OFFSET, 22 + SKY_TO_LADDER_MIDDLE_OFFSET,
  23 + SKY_TO_LADDER_MIDDLE_OFFSET, 24 + SKY_TO_LADDER_MIDDLE_OFFSET,
  25 + SKY_TO_LADDER_MIDDLE_OFFSET, 26 + SKY_TO_LADDER_MIDDLE_OFFSET,
  27 + SKY_TO_LADDER_MIDDLE_OFFSET, 28 + SKY_TO_LADDER_MIDDLE_OFFSET,
  29 + SKY_TO_LADDER_MIDDLE_OFFSET,
  
  // Solid tiles get a ladder middle overlaid
  30 + SOLID_TO_LADDER_MIDDLE_OFFSET, 31 + SOLID_TO_LADDER_MIDDLE_OFFSET,
  32 + SOLID_TO_LADDER_MIDDLE_OFFSET, 33 + SOLID_TO_LADDER_MIDDLE_OFFSET,
  34 + SOLID_TO_LADDER_MIDDLE_OFFSET, 35 + SOLID_TO_LADDER_MIDDLE_OFFSET,

  // Solid ladder tiles remain solid ladder tiles
  36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,

  // One way tiles get a ladder top overlaid
  48 + ONE_WAY_TO_LADDER_TOP_OFFSET, 49 + ONE_WAY_TO_LADDER_TOP_OFFSET,
  50 + ONE_WAY_TO_LADDER_TOP_OFFSET, // these map to ladder top tiles, since they need to be one-way tiles
};

__attribute__(( optimize("Os") ))
static void DrawTreasure(const uint8_t x, const uint8_t y)
{
  if (x < SCREEN_TILES_H && y < SCREEN_TILES_V)
    SetTile(x, y, GetTile(x, y) - (FIRST_TREASURE_TILE + THEMES_N * TREASURE_TILES_IN_THEME));
}

__attribute__(( optimize("Os") ))
static void DrawOneWay(const uint8_t y, const uint8_t x1, const uint8_t x2)
{
  if ((y < SCREEN_TILES_V) && (x1 < SCREEN_TILES_H) && (x2 < SCREEN_TILES_H)) {
    for (uint8_t x = x1; x <= x2; ++x) {
      uint8_t t = GetTile(x, y);
      switch (t) {
      case FIRST_SOLID_TILE + 1:
        t = FIRST_ONE_WAY_TILE;
        break;
      case FIRST_SOLID_TILE + 3:
        t = FIRST_ONE_WAY_TILE + 1;
        break;
      case FIRST_SOLID_TILE + 5:
        t = FIRST_ONE_WAY_TILE + 2;
        break;
      default:
        return;
      }
      SetTile(x, y, t);
    }
  }
}

__attribute__(( optimize("Os") ))
static void DrawLadder(const uint8_t x, const uint8_t y1, const uint8_t y2)
{
  if ((x < SCREEN_TILES_H) && (y1 < SCREEN_TILES_V) && (y2 < SCREEN_TILES_V)) {
    // Draw the top of the ladder
    uint8_t t = GetTile(x, y1);
    if (t < NELEMS(MapTileToLadderTop))
      SetTile(x, y1, pgm_read_byte(&MapTileToLadderTop[t]));

    // Draw the rest of the ladder
    for (uint8_t y = y1; y < y2; ++y) {
      t = GetTile(x, y + 1);
      if (t < NELEMS(MapTileToLadderMiddle))
        SetTile(x, y + 1, pgm_read_byte(&MapTileToLadderMiddle[t]));
    }
  }
}

__attribute__(( optimize("Os") ))
static void DrawFire(const uint8_t y, const uint8_t x1, const uint8_t x2, const uint8_t theme)
{  
  if ((y < SCREEN_TILES_V) && (x1 < SCREEN_TILES_H) && (x2 < SCREEN_TILES_H)) {
    for (uint8_t x = x1; x <= x2; ++x) {
      uint8_t t = GetTile(x, y);
      if (t < FIRST_SOLID_TILE)
        SetTile(x, y, FIRST_FIRE_TILE + theme * FIRE_TILES_IN_THEME);
    }
  }
}

#define MAX_PLAYERS 2
#define MAX_MONSTERS 6

static inline void entityInitialXY(const uint16_t levelOffset, const uint8_t i, uint8_t* const x, uint8_t* const y)
{
  const uint8_t* packedCoordinatesStart = &levelData[levelOffset + LEVEL_PACKED_COORDINATES_START];
  *x = PgmPacked5Bit_read(packedCoordinatesStart, i * 2);
  *y = PgmPacked5Bit_read(packedCoordinatesStart, i * 2 + 1);
}

__attribute__(( optimize("Os") ))
static void DrawAllLevelOverlays(const uint16_t levelOffset, const uint8_t theme)
{
  const uint8_t* packedCoordinatesStart = &levelData[levelOffset + LEVEL_PACKED_COORDINATES_START];
  uint16_t packedOffset = MAX_PLAYERS * 2 + MAX_MONSTERS * 2; // 2 coordinates per player, 2 coordinates per monster
  const uint8_t treasures = treasureCount(levelOffset);
  for (uint8_t i = 0; i < treasures; ++i) {
    const uint8_t x = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 2);
    const uint8_t y = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 2 + 1);
    DrawTreasure(x, y);
  }
  packedOffset += treasures * 2; // 2 coordinates per treasure
  const uint8_t oneways = onewayCount(levelOffset);
  for (uint8_t i = 0; i < oneways; ++i) {
    const uint8_t y = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3);
    const uint8_t x1 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 1);
    const uint8_t x2 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 2);
    DrawOneWay(y, x1, x2);
  }
  packedOffset += oneways * 3; // 3 coordinates per oneway
  const uint8_t ladders = ladderCount(levelOffset);
  for (uint8_t i = 0; i < ladders; ++i) {
    const uint8_t x = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3);
    const uint8_t y1 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 1);
    const uint8_t y2 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 2);
    DrawLadder(x, y1, y2);
  }
  packedOffset += ladders * 3; // 3 coordinates per ladder
  const uint8_t fires = fireCount(levelOffset);
  for (uint8_t i = 0; i < fires; ++i) {
    const uint8_t y = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3);
    const uint8_t x1 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 1);
    const uint8_t x2 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 2);
    DrawFire(y, x1, x2, theme);
  }
}

static void DisplayNumber(uint8_t x, const uint8_t y, uint16_t n, const uint8_t pad, const uint8_t theme)
{
  for (uint8_t i = 0; x != 255 && i < pad; ++i, n /= 10)
    SetTile(x--, y, (n % 10) + FIRST_DIGIT_TILE + theme * DIGIT_TILES_IN_THEME);  // get next digit
}

#define BaseMapIsSolid(x, y, levelOffset) (PgmBitArray_readBit(&levelData[(levelOffset) + LEVEL_MAP_START], (y) * SCREEN_TILES_H + (x)))

// Returns offset into levelData PROGMEM array
__attribute__(( optimize("Os") ))
static uint16_t LoadLevel(const uint8_t level, uint8_t* const theme)
{
  // Bounds check level
  if (level >= LEVELS)
    return 0xFFFF; // bogus value

  // Determine the offset into the PROGMEM array where the level data begins
  uint16_t levelOffset = levelOffset(level);

  // Using that offset, read the theme, and draw the base map
  *theme = theme(levelOffset);
  if (*theme >= THEMES_N) // something major went wrong
    return 0xFFFF; // bogus value

  for (uint8_t y = 0; y < SCREEN_TILES_V; ++y) {
    for (uint8_t x = 0; x < SCREEN_TILES_H; ++x) {
      if (BaseMapIsSolid(x, y, levelOffset)) {
        if (y == 0 || BaseMapIsSolid(x, y - 1, levelOffset)) { // if we are the top tile, or there is a solid tile above us
          SetTile(x, y, 0 + FIRST_SOLID_TILE + (*theme * SOLID_TILES_IN_THEME)); // underground tile
        } else {
          SetTile(x, y, 1 + FIRST_SOLID_TILE + (*theme * SOLID_TILES_IN_THEME)); // aboveground tile
        }
      } else { // we are a sky tile
        if (y == SCREEN_TILES_V - 1) { // holes in the bottom border are always full sky tiles
          SetTile(x, y, 0 + FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME) + (*theme * SKY_TILES_IN_THEME)); // full sky tile
        } else { // interior tile
          bool solidLDiag = (bool)((x == 0) || BaseMapIsSolid(x - 1, y + 1, levelOffset));
          bool solidRDiag = (bool)((x == SCREEN_TILES_H - 1) || BaseMapIsSolid(x + 1, y + 1, levelOffset));
          bool solidBelow = BaseMapIsSolid(x, y + 1, levelOffset);

          if (!solidLDiag && !solidRDiag && solidBelow) // island
            SetTile(x, y, 1 + FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME) + (*theme * SKY_TILES_IN_THEME));
          else if (!solidLDiag && solidRDiag && solidBelow) // clear on the left
            SetTile(x, y, 2 + FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME) + (*theme * SKY_TILES_IN_THEME));
          else if (solidLDiag && solidRDiag && solidBelow) // tiles left, below, and right
            SetTile(x, y, 3 + FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME) + (*theme * SKY_TILES_IN_THEME));
          else if (solidLDiag && !solidRDiag && solidBelow) // clear on the right
            SetTile(x, y, 4 + FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME) + (*theme * SKY_TILES_IN_THEME));
          else // clear all around
            SetTile(x, y, 0 + FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME) + (*theme * SKY_TILES_IN_THEME));
        }
      }
    }
  }

  // Overlay treasures, oneways, ladders, and fires
  DrawAllLevelOverlays(levelOffset, *theme);

  return levelOffset;
}

// How many frames to wait between animating treasure
#define BACKGROUND_FRAME_SKIP 16
// Defines the order in which the tileset "rows" are swapped in for animating tiles
const uint8_t backgroundAnimation[] PROGMEM = { 0, 1, 2, 1 };

static void killPlayer(ENTITY* const e)
{
  if (e->invincible)
    return;
  TriggerFx(1, 128, true);
  e->dead = true;
  e->monsterhop = true;
  e->dy = 0;
  e->interacts = false;
  e->input = null_input;
  e->update = entity_update_dying;
}

static void killMonster(ENTITY* const e)
{
  if (e->invincible)
    return;
  TriggerFx(3, 128, true);         // play the monster death sound
  e->dead = true;                  // kill the monster
  e->interacts = false;            // make sure we don't consider the entity again for collisions
  e->input = null_input;           // disable the entity's ai
  e->update = entity_update_dying; // use dying physics
}

static void spawnMonster(ENTITY* const e, const uint16_t levelOffset, const uint8_t i)
{
  uint8_t input = monsterInput(levelOffset, i);
  uint8_t update = monsterUpdate(levelOffset, i);
  uint8_t render = monsterRender(levelOffset, i);
  uint8_t tx;
  uint8_t ty;
  entityInitialXY(levelOffset, MAX_PLAYERS + i, &tx, &ty);
  uint8_t monsterFlags = monsterFlags(levelOffset, i);
  if (tx >= SCREEN_TILES_H || ty >= SCREEN_TILES_V) {
    input = NULL_INPUT;
    update = NULL_UPDATE;
    render = NULL_RENDER;
    tx = ty = 0;
    monsterFlags |= IFLAG_NOINTERACT;
  }
  entity_init(e,
              inputFunc(input),
              updateFunc(update),
              renderFunc(render),
              PLAYERS + i,
              (int16_t)(tx * (TILE_WIDTH << FP_SHIFT)),
              (int16_t)(ty * (TILE_HEIGHT << FP_SHIFT)),
              (int16_t)(monsterMaxDX(levelOffset, i)),
              (int16_t)(monsterImpulse(levelOffset, i)));
  // The cast to bool is necessary to properly set bit flags
  e->left = (bool)(monsterFlags & IFLAG_LEFT);
  e->right = (bool)(monsterFlags & IFLAG_RIGHT);
  e->up = (bool)(monsterFlags & IFLAG_UP);
  e->down = (bool)(monsterFlags & IFLAG_DOWN);
  e->autorespawn = (bool)(monsterFlags & IFLAG_AUTORESPAWN);
  e->interacts = (bool)!(monsterFlags & IFLAG_NOINTERACT);
  e->invincible = (bool)(monsterFlags & IFLAG_INVINCIBLE);
  sprites[e->tag].flags = (monsterFlags & IFLAG_SPRITE_FLIP_X) ? SPRITE_FLIP_X : 0;
  e->render(e);
}

static void spawnPlayer(PLAYER* const p, const uint16_t levelOffset, const uint8_t i, const uint8_t gameType)
{
      uint8_t input = playerInput(levelOffset, i);
      uint8_t update = playerUpdate(levelOffset, i);
      uint8_t render = playerRender(levelOffset, i);
      uint8_t tx;
      uint8_t ty;
      entityInitialXY(levelOffset, i, &tx, &ty);
      uint8_t playerFlags = playerFlags(levelOffset, i);
      if (tx >= SCREEN_TILES_H || ty >= SCREEN_TILES_V || ((i == 1) && (gameType & GFLAG_1P))) {
        input = NULL_INPUT;
        update = NULL_UPDATE;
        render = NULL_RENDER;
        tx = ty = 0;
        playerFlags |= IFLAG_NOINTERACT;
      } else if (gameType & GFLAG_P1_VS_P2) {
        playerFlags |= IFLAG_AUTORESPAWN;
      }
      player_init(p,
                  inputFunc(input),
                  updateFunc(update),
                  renderFunc(render), i,
                  (int16_t)(tx * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(ty * (TILE_HEIGHT << FP_SHIFT)),
                  (int16_t)(playerMaxDX(levelOffset, i)),
                  (int16_t)(playerImpulse(levelOffset, i)));
      ENTITY* const e = (ENTITY*)p;
      // The cast to bool is necessary to properly set bit flags
      //e->left = (bool)(playerFlags & IFLAG_LEFT);
      //e->right = (bool)(playerFlags & IFLAG_RIGHT);
      //e->up = (bool)(playerFlags & IFLAG_UP);
      //e->down = (bool)(playerFlags & IFLAG_DOWN);
      e->autorespawn = (bool)(playerFlags & IFLAG_AUTORESPAWN);
      e->interacts = (bool)!(playerFlags & IFLAG_NOINTERACT);
      e->invincible = (bool)(playerFlags & IFLAG_INVINCIBLE);
      sprites[e->tag].flags = (playerFlags & IFLAG_SPRITE_FLIP_X) ? SPRITE_FLIP_X : 0;
      e->render(e);
}

__attribute__(( always_inline ))
static inline bool overlap(const int16_t x1, const int16_t y1, const uint8_t w1, const uint8_t h1, const int16_t x2, const int16_t y2, const uint8_t w2, const uint8_t h2)
{
  return !(((x1 + w1 - 1) < x2) ||
           ((x2 + w2 - 1) < x1) ||
           ((y1 + h1 - 1) < y2) ||
           ((y2 + h2 - 1) < y1));
}

/* volatile uint8_t globalFrameCounter = 0; */

/* void VsyncCallBack() */
/* { */
/*   ++globalFrameCounter; */
/* } */

const uint8_t copyright[] PROGMEM = {
  LAST_FIRE_TILE + 10, FIRST_SKY_TILE + SKY_TILES_IN_THEME, FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME + 2,
  FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME, FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME + 1,
  FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME + 5, FIRST_SKY_TILE + SKY_TILES_IN_THEME,
  LAST_FIRE_TILE + 4, LAST_FIRE_TILE + 5, LAST_FIRE_TILE + 6, LAST_FIRE_TILE + 6,
  FIRST_SKY_TILE + SKY_TILES_IN_THEME, LAST_FIRE_TILE + 1, LAST_FIRE_TILE + 5,
  LAST_FIRE_TILE + 7, LAST_FIRE_TILE + 8, LAST_FIRE_TILE + 9, LAST_FIRE_TILE + 7,
  LAST_FIRE_TILE + 5,
};

const uint8_t p1_vs_p2[] PROGMEM = {
  LAST_FIRE_TILE + 1, FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME + 1, FIRST_SKY_TILE + SKY_TILES_IN_THEME,
  LAST_FIRE_TILE + 2, LAST_FIRE_TILE + 3, FIRST_SKY_TILE + SKY_TILES_IN_THEME,
  LAST_FIRE_TILE + 1, FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME + 2,
};

__attribute__(( optimize("Os") ))
static GAME_FLAGS doTitleScreen(ENTITY* const monster)
{
  // Switch to the last animation row, where the title screen tiles are
  SetTileTable(tileset + 64 * ((TILESET_SIZE - TITLE_SCREEN_TILES) / 3) * 2);

  for (uint8_t i = 0; i < NELEMS(copyright); ++i)
    SetTile(5 + i, 13, pgm_read_byte(&copyright[i]));

  for (uint8_t i = 0; i < 2; ++i) {
    SetTile(11, 17 + i * 2, FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME + 1 + i);
    SetTile(12, 17 + i * 2, LAST_FIRE_TILE + 1);
  }

  for (uint8_t i = 0; i < NELEMS(p1_vs_p2); ++i)
    SetTile(11 + i, 21, pgm_read_byte(&p1_vs_p2[i]));
  
  // Set pointer to 1P
  uint8_t selection = 0;

  uint16_t prev = 0;
  uint16_t held = 0;

  bool fadedIn = false;
  bool wasReleased = false;

  sprites[0].x -= 4;
  
  for (;;) {
    sprites[0].y = (vt2p((17 + selection * 2)) + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;

    for (uint8_t i = 0; i < MONSTERS; ++i)
      monster[i].input(&monster[i]);
    for (uint8_t i = 0; i < MONSTERS; ++i)
      monster[i].update(&monster[i]);
    for (uint8_t i = 0; i < MONSTERS; ++i)
      monster[i].render(&monster[i]);

    if (!fadedIn) {
      FadeIn(0, true);
      SetSpriteVisibility(true);
      fadedIn = true;
    }

    prev = held;
    held = ReadJoypad(0);
    uint16_t pressed = held & (held ^ prev);
    uint16_t released = prev & (held ^ prev);

    // Check for level select buttons (hold select, and press a left or right shoulder button)
    if (pressed & BTN_DOWN) {
      TriggerFx(3, 128, true);
      if (++selection == 3)
        selection = 0;
    }
    if (pressed & BTN_UP) {
      TriggerFx(3, 128, true);
      if (--selection == 255)
        selection = 2;
    }
    if ((held & BTN_START) == 0)
      wasReleased = true;

    if ((pressed & BTN_START) && wasReleased) {
      TriggerFx(2, 128, true);
      for (uint8_t j = 0; j < 3; ++j)
        if (j != selection)
          for (uint8_t i = 11; i < 19; ++i)
            SetTile(i, 17 + j * 2, FIRST_SKY_TILE + SKY_TILES_IN_THEME);

      WaitVsync(32);
      switch (selection) {
      case 0:
        return GFLAG_1P;
      case 1:
        return GFLAG_2P;
      default: // case 2
        return GFLAG_P1_VS_P2;
      }
    }

    WaitVsync(1);
  }
}

int main()
{
  PLAYER player[PLAYERS];
  ENTITY monster[MONSTERS];

  uint8_t currentLevel;
  uint16_t levelOffset;
  uint8_t theme;
  uint8_t backgroundFrameCounter;
  uint16_t timer;
  uint8_t gameType;

  SetSpritesTileBank(0, mysprites);
  InitMusicPlayer(patches);

 begin:
  currentLevel = levelOffset = theme = 0;
  gameType = GFLAG_1P;

  for (;;) {
    SetSpriteVisibility(false);
    FadeOut(0, true);
    SetTileTable(tileset);

    levelOffset = LoadLevel(currentLevel, &theme);

    /* SetUserPostVsyncCallback(&VsyncCallBack);   */

    // Check the return value of LoadLevel
    if (levelOffset == 0xFFFF)
      goto begin;

    backgroundFrameCounter = 0;
    timer = -1;

    // Initialize players
    for (uint8_t i = 0; i < PLAYERS; ++i)
      spawnPlayer(&player[i], levelOffset, i, gameType);

/* #if (PLAYERS == 2) */
/*     // Player 2 starts off hidden and disabled */
/*     ((ENTITY*)(&player[1]))->render(((ENTITY*)(&player[1]))); // setup sprite */
/*     ((ENTITY*)(&player[1]))->interacts = false; */
/*     ((ENTITY*)(&player[1]))->render = null_render; */
/*     ((ENTITY*)(&player[1]))->invincible = true; */
/* #endif // (PLAYERS == 2) */

    // Initialize monsters
    for (uint8_t i = 0; i < MONSTERS; ++i)
      spawnMonster(&monster[i], levelOffset, i);

    if (currentLevel == 0) {
      gameType = doTitleScreen(monster);
      currentLevel = 1;
      continue;
    } else {
      FadeIn(0, true);
      SetSpriteVisibility(true);
    }

    // Display the level number
    DisplayNumber(3, 0, currentLevel, 2, theme);

    // Main game loop
    for (;;) {
      /* static uint8_t localFrameCounter; */
      WaitVsync(1);

      // Animate all background tiles at once by modifying the tileset pointer
      if ((backgroundFrameCounter % BACKGROUND_FRAME_SKIP) == 0) {
        SetTileTable(tileset + 64 * ((TILESET_SIZE - TITLE_SCREEN_TILES) / 3) * pgm_read_byte(&backgroundAnimation[backgroundFrameCounter / BACKGROUND_FRAME_SKIP]));
        DisplayNumber(8, 0, ++timer, 4, theme); // increment the in-game time display
      }
      // Compile-time assert that we are working with a power of 2
      BUILD_BUG_ON(isNotPowerOf2(BACKGROUND_FRAME_SKIP * NELEMS(backgroundAnimation)));
      backgroundFrameCounter = (backgroundFrameCounter + 1) & (BACKGROUND_FRAME_SKIP * NELEMS(backgroundAnimation) - 1);

      // Display debugging information
      /* uint16_t sc = StackCount(); */
      /* DisplayNumber(SCREEN_TILES_H - 3, 0, sc, 4, theme); */
      /* DisplayNumber(2, 0, globalFrameCounter, 3); */
      /* DisplayNumber(6, 0, localFrameCounter++, 3); */
      /* DisplayNumber(4, 0, (uint16_t)tracks[1].patchCommandStreamPos, 5); */
      /* DisplayNumber(8, 0, (uint16_t)tracks[1].patchCurrDeltaTime, 3); */
      /* DisplayNumber(12, 0, (uint16_t)tracks[1].patchNextDeltaTime, 3); */
      /* DisplayNumber(18, 0, (uint16_t)tracks[2].patchCommandStreamPos, 5); */
      /* DisplayNumber(22, 0, (uint16_t)tracks[2].patchCurrDeltaTime, 3); */
      /* DisplayNumber(26, 0, (uint16_t)tracks[2].patchNextDeltaTime, 3); */
      /* DisplayNumber(23, 0, currentLevel, 3); */
      /* //DisplayNumber(SCREEN_TILES_H - 1, SCREEN_TILES_V - 1, levelOffset, 5); */

      // Get the inputs for every entity
      for (uint8_t i = 0; i < PLAYERS; ++i)
        ((ENTITY*)(&player[i]))->input((ENTITY*)(&player[i]));
      for (uint8_t i = 0; i < MONSTERS; ++i)
        monster[i].input(&monster[i]);

      // Proper kill detection requires the previous Y value for each entity
      int16_t playerPrevY[PLAYERS];

      // Update the state of the players
      for (uint8_t i = 0; i < PLAYERS; ++i) {
        playerPrevY[i] = ((ENTITY*)(&player[i]))->y; // cache the previous Y value to use for kill detection below
        ((ENTITY*)(&player[i]))->update((ENTITY*)(&player[i]));
      }

#if (PLAYERS == 2)
      // Collision check between players (only in versus mode)
      if (gameType & GFLAG_P1_VS_P2) {
        ENTITY* p1 = (ENTITY*)(&player[0]);
        ENTITY* p2 = (ENTITY*)(&player[1]);
        if (p1->interacts && !p1->dead && p2->interacts && !p2->dead &&
            overlap(p1->x, p1->y, WORLD_METER, WORLD_METER, p2->x, p2->y, WORLD_METER, WORLD_METER)) {
          if (((playerPrevY[0] + WORLD_METER - (1 << FP_SHIFT)) < (playerPrevY[1])) && !p2->invincible) {
            killPlayer(p2);
            p2->monsterhop = false; // die like a bug
            if (p1->update == entity_update)
              p1->monsterhop = true;
          } else if (((playerPrevY[1] + WORLD_METER - (1 << FP_SHIFT)) < (playerPrevY[0])) && !p1->invincible) {
            killPlayer(p1);
            p1->monsterhop = false; // die like a bug
            if (p2->update == entity_update)
              p2->monsterhop = true;
          }
        }
      }
#endif

      // Update the state of the monsters, and perform collision detection with each player
      for (uint8_t i = 0; i < MONSTERS; ++i) {
        int16_t monsterPrevY = monster[i].y; // cache the previous Y value to use for kill detection below
        monster[i].update(&monster[i]);

        // Collision detection (calculation assumes each sprite is WORLD_METER wide, and uses a shrunken hitbox for the monster)
        for (uint8_t p = 0; p < PLAYERS; ++p) {
          ENTITY* const e = (ENTITY*)(&player[p]);
          if (monster[i].interacts && !monster[i].dead && e->interacts && !e->dead &&
              overlap(e->x, e->y, WORLD_METER, WORLD_METER,
                      monster[i].x + (1 << FP_SHIFT),
                      monster[i].y + (3 << FP_SHIFT),
                      WORLD_METER - (2 << FP_SHIFT), WORLD_METER - (4 << FP_SHIFT))) {
            // If a player and a monster overlap, and the bottom pixel of the player's previous Y is above the top
            // of the monster's previous Y then the player kills the monster, otherwise the monster kills the player.
            if (((playerPrevY[p] + WORLD_METER - (1 << FP_SHIFT)) <= (monsterPrevY + (3 << FP_SHIFT))) && !monster[i].invincible) {
              killMonster(&monster[i]);
              if (e->update == entity_update)
                e->monsterhop = true; // player should now do the monster hop, but only if gravity applies
            } else {
              killPlayer(e);
            }
          }
        }
      }

      // Render every entity
      for (uint8_t i = 0; i < PLAYERS; ++i)
        ((ENTITY*)(&player[i]))->render((ENTITY*)(&player[i]));
      for (uint8_t i = 0; i < MONSTERS; ++i)
        monster[i].render(&monster[i]);

      // Check if the dead flag has been set for a monster and/or if we need to respawn a monster
      for (uint8_t i = 0; i < MONSTERS; ++i) {
        if (monster[i].interacts && monster[i].dead)
          killMonster(&monster[i]);
        if (monster[i].dead && monster[i].autorespawn && monster[i].render == null_render) // monster is dead, and its dying animation has finished
          spawnMonster(&monster[i], levelOffset, i);
      }

      // Check if the dead flag has been set for a player and/or if we need to respawn a player
      for (uint8_t i = 0; i < PLAYERS; ++i) {
        ENTITY* const e = (ENTITY*)&player[i];
        if (e->interacts && e->dead)
          killPlayer(e);
        if (e->dead && e->autorespawn && e->render == null_render) // player is dead, and its dying animation has finished
          spawnPlayer(&player[i], levelOffset, i, gameType);
      }

      // Check for environmental collisions (treasure, fire) by looping over the interacting players, converting
      // their (x, y) coordinates into tile coordinates, and checking any overlapping tiles.
      for (uint8_t i = 0; i < PLAYERS; ++i) {
        ENTITY* const e = (ENTITY*)(&player[i]);
        if (e->interacts && !e->dead) {
          uint8_t tx = p2ht(e->x);
          uint8_t ty = p2vt(e->y);
          /* if (tx >= SCREEN_TILES_H || ty >= SCREEN_TILES_V) // bounds check to prevent writing outside of VRAM */
          /*   continue; */

          bool nx = nh(e->x) && (tx + 1 < SCREEN_TILES_H);  // true if entity overlaps right
          bool ny = nv(e->y) && (ty + 1 < SCREEN_TILES_V);  // true if entity overlaps below

          bool collectedTreasure = false;
          bool killedByFire = false;

          uint8_t t = GetTile(tx, ty);
          if (isTreasure(t)) {
            SetTile(tx, ty, t + TREASURE_TO_SKY_OFFSET);
            collectedTreasure = true;
          } else if (isFire(t)) {
            if (overlap(e->x, e->y, WORLD_METER, WORLD_METER,
                        ht2p(tx    ) + (1 << FP_SHIFT),
                        vt2p(ty    ) + (5 << FP_SHIFT),
                        WORLD_METER - (2 << FP_SHIFT), WORLD_METER - (3 << FP_SHIFT)))
              killedByFire = true;
          }
          t = GetTile(tx + 1, ty);
          if (nx) {
            if (isTreasure(t)) {
              SetTile(tx + 1, ty, t + TREASURE_TO_SKY_OFFSET);
              collectedTreasure = true;
            } else if (isFire(t)) {
              if (overlap(e->x, e->y, WORLD_METER, WORLD_METER,
                          ht2p(tx + 1) + (1 << FP_SHIFT),
                          vt2p(ty    ) + (5 << FP_SHIFT),
                          WORLD_METER - (2 << FP_SHIFT), WORLD_METER - (3 << FP_SHIFT)))
                killedByFire = true;
            }
          }
          t = GetTile(tx, ty + 1);
          if (ny) {
            if (isTreasure(t)) {
              SetTile(tx, ty + 1, t + TREASURE_TO_SKY_OFFSET);
              collectedTreasure = true;
            } else if (isFire(t)) {
              if (overlap(e->x, e->y, WORLD_METER, WORLD_METER,
                          ht2p(tx    ) + (1 << FP_SHIFT),
                          vt2p(ty + 1) + (5 << FP_SHIFT),
                          WORLD_METER - (2 << FP_SHIFT), WORLD_METER - (3 << FP_SHIFT)))
                killedByFire = true;
            }
          }
          t = GetTile(tx + 1, ty + 1);
          if (nx && ny) {
            if (isTreasure(t)) {
              SetTile(tx + 1, ty + 1, t + TREASURE_TO_SKY_OFFSET);
              collectedTreasure = true;
            } else if (isFire(t)) {
              if (overlap(e->x, e->y, WORLD_METER, WORLD_METER,
                          ht2p(tx + 1) + (1 << FP_SHIFT),
                          vt2p(ty + 1) + (5 << FP_SHIFT),
                          WORLD_METER - (2 << FP_SHIFT), WORLD_METER - (3 << FP_SHIFT)))
                killedByFire = true;
            }
          }
          if (collectedTreasure)
            TriggerFx(2, 128, true);
          if (killedByFire && !e->invincible)
            e->dead = true;
        }
      }


      // Read joypad independently of the players, because when a player dies, their joypad is no longer polled
      static uint16_t prev;
      static uint16_t held;
      prev = held;
      held = ReadJoypad(0);
      uint16_t pressed = held & (held ^ prev);

      // Check for level select buttons (hold select, and press a left or right shoulder button)
      if (held & BTN_SELECT) {
        if (pressed & BTN_SL) {
          if (--currentLevel == 0)
            currentLevel = LEVELS - 1;
          break; // load previous level
        } else if (pressed & BTN_SR) {
          if (++currentLevel == LEVELS)
            currentLevel = 1;
          break; // load next level
        } else if (pressed & BTN_START) {
          currentLevel = 0;
          break; // return to title screen
        }
      }

      // Check for level restart button
      if (pressed & BTN_START)
        break; // restart level

    }
  }
}
