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

#define BitArray_numBytes(bits) (((bits) >> 3) + 1 * (((bits) & 7) ? 1 : 0))
#define BitArray_setBit(array, index) ((array)[(index) >> 3] |= (1 << ((index) & 7)))
#define BitArray_clearBit(array, index) ((array)[(index) >> 3] &= ((1 << ((index) & 7)) ^ 0xFF))
#define BitArray_readBit(array, index) ((bool)((array)[(index) >> 3] & (1 << ((index) & 7))))

#define PgmBitArray_readBit(array, index) ((bool)(pgm_read_byte((array)+((index) >> 3)) & (1 << ((index) & 7))))

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
inputFnPtr inputFunc(INPUT_FUNCTION i)
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
  PLAYER_UPDATE = 1,
  ENTITY_UPDATE = 2,
  ENTITY_UPDATE_FLYING = 3,
};

typedef void (*updateFnPtr)(ENTITY*);
updateFnPtr updateFunc(UPDATE_FUNCTION u)
{
  switch (u) {
  case PLAYER_UPDATE:
    return player_update;
  case ENTITY_UPDATE:
    return entity_update;
  case ENTITY_UPDATE_FLYING:
    return entity_update_flying;
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
renderFnPtr renderFunc(RENDER_FUNCTION r)
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

enum MONSTER_FLAGS;
typedef enum MONSTER_FLAGS MONSTER_FLAGS;

enum MONSTER_FLAGS {
  MFLAG_LEFT = 1,
  MFLAG_RIGHT = 2,
  MFLAG_UP = 4,
  MFLAG_DOWN = 8,
  MFLAG_AUTORESPAWN = 16,
  MFLAG_NOINTERACT = 32,
  MFLAG_INVINCIBLE = 64,
};

// Parenthesis cannot be placed around this macro expansion
#define LO8HI8(i) LO8((i)), HI8((i))

const uint8_t levelData[] PROGMEM = {
  3,      // uint8_t numLevels
  LO8HI8(7),   // uint16_t levelOffsets[numLevels] = offsets to each level (little endian)
  LO8HI8(256),
  LO8HI8(505),
  //LO8HI8(756),

/*           // ---------- start of level 0 data */
/*   1,      // uint8_t theme; */

/* #include "editor/levels/title_level.png.inc" */

/*   MFLAG_RIGHT, MFLAG_LEFT, MFLAG_LEFT, MFLAG_LEFT, MFLAG_LEFT,  MFLAG_LEFT,      // uint8_t monsterFlags[6] */
/*   LO8HI8(WORLD_MAXDX), LO8HI8(WORLD_MAXDX),                  // int16_t playerMaxDX[2] */
/*   LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE),    // int16_t playerImpulse[2] */
/*   PLAYER_INPUT, PLAYER_INPUT,       // INPUT_FUNCTIONS playerInputFuncs[2] */
/*   PLAYER_UPDATE, PLAYER_UPDATE,   // UPDATE_FUNCTIONS playerUpdateFuncs[2] */
/*   PLAYER_RENDER, PLAYER_RENDER,   // RENDER_FUNCTIONS playerRenderFuncs[2] */
/*   LO8HI8(WORLD_METER * 6), LO8HI8(WORLD_METER * 1), LO8HI8(WORLD_METER * 3), LO8HI8(WORLD_METER * 2), LO8HI8(WORLD_METER * 1), LO8HI8(WORLD_METER * 1), // int16_t monsterMaxDX[6] */
/*   3, 25, LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), // int16_t monsterImpulse[6] */
/*   AI_FLY_HORIZONTAL, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6] */
/*   ENTITY_UPDATE_FLYING, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6] */
/*   BEE_RENDER, CRICKET_RENDER, LADYBUG_RENDER, GRASSHOPPER_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6] */


          // ---------- start of level 1 data
  0,      // uint8_t theme;

#include "editor/levels/prototype_level.png.inc"

  MFLAG_LEFT, MFLAG_LEFT|MFLAG_AUTORESPAWN, MFLAG_RIGHT, MFLAG_RIGHT, MFLAG_RIGHT|MFLAG_AUTORESPAWN, MFLAG_RIGHT,      // uint8_t monsterFlags[6]
  LO8HI8(WORLD_MAXDX), LO8HI8(WORLD_MAXDX),                                // int16_t playerMaxDX[2]
  LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE),    // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT,     // INPUT_FUNCTIONS playerInputFuncs[2]
  PLAYER_UPDATE, PLAYER_UPDATE,   // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER,   // RENDER_FUNCTIONS playerRenderFuncs[2]
  LO8HI8(WORLD_METER * 2), LO8HI8(WORLD_METER * 2), LO8HI8(WORLD_METER * 3), LO8HI8(WORLD_METER * 2), LO8HI8(WORLD_METER * 1), LO8HI8(WORLD_METER * 2), // int16_t monsterMaxDX[6]
  LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE >> 1),    // int16_t monsterImpulse[6]
  AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  ANT_RENDER, ANT_RENDER, ANT_RENDER, ANT_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]

  1,      // uint8_t theme;
#include "editor/levels/test_level.png.inc"

  /* 255, 255, 255, 254, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 255, 0, 1, 128, 0, 0, 6, 0, 0, 15, 24, 0, 0, 0, 127, 255, 255, 255, // uint8_t map[105]; */
  /* 1, 2,   // uint8_t playerInitialX[2] */
  /* 26, 26, // uint8_t playerInitialY[2] */
  /* 25, 28,  9, 16, 19,  7,      // uint8_t monsterInitialX[6] */
  /* 12, 22, 19, 17,  8, 25,      // uint8_t monsterInitialY[6] */
  /* 12, // uint8_t treasureCount */
  /* 1,  7,  4, 12, 18,  6, 24, 27, 21, 28, 13, 18, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // uint8_t treasureX[32] */
  /* 24, 5,  8, 11, 17,  3,  4, 18,  7, 12, 22, 23, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // uint8_t treasureY[32] */
  MFLAG_DOWN, MFLAG_LEFT, MFLAG_LEFT, MFLAG_LEFT, MFLAG_LEFT,  MFLAG_LEFT,      // uint8_t monsterFlags[6]
  LO8HI8(WORLD_MAXDX), LO8HI8(WORLD_MAXDX),                  // int16_t playerMaxDX[2]
  LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE),    // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT,     // INPUT_FUNCTIONS playerInputFuncs[2]
  PLAYER_UPDATE, PLAYER_UPDATE,   // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER,   // RENDER_FUNCTIONS playerRenderFuncs[2]
  LO8HI8(WORLD_METER * 12), LO8HI8(WORLD_METER * 1), LO8HI8(WORLD_METER * 3), LO8HI8(WORLD_METER * 2), LO8HI8(WORLD_METER * 1), LO8HI8(WORLD_METER * 1), // int16_t monsterMaxDX[6]
  16, 23, LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), // int16_t monsterImpulse[6]
  AI_FLY_VERTICAL, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE_FLYING, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  SPIDER_RENDER, CRICKET_RENDER, LADYBUG_RENDER, GRASSHOPPER_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]



  2,      // uint8_t theme;
#include "editor/levels/space_level.png.inc"

  /* 255, 255, 255, 254, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 255, 0, 1, 128, 0, 0, 6, 0, 0, 15, 24, 0, 0, 0, 127, 255, 255, 255, // uint8_t map[105]; */
  /* 1, 2,   // uint8_t playerInitialX[2] */
  /* 26, 26, // uint8_t playerInitialY[2] */
  /* 25, 28,  9, 16, 19,  7,      // uint8_t monsterInitialX[6] */
  /* 12, 22, 19, 17,  8, 25,      // uint8_t monsterInitialY[6] */
  /* 12, // uint8_t treasureCount */
  /* 1,  7,  4, 12, 18,  6, 24, 27, 21, 28, 13, 18, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // uint8_t treasureX[32] */
  /* 24, 5,  8, 11, 17,  3,  4, 18,  7, 12, 22, 23, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // uint8_t treasureY[32] */
  MFLAG_DOWN|MFLAG_INVINCIBLE, MFLAG_LEFT, MFLAG_LEFT, MFLAG_LEFT, MFLAG_LEFT,  MFLAG_LEFT,      // uint8_t monsterFlags[6]
  LO8HI8(WORLD_MAXDX), LO8HI8(WORLD_MAXDX),                  // int16_t playerMaxDX[2]
  LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE),    // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT,     // INPUT_FUNCTIONS playerInputFuncs[2]
  PLAYER_UPDATE, PLAYER_UPDATE,   // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER,   // RENDER_FUNCTIONS playerRenderFuncs[2]
  LO8HI8(WORLD_METER * 5), LO8HI8(WORLD_METER * 1), LO8HI8(WORLD_METER * 3), LO8HI8(WORLD_METER * 2), LO8HI8(WORLD_METER * 1), LO8HI8(WORLD_METER * 1), // int16_t monsterMaxDX[6]
  16, 23, LO8HI8(WORLD_JUMP_IMPULSE >> 1), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), LO8HI8(WORLD_JUMP_IMPULSE), // int16_t monsterImpulse[6]
  AI_FLY_VERTICAL, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE_FLYING, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  BEE_RENDER, CRICKET_RENDER, LADYBUG_RENDER, GRASSHOPPER_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]


 };

#define LEVEL_HEADER_SIZE 1
#define LEVEL_THEME_START 0
#define LEVEL_THEME_SIZE 1
#define LEVEL_MAP_START (LEVEL_THEME_START + LEVEL_THEME_SIZE)
#define LEVEL_MAP_SIZE 105
#define LEVEL_PLAYER_INITIAL_X_START (LEVEL_MAP_START + LEVEL_MAP_SIZE)
#define LEVEL_PLAYER_INITIAL_X_SIZE 2
#define LEVEL_PLAYER_INITIAL_Y_START (LEVEL_PLAYER_INITIAL_X_START + LEVEL_PLAYER_INITIAL_X_SIZE)
#define LEVEL_PLAYER_INITIAL_Y_SIZE 2
#define LEVEL_MONSTER_INITIAL_X_START (LEVEL_PLAYER_INITIAL_Y_START + LEVEL_PLAYER_INITIAL_Y_SIZE)
#define LEVEL_MONSTER_INITIAL_X_SIZE 6
#define LEVEL_MONSTER_INITIAL_Y_START (LEVEL_MONSTER_INITIAL_X_START + LEVEL_MONSTER_INITIAL_X_SIZE)
#define LEVEL_MONSTER_INITIAL_Y_SIZE 6
#define LEVEL_TREASURE_COUNT_START (LEVEL_MONSTER_INITIAL_Y_START + LEVEL_MONSTER_INITIAL_Y_SIZE)
#define LEVEL_TREASURE_COUNT_SIZE 1
#define LEVEL_TREASURE_X_START (LEVEL_TREASURE_COUNT_START + LEVEL_TREASURE_COUNT_SIZE)
#define LEVEL_TREASURE_X_SIZE 32
#define LEVEL_TREASURE_Y_START (LEVEL_TREASURE_X_START + LEVEL_TREASURE_X_SIZE)
#define LEVEL_TREASURE_Y_SIZE 32
#define LEVEL_MONSTER_FLAGS_START (LEVEL_TREASURE_Y_START + LEVEL_TREASURE_Y_SIZE)
#define LEVEL_MONSTER_FLAGS_SIZE 6
#define LEVEL_PLAYER_MAXDX_START (LEVEL_MONSTER_FLAGS_START + LEVEL_MONSTER_FLAGS_SIZE)
#define LEVEL_PLAYER_MAXDX_SIZE (2 * sizeof(int16_t))
#define LEVEL_PLAYER_IMPULSE_START (LEVEL_PLAYER_MAXDX_START + LEVEL_PLAYER_MAXDX_SIZE)
#define LEVEL_PLAYER_IMPULSE_SIZE (2 * sizeof(int16_t))
#define LEVEL_PLAYER_INPUT_START (LEVEL_PLAYER_IMPULSE_START + LEVEL_PLAYER_IMPULSE_SIZE)
#define LEVEL_PLAYER_INPUT_SIZE 2
#define LEVEL_PLAYER_UPDATE_START (LEVEL_PLAYER_INPUT_START + LEVEL_PLAYER_INPUT_SIZE)
#define LEVEL_PLAYER_UPDATE_SIZE 2
#define LEVEL_PLAYER_RENDER_START (LEVEL_PLAYER_UPDATE_START + LEVEL_PLAYER_UPDATE_SIZE)
#define LEVEL_PLAYER_RENDER_SIZE 2
#define LEVEL_MONSTER_MAXDX_START (LEVEL_PLAYER_RENDER_START + LEVEL_PLAYER_RENDER_SIZE)
#define LEVEL_MONSTER_MAXDX_SIZE (6 * sizeof(int16_t))
#define LEVEL_MONSTER_IMPULSE_START (LEVEL_MONSTER_MAXDX_START + LEVEL_MONSTER_MAXDX_SIZE)
#define LEVEL_MONSTER_IMPULSE_SIZE (6 * sizeof(int16_t))
#define LEVEL_MONSTER_INPUT_START (LEVEL_MONSTER_IMPULSE_START + LEVEL_MONSTER_IMPULSE_SIZE)
#define LEVEL_MONSTER_INPUT_SIZE 6
#define LEVEL_MONSTER_UPDATE_START (LEVEL_MONSTER_INPUT_START + LEVEL_MONSTER_INPUT_SIZE)
#define LEVEL_MONSTER_UPDATE_SIZE 6
#define LEVEL_MONSTER_RENDER_START (LEVEL_MONSTER_UPDATE_START + LEVEL_MONSTER_UPDATE_SIZE)
#define LEVEL_MONSTER_RENDER_SIZE 6

#define numLevels() ((uint8_t)pgm_read_byte(&levelData[0]))
#define levelOffset(level) ((uint16_t)pgm_read_word(&levelData[LEVEL_HEADER_SIZE + ((level) * sizeof(uint16_t))]))
#define theme(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_THEME_START]))
#define compressedMapChunk(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MAP_START + (i)]))
#define playerInitialX(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INITIAL_X_START + (i)]))
#define playerInitialY(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INITIAL_Y_START + (i)]))
#define monsterInitialX(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INITIAL_X_START + (i)]))
#define monsterInitialY(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INITIAL_Y_START + (i)]))
#define monsterFlags(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_FLAGS_START + (i)]))
#define treasureCount(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TREASURE_COUNT_START]))
#define treasureX(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TREASURE_X_START + (i)]))
#define treasureY(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TREASURE_Y_START + (i)]))
#define playerMaxDX(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_PLAYER_MAXDX_START + ((i) * sizeof(int16_t))]))
#define playerImpulse(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_PLAYER_IMPULSE_START + ((i) * sizeof(int16_t))]))
#define playerInput(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INPUT_START + (i)]))
#define playerUpdate(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_UPDATE_START + (i)]))
#define playerRender(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_RENDER_START + (i)]))
#define monsterMaxDX(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_MONSTER_MAXDX_START + ((i) * sizeof(int16_t))]))
#define monsterImpulse(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_MONSTER_IMPULSE_START + ((i) * sizeof(int16_t))]))
#define monsterInput(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INPUT_START + (i)]))
#define monsterUpdate(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_UPDATE_START + (i)]))
#define monsterRender(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_RENDER_START + (i)]))

#define BaseMapIsSolid(x, y, levelOffset) (PgmBitArray_readBit(&levelData[(levelOffset) + LEVEL_MAP_START], (y) * SCREEN_TILES_H + (x)))

// Returns offset into levelData PROGMEM array
static uint16_t LoadLevel(uint8_t level, uint8_t* theme)
{
  // Bounds check level
  if (level >= LEVELS)
    return 0xFFFF; // bogus value

  // Determine the offset into the PROGMEM array where the level data begins
  // struct LEVEL_HEADER {
  //   uint8_t numLevels;
  //   uint16_t levelOffsets[numLevels];
  // };
  uint16_t levelOffset = levelOffset(level);

  // Using that offset, read the tile set, and draw the map
  // struct LEVEL_DATA {
  //   uint8_t theme;
  //   uint8_t map[105];
  //   ...
  // };
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

  // Draw a solid black bar across the top of the screen
  /* for (uint8_t i = 0; i < SCREEN_TILES_H; ++i) */
  /*   SetTile(i, 0, LAST_SOLID_TILE); */

  return levelOffset;
}

#define MAX_TREASURE_COUNT 32
// How many frames to wait between animating treasure
#define BACKGROUND_FRAME_SKIP 16
// Defines the order in which the tileset "rows" are swapped in for animating tiles
const uint8_t backgroundAnimation[] PROGMEM = { 0, 1, 2, 1 };

static void killPlayer(PLAYER* p)
{
  ENTITY* e = (ENTITY*)p;
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

static void killMonster(ENTITY* e)
{
  if (e->invincible)
    return;
  TriggerFx(3, 128, true);         // play the monster death sound
  e->dead = true;                  // kill the monster
  e->interacts = false;              // make sure we don't consider the entity again for collisions
  e->input = null_input;           // disable the entity's ai
  e->update = entity_update_dying; // use dying physics
}

static void spawnMonster(ENTITY* e, uint16_t levelOffset, uint8_t i) {
  entity_init(e,
              inputFunc(monsterInput(levelOffset, i)),
              updateFunc(monsterUpdate(levelOffset, i)),
              renderFunc(monsterRender(levelOffset, i)),
              PLAYERS + i,
              (int16_t)(monsterInitialX(levelOffset, i) * (TILE_WIDTH << FP_SHIFT)),
              (int16_t)(monsterInitialY(levelOffset, i) * (TILE_HEIGHT << FP_SHIFT)),
              (int16_t)(monsterMaxDX(levelOffset, i)),
              (int16_t)(monsterImpulse(levelOffset, i)));
  uint8_t monsterFlags = monsterFlags(levelOffset, i);
  // The cast to bool is necessary to properly set bit flags
  e->left = (bool)(monsterFlags & MFLAG_LEFT);
  e->right = (bool)(monsterFlags & MFLAG_RIGHT);
  e->up = (bool)(monsterFlags & MFLAG_UP);
  e->down = (bool)(monsterFlags & MFLAG_DOWN);
  e->autorespawn = (bool)(monsterFlags & MFLAG_AUTORESPAWN);
  e->interacts = (bool)!(monsterFlags & MFLAG_NOINTERACT);
  e->invincible = (bool)(monsterFlags & MFLAG_INVINCIBLE);
}

static bool overlap(int16_t x1, int16_t y1, uint8_t w1, uint8_t h1, int16_t x2, int16_t y2, uint8_t w2, uint8_t h2)
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

static void DisplayNumber(uint8_t x, uint8_t y, uint16_t n, const uint8_t pad)
{
  for (uint8_t i = 0; i < pad; ++i, n /= 10)
    SetTile(x--, y, (n % 10) + FIRST_DIGIT_TILE);  // get next digit
}

int main()
{
  PLAYER player[PLAYERS];
  ENTITY monster[MONSTERS];

  uint8_t currentLevel;
  uint16_t levelOffset;
  uint8_t theme;

  SetTileTable(tileset);
  SetSpritesTileBank(0, mysprites);
  InitMusicPlayer(patches);

 begin:

  currentLevel = levelOffset = theme = 0;

  for (;;) {
    //WaitVsync(1); // since it takes a while to decode the level, ensure we don't miss vsync
    levelOffset = LoadLevel(currentLevel, &theme);

    /* SetUserPostVsyncCallback(&VsyncCallBack);   */

    // Check the return value of LoadLevel
    if (levelOffset == 0xFFFF)
      goto begin;

    // Initialize treasure
    uint8_t tcount = treasureCount(levelOffset);
    for (uint8_t i = 0; i < tcount; ++i) {
      uint8_t x = treasureX(levelOffset, i);
      uint8_t y = treasureY(levelOffset, i);
      if (x < SCREEN_TILES_H && y < SCREEN_TILES_V)
        SetTile(x, y, GetTile(x, y) - (FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME)));
    }

    // Initialize players
    for (uint8_t i = 0; i < PLAYERS; ++i)
      player_init(&player[i],
                  inputFunc(playerInput(levelOffset, i)),
                  updateFunc(playerUpdate(levelOffset, i)),
                  renderFunc(playerRender(levelOffset, i)), i,
                  (int16_t)(playerInitialX(levelOffset, i) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(playerInitialY(levelOffset, i) * (TILE_HEIGHT << FP_SHIFT)),
                  (int16_t)(playerMaxDX(levelOffset, i)),
                  (int16_t)(playerImpulse(levelOffset, i)));

/* #if (PLAYERS == 2) */
/*     // Player 2 starts off hidden and disabled */
/*     ((ENTITY*)(&player[1]))->render(((ENTITY*)(&player[1]))); // setup sprite */
/*     ((ENTITY*)(&player[1]))->interacts = false; */
/*     ((ENTITY*)(&player[1]))->render = null_render; */
/*     ((ENTITY*)(&player[1]))->invincible = true; */
/* #endif // (PLAYERS == 2) */

    //WaitVsync(1); // since it takes a while to decode the level, ensure we don't miss vsync

    // Initialize monsters
    for (uint8_t i = 0; i < MONSTERS; ++i)
      spawnMonster(&monster[i], levelOffset, i);


    // Since the level compiler doesn't support one-way tiles, ladders, or fire yet, these are hardcoded for now
    if (currentLevel == 0) {
      SetTile(12, 6, FIRST_ONE_WAY_TILE + theme);
      SetTile(13, 6, FIRST_ONE_WAY_TILE + theme);
      SetTile(14, 6, FIRST_ONE_WAY_TILE + theme);
      SetTile(15, 6, FIRST_ONE_WAY_TILE + theme);

      SetTile(20, 6, FIRST_ONE_WAY_TILE + theme);
      SetTile(21, 6, FIRST_ONE_WAY_TILE + theme);
      SetTile(22, 6, FIRST_ONE_WAY_TILE + theme);
      SetTile(23, 6, FIRST_ONE_WAY_TILE + theme);

      SetTile(3, 24, FIRST_ONE_WAY_TILE + theme);
      SetTile(4, 24, FIRST_ONE_WAY_TILE + theme);
      SetTile(5, 24, FIRST_ONE_WAY_TILE + theme);
      SetTile(6, 24, FIRST_ONE_WAY_TILE + theme);
    }

    SetTile(22, 26, FIRST_FIRE_TILE + theme);

    if (currentLevel == 0 || currentLevel == 1 || currentLevel == 2) {
      if (currentLevel == 2)
        SetTile(22, 9, 2 + FIRST_LADDER_TILE + ONE_WAY_LADDER_TILES_IN_THEME * theme);
      else
        SetTile(22, 9, FIRST_LADDER_TILE + ONE_WAY_LADDER_TILES_IN_THEME * theme);

      SetTile(22, 10, 1 + FIRST_LADDER_TILE + ONE_WAY_LADDER_TILES_IN_THEME * THEMES_N + LADDER_TILES_IN_THEME * theme);
      SetTile(22, 11, 1 + FIRST_LADDER_TILE + ONE_WAY_LADDER_TILES_IN_THEME * THEMES_N + LADDER_TILES_IN_THEME * theme);
      SetTile(22, 12, 1 + FIRST_LADDER_TILE + ONE_WAY_LADDER_TILES_IN_THEME * THEMES_N + LADDER_TILES_IN_THEME * theme);
      SetTile(22, 13, 1 + FIRST_LADDER_TILE + ONE_WAY_LADDER_TILES_IN_THEME * THEMES_N + LADDER_TILES_IN_THEME * theme);
      SetTile(22, 14, 1 + FIRST_LADDER_TILE + ONE_WAY_LADDER_TILES_IN_THEME * THEMES_N + LADDER_TILES_IN_THEME * theme);
    }

    // Main game loop
    for (;;) {
      /* static uint8_t localFrameCounter; */
      WaitVsync(1);

      // Animate all background tiles at once by modifying the tileset pointer
      static uint8_t backgroundFrameCounter;
      if ((backgroundFrameCounter % BACKGROUND_FRAME_SKIP) == 0)
        SetTileTable(tileset + 64 * ALLTILES_WIDTH * pgm_read_byte(&backgroundAnimation[backgroundFrameCounter / BACKGROUND_FRAME_SKIP]));
      // Compile-time assert that we are working with a power of 2
      BUILD_BUG_ON(isNotPowerOf2(BACKGROUND_FRAME_SKIP * NELEMS(backgroundAnimation)));
      backgroundFrameCounter = (backgroundFrameCounter + 1) & (BACKGROUND_FRAME_SKIP * NELEMS(backgroundAnimation) - 1);
      /* if (++backgroundFrameCounter == BACKGROUND_FRAME_SKIP * NELEMS(backgroundAnimation)) */
      /*   backgroundFrameCounter = 0; */

      // Display debugging information
      /* uint16_t sc = StackCount(); */
      /* DisplayNumber(SCREEN_TILES_H - 2, 0, sc, 4); */
      /* DisplayNumber(2, 0, globalFrameCounter, 3); */
      /* DisplayNumber(6, 0, localFrameCounter++, 3); */
      /* DisplayNumber(5, 0, (uint16_t)tracks[1].patchCommandStreamPos, 5); */
      /* DisplayNumber(9, 0, (uint16_t)tracks[1].patchCurrDeltaTime, 3); */
      /* //DisplayNumber(13, 0, (uint16_t)tracks[1].patchNextDeltaTime, 3); */
      /* DisplayNumber(15, 0, (uint16_t)tracks[2].patchCommandStreamPos, 5); */
      /* DisplayNumber(19, 0, (uint16_t)tracks[2].patchCurrDeltaTime, 3); */
      /* //DisplayNumber(27, 0, (uint16_t)tracks[2].patchNextDeltaTime, 3); */
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

      // Update the state of the monsters, and perform collision detection with each player
      for (uint8_t i = 0; i < MONSTERS; ++i) {
        int16_t monsterPrevY = monster[i].y; // cache the previous Y value to use for kill detection below
        monster[i].update(&monster[i]);

        // Collision detection (calculation assumes each sprite is WORLD_METER wide, and uses a shrunken hitbox for the monster)
        for (uint8_t p = 0; p < PLAYERS; ++p) {
          ENTITY* e = (ENTITY*)(&player[p]);
          if (monster[i].interacts && !monster[i].dead && e->interacts && !e->dead &&
              overlap(e->x,
                      e->y,
                      WORLD_METER,
                      WORLD_METER,
                      monster[i].x + (1 << FP_SHIFT),
                      monster[i].y + (3 << FP_SHIFT),
                      WORLD_METER - (2 << FP_SHIFT),
                      WORLD_METER - (4 << FP_SHIFT))) {
            // If a player and a monster overlap, and the bottom pixel of the player's previous Y is above the top
            // of the monster's previous Y then the player kills the monster, otherwise the monster kills the player.
            if (((playerPrevY[p] + WORLD_METER - (1 << FP_SHIFT)) <= (monsterPrevY + (3 << FP_SHIFT))) && !monster[i].invincible) {
              killMonster(&monster[i]);
              e->monsterhop = true; // player should now do the monster hop
            } else {
              killPlayer((PLAYER*)e);
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

      // Check if the dead flag has been set for a player
      for (uint8_t i = 0; i < PLAYERS; ++i)
        if (((ENTITY*)(&player[i]))->interacts && ((ENTITY*)(&player[i]))->dead)
          killPlayer(&player[i]);

      // Check for environmental collisions (treasure, fire) by looping over the interacting players, converting
      // their (x, y) coordinates into tile coordinates, and then reading any overlapping tiles.
      for (uint8_t p = 0; p < PLAYERS; ++p) {
        ENTITY* e = (ENTITY*)(&player[p]);
        if (e->interacts && !e->dead) {
          uint8_t tx = p2ht(e->x);
          uint8_t ty = p2vt(e->y);
          if (tx >= SCREEN_TILES_H || ty >= SCREEN_TILES_V) // bounds check to prevent writing outside of VRAM
            continue;

          bool nx = nh(e->x) && (tx + 1 < SCREEN_TILES_H);  // true if entity overlaps right
          bool ny = nv(e->y) && (ty + 1 < SCREEN_TILES_V);  // true if entity overlaps below

          bool collectedTreasure = false;
          bool killedByFire = false;

          uint8_t t = GetTile(tx, ty);
          if (isTreasure(t)) {
            SetTile(tx, ty, t + (FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME)));
            collectedTreasure = true;
          } else if (isFire(t)) {
            if (overlap(e->x,
                        e->y,
                        WORLD_METER,
                        WORLD_METER,
                        ht2p(tx) + (1 << FP_SHIFT),
                        vt2p(ty) + (5 << FP_SHIFT),
                        WORLD_METER - (2 << FP_SHIFT),
                        WORLD_METER - (3 << FP_SHIFT))) {
              killedByFire = true;
            }
          }
          t = GetTile(tx + 1, ty);
          if (nx) {
            if (isTreasure(t)) {
              SetTile(tx + 1, ty, t + (FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME)));
              collectedTreasure = true;
            } else if (isFire(t)) {
              if (overlap(e->x,
                          e->y,
                          WORLD_METER,
                          WORLD_METER,
                          ht2p(tx + 1) + (1 << FP_SHIFT),
                          vt2p(ty) + (5 << FP_SHIFT),
                          WORLD_METER - (2 << FP_SHIFT),
                          WORLD_METER - (3 << FP_SHIFT))) {
                killedByFire = true;
              }
            }
          }
          t = GetTile(tx, ty + 1);
          if (ny) {
            if (isTreasure(t)) {
              SetTile(tx, ty + 1, t + (FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME)));
              collectedTreasure = true;
            } else if (isFire(t)) {
              if (overlap(e->x,
                          e->y,
                          WORLD_METER,
                          WORLD_METER,
                          ht2p(tx) + (1 << FP_SHIFT),
                          vt2p(ty + 1) + (5 << FP_SHIFT),
                          WORLD_METER - (2 << FP_SHIFT),
                          WORLD_METER - (3 << FP_SHIFT))) {
                killedByFire = true;
              }
            }
          }
          t = GetTile(tx + 1, ty + 1);
          if (nx && ny) {
            if (isTreasure(t)) {
              SetTile(tx + 1, ty + 1, t + (FIRST_TREASURE_TILE + (THEMES_N * TREASURE_TILES_IN_THEME)));
              collectedTreasure = true;
            } else if (isFire(t)) {
              if (overlap(e->x,
                          e->y,
                          WORLD_METER,
                          WORLD_METER,
                          ht2p(tx + 1) + (1 << FP_SHIFT),
                          vt2p(ty + 1) + (5 << FP_SHIFT),
                          WORLD_METER - (2 << FP_SHIFT),
                          WORLD_METER - (3 << FP_SHIFT))) {
                killedByFire = true;
              }
            }
          }
          if (collectedTreasure)
            TriggerFx(2, 128, true);
          if (killedByFire)
            e->dead = true;
        }
      }


      // Read joypad independently of the players, because when a player dies, their joypad is no longer polled
      static uint16_t prev;
      static uint16_t held;
      prev = held;
      held = ReadJoypad(0);
      uint16_t pressed = held & (held ^ prev);

      // Check for level restart button
      if (pressed & BTN_START)
        break; // restart level

      // Check for level select buttons (hold select, and press a left or right shoulder button)
      if (held & BTN_SELECT) {
        if (pressed & BTN_SL) {
          if (--currentLevel == 255)
            currentLevel = LEVELS - 1;
          break; // load previous level
        } else if (pressed & BTN_SR) {
          if (++currentLevel == LEVELS)
            currentLevel = 0;
          break; // load next level
        }
      }

    }
  }
}
