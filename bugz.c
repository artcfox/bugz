/*

  bugz.c

  Copyright 2015 Matthew T. Pandina. All rights reserved.

  This file is part of Bugz.

  Bugz is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Bugz is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License
  along with Bugz.  If not, see <http://www.gnu.org/licenses/>.

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

#define SCORE_DIGITS 5
#define TIMER_DIGITS 3
#define COLLECT_TREASURE_POINTS 5
#define KILL_MONSTER_POINTS 25

/*
 * BCD_zero
 *
 * Sets all digits of a BCD number to zero
 *
 * num [out]
 *   The BCD number to be zeroed
 *
 * digits [in]
 *   The number of digits in the BCD number, num
 */
static void BCD_zero(uint8_t* const num, uint8_t digits)
{
  while (digits--)
    num[digits] = 0;
}

/*
 * BCD_copy
 *
 * Copies the value of the BCD number, src, into the BCD number, dst
 *
 * dst [out]
 *   The destination BCD number
 *
 * src [in]
 *   The source BCD number
 *
 * digits [in]
 *   The number of digits to copy
 *
 */
static void BCD_copy(uint8_t* const dst, const uint8_t* const src, const uint8_t digits)
{
  for (uint8_t i = 0; i < digits; ++i)
    dst[i] = src[i];
}

/*
 * BCD_compare
 *
 * Compares two (equal length) BCD numbers
 *
 * num1 [in]
 *   The first BCD number
 *
 * num2 [in]
 *   The second BCD number
 *
 * digits [in]
 *    The number of digits in the BCD numbers to compare
 *
 * Returns:
 *   An integer less than, equal to, or greater than zero if num1 is
 *   found, respectively, to be less than, to match, or be greater
 *   than num2.
 */
static int8_t BCD_compare(uint8_t* const num1, uint8_t* const num2, uint8_t digits)
{
  while (digits--) {
    if (num1[digits] < num2[digits])
      return -1;
    else if (num1[digits] > num2[digits])
      return 1;
  }
  
  return 0;
}

/*
 * BCD_addConstant
 *
 * Adds a constant (binary number) to a BCD number
 *
 * num [in, out]
 *   The BCD number
 *
 * digits [in]
 *   The number of digits in the BCD number, num
 *
 * x [in]
 *   The binary value to be added to the BCD number
 *
 *   Note: The largest value that can be safely added to a BCD number
 *         is BCD_ADD_CONSTANT_MAX. If the result would overflow num,
 *         then num will be clamped to its maximum value (all 9's).
 *
 * Returns:
 *   A boolean that is true if num has been clamped to its maximum
 *   value (all 9's), or false otherwise.
 */
#define BCD_ADD_CONSTANT_MAX 244
static bool BCD_addConstant(uint8_t* const num, const uint8_t digits, uint8_t x)
{
  for (uint8_t i = 0; i < digits; ++i) {
    uint8_t val = num[i] + x;
    if (val < 10) { // speed up the common cases
      num[i] = val;
      x = 0;
      break;
    } else if (val < 20) {
      num[i] = val - 10;
      x = 1;
    } else if (val < 30) {
      num[i] = val - 20;
      x = 2;
    } else if (val < 40) {
      num[i] = val - 30;
      x = 3;
    } else { // handle the rest of the cases (up to 255 - 9) with a loop
      for (uint8_t j = 5; j < 26; ++j) {
        if (val < (j * 10)) {
          num[i] = val - ((j - 1) * 10);
          x = (j - 1);
          break;
        }
      }
    }
  }

  if (x > 0) {
    for (uint8_t i = 0; i < digits; ++i)
      num[i] = 9;
    return true;
  }

  return false;
}
/*
 * BCD_addBCD
 *
 * Adds two BCD numbers, num1, num2, and stores the result in num1
 *
 * num1 [in, out]
 *   The first BCD number. Will contain the result of the addition.
 *
 *   Note: If the result would overflow num1, then num1 will be
 *         clamped to its maximum value (all 9's).
 *
 * num1_digits [in]
 *   The number of digits in the BCD number, num1
 *
 * num2 [in]
 *   The second BCD number
 *
 * num2_digits [in]
 *   The number of digits in the BCD number, num2
 *
 *   Note: The number of digits in num1 and num2 do not have to be the
 *         same, but num1_digits must be greater than or equal to
 *         num2_digits.
 *
 * Returns:
 *   A boolean that is true if num1 has been clamped to its maximum
 *   value (all 9's), or false otherwise.
 */
static bool BCD_addBCD(uint8_t* const num1, const uint8_t num1_digits, const uint8_t* const num2, const uint8_t num2_digits)
{
  bool retval = false;
  for (uint8_t i = 0; i < num2_digits; ++i)
    retval |= BCD_addConstant(num1 + i, num1_digits - i, num2[i]);

  if (retval)
    for (uint8_t i = 0; i < num1_digits; ++i)
      num1[i] = 9;
  return retval;
}

/*
 * BCD_decrement
 *
 * Decrements a BCD number by one
 * 
 * num [in, out]
 *   The BCD number. Will be decremented by one.
 *
 * Note: If the result would underflow num, it will be clamped to its
 *       minimum value (all 0's).
 *
 * Returns:
 *   A boolean that is true if num has been clamped to its minimum
 *   value (all 0's), or false otherwise.
 */
static bool BCD_decrement(uint8_t* const num, const uint8_t digits)
{
  // Check to make sure the entire number is greater than zero first
  for (uint8_t i = 0; i < digits; ++i)
    if (num[i] > 0)
      goto skip;
  return true;

 skip:
  for (uint8_t i = 0; i < digits; ++i) {
    uint8_t val = num[i] - 1;
    if (val == 255) {
      num[i] = 9;
    } else {
      num[i] = val;
      break;
    }
  }
  return false;
}

/*
 * BCD_display
 *
 * Displays a BCD number at tile coordinates (x, y)
 *
 * x [in]
 *   The x location of the most significant digit of the BCD number to be printed
 *
 * y [in]
 *   The y location of the BCD number to be printed
 *
 * num [in]
 *   The BCD number
 *
 * digits [in]
 *   The number of digits in the BCD number, num
 */
__attribute__(( always_inline ))
static inline void BCD_display(const uint8_t x, const uint8_t y, const uint8_t* const num, uint8_t digits)
{
  uint16_t offset = y * SCREEN_TILES_H + x;
  while (digits--)
    vram[offset++] = num[digits] + FIRST_DIGIT_TILE + RAM_TILES_COUNT;
}

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
  AI_FLY_VERTICAL_UNDULATE = 8,
  AI_FLY_HORIZONTAL_UNDULATE = 9,
  AI_FLY_VERTICAL_ERRATIC = 10,
  AI_FLY_HORIZONTAL_ERRATIC = 11,
  AI_FLY_CIRCLE_CW = 12,
  AI_FLY_CIRCLE_CCW = 13,
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
  case AI_FLY_VERTICAL_UNDULATE:
    return ai_fly_vertical_undulate;
  case AI_FLY_HORIZONTAL_UNDULATE:
    return ai_fly_horizontal_undulate;
  case AI_FLY_VERTICAL_ERRATIC:
    return ai_fly_vertical_erratic;
  case AI_FLY_HORIZONTAL_ERRATIC:
    return ai_fly_horizontal_erratic;
  case AI_FLY_CIRCLE_CW:
    return ai_fly_circle_cw;
  case AI_FLY_CIRCLE_CCW:
    return ai_fly_circle_ccw;    
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
  ENTITY_UPDATE_LADDER = 4,
};

typedef void (*updateFnPtr)(ENTITY*);
__attribute__(( optimize("Os") ))
static updateFnPtr updateFunc(const UPDATE_FUNCTION u)
{
  switch (u) {
  case PLAYER_UPDATE:
    return player_update;
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
  ALT_SPIDER_RENDER = 9,
  MOTH_RENDER = 10,
  BUTTERFLY_RENDER = 11,
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
  case ALT_SPIDER_RENDER:
    return alt_spider_render;
  case MOTH_RENDER:
    return moth_render;
  case BUTTERFLY_RENDER:
    return butterfly_render;
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

#define THEME_DIRT 0
#define THEME_SPACE 1
#define THEME_GRASS 2

const uint8_t levelData[] PROGMEM = {
  LEVELS,      // uint8_t numLevels

  // Include the auto-generated table of level offsets (uint16_t levelOffsets[numLevels])
#include "editor/levels/level_offsets.inc"

  // Title screen
#include "data/levels/0000-title_level.inc"
#include "editor/levels/0000-title_level.xcf.png.inc"

  // First level
#include "data/levels/0010-prototype_level.inc"
#include "editor/levels/0010-prototype_level.xcf.png.inc"

#include "data/levels/0020-test_level.inc"
#include "editor/levels/0020-test_level.xcf.png.inc"

#include "data/levels/0030-space_level.inc"
#include "editor/levels/0030-space_level.xcf.png.inc"

#include "data/levels/0040-underground_level.inc"
#include "editor/levels/0040-underground_level.xcf.png.inc"

#include "data/levels/0050-grasshoppers_level.inc"
#include "editor/levels/0050-grasshoppers_level.xcf.png.inc"

#include "data/levels/0060-2nd_space_level.inc"
#include "editor/levels/0060-2nd_space_level.xcf.png.inc"

#include "data/levels/0070-more_dirt_level.inc"
#include "editor/levels/0070-more_dirt_level.xcf.png.inc"

#include "data/levels/0080-symmetric_level.inc"
#include "editor/levels/0080-symmetric_level.xcf.png.inc"

#include "data/levels/0090-must_jump_higher_level.inc"
#include "editor/levels/0090-must_jump_higher_level.xcf.png.inc"

#include "data/levels/0100-watch_your_head_level.inc"
#include "editor/levels/0100-watch_your_head_level.xcf.png.inc"

#include "data/levels/0110-only_ladders_level.inc"
#include "editor/levels/0110-only_ladders_level.xcf.png.inc"

#include "data/levels/0120-one_chance_level.inc"
#include "editor/levels/0120-one_chance_level.xcf.png.inc"

#include "data/levels/0130-underground_moth_level.inc"
#include "editor/levels/0130-underground_moth_level.xcf.png.inc"

#include "data/levels/0140-butterflies_level.inc"
#include "editor/levels/0140-butterflies_level.xcf.png.inc"

#include "data/levels/0150-smaller_platforms_level.inc"
#include "editor/levels/0150-smaller_platforms_level.xcf.png.inc"

#include "data/levels/0160-moth_butterfly_spider_level.inc"
#include "editor/levels/0160-moth_butterfly_spider_level.xcf.png.inc"

#include "data/levels/0170-organic_mountain_level.inc"
#include "editor/levels/0170-organic_mountain_level.xcf.png.inc"

#include "data/levels/0180-faces_level.inc"
#include "editor/levels/0180-faces_level.xcf.png.inc"

#include "data/levels/0190-pachinko_ant_tubes_level.inc"
#include "editor/levels/0190-pachinko_ant_tubes_level.xcf.png.inc"

#include "data/levels/0200-platforms_and_ladders_level.inc"
#include "editor/levels/0200-platforms_and_ladders_level.xcf.png.inc"

#include "data/levels/0210-dodge_the_fire_level.inc"
#include "editor/levels/0210-dodge_the_fire_level.xcf.png.inc"

  // Victory screen
#include "data/levels/9999-victory_level.inc"
#include "editor/levels/9999-victory_level.xcf.png.inc"
 };

#define LEVEL_HEADER_SIZE 1
#define LEVEL_THEME_START 0
#define LEVEL_THEME_SIZE 1
#define LEVEL_TIME_BONUS_START (LEVEL_THEME_START + LEVEL_THEME_SIZE)
#define LEVEL_TIME_BONUS_SIZE 2
#define LEVEL_PLAYER_INITIAL_FLAGS_START (LEVEL_TIME_BONUS_START + LEVEL_TIME_BONUS_SIZE)
#define LEVEL_PLAYER_INITIAL_FLAGS_SIZE 2
#define LEVEL_PLAYER_INPUT_START (LEVEL_PLAYER_INITIAL_FLAGS_START + LEVEL_PLAYER_INITIAL_FLAGS_SIZE)
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
#define timeBonus(levelOffset) ((uint16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_TIME_BONUS_START]))
#define playerFlags(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INITIAL_FLAGS_START + (i)]))
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
  0 + TREASURE_TO_SKY_LADDER_TOP_OFFSET,
  1 + TREASURE_TO_SKY_LADDER_TOP_OFFSET,
  2 + TREASURE_TO_SKY_LADDER_TOP_OFFSET,
  3 + TREASURE_TO_SKY_LADDER_TOP_OFFSET,
  4 + TREASURE_TO_SKY_LADDER_TOP_OFFSET,

  // Open sky tiles get a ladder top overlaid
  5 + SKY_TO_SKY_LADDER_TOP_OFFSET,
  6 + SKY_TO_SKY_LADDER_TOP_OFFSET,
  7 + SKY_TO_SKY_LADDER_TOP_OFFSET,
  8 + SKY_TO_SKY_LADDER_TOP_OFFSET,
  9 + SKY_TO_SKY_LADDER_TOP_OFFSET,

  // Underground tiles get an underground ladder top overlaid
  10 + UNDERGROUND_TO_UNDERGROUND_LADDER_TOP_OFFSET,

  // Aboveground tiles get an aboveground ladder top overlaid
  11 + ABOVEGROUND_TO_ABOVEGROUND_LADDER_TOP_OFFSET,

  // Ladder tiles remain ladder tiles
  12, 13, 14,

  // One way tiles get a ladder top overlaid
  15 + ABOVEGROUND_ONE_WAY_TO_ABOVEGROUND_ONE_WAY_LADDER_TOP_OFFSET,
};

const uint8_t MapTileToLadderMiddle[] PROGMEM = {
  // If a ladder middle overlaps a treasure tile, the treasure gets replaced with open sky and gets a ladder middle overlaid
  0 + TREASURE_TO_SKY_LADDER_MIDDLE_OFFSET,
  1 + TREASURE_TO_SKY_LADDER_MIDDLE_OFFSET,
  2 + TREASURE_TO_SKY_LADDER_MIDDLE_OFFSET,
  3 + TREASURE_TO_SKY_LADDER_MIDDLE_OFFSET,
  4 + TREASURE_TO_SKY_LADDER_MIDDLE_OFFSET,

  // Open sky tiles get a ladder middle overlaid
  5 + SKY_TO_SKY_LADDER_MIDDLE_OFFSET,
  6 + SKY_TO_SKY_LADDER_MIDDLE_OFFSET,
  7 + SKY_TO_SKY_LADDER_MIDDLE_OFFSET,
  8 + SKY_TO_SKY_LADDER_MIDDLE_OFFSET,
  9 + SKY_TO_SKY_LADDER_MIDDLE_OFFSET,

  // Underground tiles get an underground ladder middle overlaid
  10 + UNDERGROUND_TO_UNDERGROUND_LADDER_MIDDLE_OFFSET,

  // Aboveground tiles get an aboveground ladder top (yes, TOP) overlaid
  11 + ABOVEGROUND_TO_ABOVEGROUND_LADDER_TOP_OFFSET,

  // Ladder tiles remain ladder tiles
  12, 13, 14,

  // One way tiles get a ladder top (yes, TOP) overlaid
  15 + ABOVEGROUND_ONE_WAY_TO_ABOVEGROUND_ONE_WAY_LADDER_TOP_OFFSET,
};

#define BaseMapIsSolid PgmBitArray_readBit

__attribute__(( optimize("Os") ))
static void DrawTreasure(const uint8_t x, const uint8_t y)
{
  if (x < SCREEN_TILES_H && y < SCREEN_TILES_V) {
    uint16_t offset = y * SCREEN_TILES_H + x;
    vram[offset] -= TREASURE_TO_SKY_OFFSET; // equiv. SetTile(x, y, GetTile(x, y) - ...
  }
}

__attribute__(( optimize("Os") ))
static void DrawOneWay(const uint8_t y, const uint8_t x1, const uint8_t x2, const uint8_t* const map)
{
  // In this function, using GetTile/SetTile produces smaller code than using vram directly
  if ((y < SCREEN_TILES_V) && (x1 < SCREEN_TILES_H) && (x2 < SCREEN_TILES_H)) {
    for (uint8_t x = x1; x <= x2; ++x) {
      uint8_t t = GetTile(x, y);
      if ((t >= FIRST_ABOVEGROUND_TILE) && (t <= LAST_ABOVEGROUND_TILE) && ((y == SCREEN_TILES_V) || !BaseMapIsSolid(map, (y + 1) * SCREEN_TILES_H + x)))
        SetTile(x, y, t + ABOVEGROUND_TO_ABOVEGROUND_ONE_WAY_OFFSET);
    }
  }
}

__attribute__(( optimize("Os") ))
static void DrawLadder(const uint8_t x, const uint8_t y1, const uint8_t y2)
{
  // In this function, using GetTile/SetTile produces smaller code than using vram directly
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
static void DrawFire(const uint8_t y, const uint8_t x1, const uint8_t x2)
{
  // In this function, using GetTile/SetTile produces smaller code than using vram directly
  if ((y < SCREEN_TILES_V) && (x1 < SCREEN_TILES_H) && (x2 < SCREEN_TILES_H)) {
    for (uint8_t x = x1; x <= x2; ++x) {
      uint8_t t = GetTile(x, y);
      if (t < FIRST_UNDERGROUND_TILE)
        SetTile(x, y, FIRST_FIRE_TILE);
    }
  }
}

#define MAX_PLAYERS 2
#define MAX_MONSTERS 6

__attribute__(( always_inline ))
static inline void entityInitialXY(const uint16_t levelOffset, const uint8_t i, uint8_t* const x, uint8_t* const y)
{
  const uint8_t* packedCoordinatesStart = &levelData[levelOffset + LEVEL_PACKED_COORDINATES_START];
  *x = PgmPacked5Bit_read(packedCoordinatesStart, i * 2);
  *y = PgmPacked5Bit_read(packedCoordinatesStart, i * 2 + 1);
}

// Returns offset into levelData PROGMEM array
__attribute__(( optimize("Os") ))
static uint16_t LoadLevel(const uint8_t level, uint8_t* const theme, uint8_t* const treasures, uint16_t* const timeBonus)
{
  // Bounds check level
  if (level >= LEVELS)
    return 0xFFFF; // bogus value

  // Determine the offset into the PROGMEM array where the level data begins
  const uint16_t levelOffset = levelOffset(level);

  // Using that offset, read the theme, and draw the base map
  *theme = theme(levelOffset);
  if (*theme >= THEMES_N) // something major went wrong
    return 0xFFFF; // bogus value

  SetTileTable(tileset + (TILE_WIDTH * TILE_HEIGHT) * ((TILESET_SIZE - TITLE_SCREEN_TILES) / (THEMES_N)) * *theme);

  *timeBonus = timeBonus(levelOffset) + 1;
  if (*timeBonus > 999)
    *timeBonus = 999;

  const uint8_t* const map = &levelData[levelOffset + LEVEL_MAP_START];

  for (uint8_t y = 0; y < SCREEN_TILES_V; ++y) {
    for (uint8_t x = 0; x < SCREEN_TILES_H; ++x) {
      uint16_t offset = y * SCREEN_TILES_H + x;

      if (BaseMapIsSolid(map, offset/* x, y */)) {
        if (y == 0 || BaseMapIsSolid(map, offset - SCREEN_TILES_H/* x, y - 1 */)) { // if we are the top tile, or there is a solid tile above us
          vram[offset] = FIRST_UNDERGROUND_TILE + RAM_TILES_COUNT; // underground tile
        } else {
          vram[offset] = FIRST_ABOVEGROUND_TILE + RAM_TILES_COUNT; // aboveground tile
        }
      } else { // we are a sky tile
        if (y == SCREEN_TILES_V - 1) { // holes in the bottom border are always full sky tiles
          vram[offset] = FIRST_SKY_TILE + RAM_TILES_COUNT; // full sky tile
        } else { // interior tile
          bool solidLDiag = (bool)((x == 0) || BaseMapIsSolid(map, offset + SCREEN_TILES_H - 1/* x - 1, y + 1 */));
          bool solidRDiag = (bool)((x == SCREEN_TILES_H - 1) || BaseMapIsSolid(map, offset + SCREEN_TILES_H + 1/* x + 1, y + 1 */));
          bool solidBelow = BaseMapIsSolid(map, offset + SCREEN_TILES_H/* x, y + 1 */);

          if (!solidLDiag && !solidRDiag && solidBelow) // island
            vram[offset] = 1 + FIRST_SKY_TILE + RAM_TILES_COUNT;
          else if (!solidLDiag && solidRDiag && solidBelow) // clear on the left
            vram[offset] = 2 + FIRST_SKY_TILE + RAM_TILES_COUNT;
          else if (solidLDiag && solidRDiag && solidBelow) // tiles left, below, and right
            vram[offset] = 3 + FIRST_SKY_TILE + RAM_TILES_COUNT;
          else if (solidLDiag && !solidRDiag && solidBelow) // clear on the right
            vram[offset] = 4 + FIRST_SKY_TILE + RAM_TILES_COUNT;
          else // clear all around
            vram[offset] = FIRST_SKY_TILE + RAM_TILES_COUNT;
        }
      }
    }
  }

  // Overlay treasures, oneways, ladders, and fires
  const uint8_t* packedCoordinatesStart = &levelData[levelOffset + LEVEL_PACKED_COORDINATES_START];
  uint16_t packedOffset = MAX_PLAYERS * 2 + MAX_MONSTERS * 2; // 2 coordinates per player, 2 coordinates per monster
  *treasures = treasureCount(levelOffset);
  for (uint8_t i = 0; i < *treasures; ++i) {
    const uint8_t x = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 2);
    const uint8_t y = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 2 + 1);
    DrawTreasure(x, y);
  }
  packedOffset += *treasures * 2; // 2 coordinates per treasure
  const uint8_t oneways = onewayCount(levelOffset);
  for (uint8_t i = 0; i < oneways; ++i) {
    const uint8_t y = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3);
    const uint8_t x1 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 1);
    const uint8_t x2 = PgmPacked5Bit_read(packedCoordinatesStart, packedOffset + i * 3 + 2);
    DrawOneWay(y, x1, x2, map);
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
    DrawFire(y, x1, x2);
  }

  return levelOffset;
}

// How many frames to wait between animating treasure
#define BACKGROUND_FRAME_SKIP 8
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
              PLAYERS + i + 4, // offset by 4, so the EXIT sign is between the players and monsters
              tx, ty,
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
  if (input >= AI_FLY_VERTICAL_UNDULATE) // these AI functions directly manipulate the X and Y values,
    e->input(e);                         // so call input before rendering, so the initial render happens at the proper X,Y
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
  }
  player_init(p,
              inputFunc(input),
              updateFunc(update),
              renderFunc(render), i,
              tx, ty);
  ENTITY* const e = (ENTITY*)p;
  if (e->update == entity_update)
    e->update = player_update;
  // The cast to bool is necessary to properly set bit flags
  //e->left = (bool)(playerFlags & IFLAG_LEFT);
  //e->right = (bool)(playerFlags & IFLAG_RIGHT);
  //e->up = (bool)(playerFlags & IFLAG_UP);
  //e->down = (bool)(playerFlags & IFLAG_DOWN);
  //e->autorespawn = (bool)(playerFlags & IFLAG_AUTORESPAWN);
  e->interacts = (bool)!(playerFlags & IFLAG_NOINTERACT);
  e->invincible = (bool)(playerFlags & IFLAG_INVINCIBLE);
  sprites[e->tag].flags = (playerFlags & IFLAG_SPRITE_FLIP_X) ? SPRITE_FLIP_X : 0;
  e->render(e);
}

__attribute__(( always_inline ))
static inline bool overlap(const uint8_t x1, const uint8_t y1, const uint8_t w1, const uint8_t h1, const uint8_t x2, const uint8_t y2, const uint8_t w2, const uint8_t h2)
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

#define EEPROM_ID 0x0089
#define EEPROM_SAVEGAME_VERSION 0x0001

struct EEPROM_SAVEGAME;
typedef struct EEPROM_SAVEGAME EEPROM_SAVEGAME;

struct EEPROM_SAVEGAME {
  uint16_t id;
  uint16_t version;
  uint8_t score[SCORE_DIGITS];
  char reserved[32 - 4 - SCORE_DIGITS];
} __attribute__ ((packed));

__attribute__(( optimize("Os") ))
static void LoadHighScore(uint8_t* const highScore)
{
  EEPROM_SAVEGAME save = {0};
  uint8_t retval = EepromReadBlock(EEPROM_ID, (struct EepromBlockStruct*)&save);
  if (retval == EEPROM_ERROR_BLOCK_NOT_FOUND || save.version == 0xFFFF) {
    save.id = EEPROM_ID;
    save.version = EEPROM_SAVEGAME_VERSION;
    EepromWriteBlock((struct EepromBlockStruct*)&save);
  }
  uint8_t digits = SCORE_DIGITS;
  while (digits--)
    highScore[digits] = save.score[digits];
}

__attribute__(( optimize("Os") ))
static void SaveHighScore(const uint8_t* score)
{
  EEPROM_SAVEGAME save = {0};
  save.id = EEPROM_ID;
  save.version = EEPROM_SAVEGAME_VERSION;
  uint8_t digits = SCORE_DIGITS;
  while (digits--)
    save.score[digits] = score[digits];
  EepromWriteBlock((struct EepromBlockStruct*)&save);
}

const uint8_t copyright[] PROGMEM = {
  LAST_FIRE_TILE + 9, FIRST_SKY_TILE, FIRST_DIGIT_TILE + 2,
  FIRST_DIGIT_TILE, FIRST_DIGIT_TILE + 1,
  FIRST_DIGIT_TILE + 5, FIRST_SKY_TILE,
  LAST_FIRE_TILE + 3, LAST_FIRE_TILE + 4, LAST_FIRE_TILE + 5, LAST_FIRE_TILE + 5,
  FIRST_SKY_TILE, FIRST_DIGIT_TILE + 10, LAST_FIRE_TILE + 4,
  LAST_FIRE_TILE + 6, LAST_FIRE_TILE + 7, LAST_FIRE_TILE + 8, LAST_FIRE_TILE + 6,
  LAST_FIRE_TILE + 4,
};

const uint8_t x_player[] PROGMEM = {
  FIRST_DIGIT_TILE + 10,
  LAST_FIRE_TILE + 11,
  LAST_FIRE_TILE + 4,
  LAST_FIRE_TILE + 12,
  LAST_FIRE_TILE + 13,
  LAST_FIRE_TILE + 14,
};

#if (PLAYERS == 2)
const uint8_t p1_vs_p2[] PROGMEM = {
  FIRST_DIGIT_TILE + 10, FIRST_DIGIT_TILE + 1, FIRST_SKY_TILE,
  LAST_FIRE_TILE + 1, LAST_FIRE_TILE + 2, FIRST_SKY_TILE,
  FIRST_DIGIT_TILE + 10, FIRST_DIGIT_TILE + 2,
};
#endif // (PLAYERS == 2)

__attribute__(( optimize("Os") ))
static GAME_FLAGS doTitleScreen(ENTITY* const monster, uint8_t* highScore)
{
  // Switch to the last animation row, where the title screen tiles are
  SetTileTable((tileset + (TILE_WIDTH * TILE_HEIGHT) * ((TILESET_SIZE - TITLE_SCREEN_TILES) / (THEMES_N)) * 2) + 
               (TILE_WIDTH * TILE_HEIGHT) * ((TILESET_SIZE - TITLE_SCREEN_TILES) / (3 * THEMES_N)) * 2);

  uint16_t offset = 13 * SCREEN_TILES_H + 5;
  for (uint8_t i = 0; i < NELEMS(copyright); ++i)
    vram[offset + i] = pgm_read_byte(&copyright[i]) + RAM_TILES_COUNT;

#if (PLAYERS == 2)
  offset = 21 * SCREEN_TILES_H + 11;
  for (uint8_t i = 0; i < NELEMS(p1_vs_p2); ++i)
    vram[offset + i] = pgm_read_byte(&p1_vs_p2[i]) + RAM_TILES_COUNT;
#endif // (PLAYERS == 2)

  offset = 17 * SCREEN_TILES_H + 11;
  for (uint8_t i = 0; i < PLAYERS; ++i) {
    vram[offset + (SCREEN_TILES_H * 2 * i)] = FIRST_DIGIT_TILE + 1 + i + RAM_TILES_COUNT;
    for (uint8_t j = 0; j < NELEMS(x_player); ++j)
      vram[offset + (SCREEN_TILES_H * 2 * i) + 2 + j] = pgm_read_byte(&x_player[j]) + RAM_TILES_COUNT;
  }

  // Set pointer to 1P
  uint8_t selection = 0;

  uint16_t prev = 0;
  uint16_t held = 0;

  bool fadedIn = false;
  bool wasReleased = false;

  LoadHighScore(highScore);

  sprites[0].x -= 4;

  for (;;) {
    if (selection == 0 && (highScore[0] || highScore[1] || highScore[2] || highScore[3] || highScore[4])) { // 1P
      // Using SetTile here results in smaller code
      SetTile(11, 24, LAST_FIRE_TILE + 10);
      SetTile(12, 24, LAST_FIRE_TILE + 8);
      BCD_display(14, 24, highScore, SCORE_DIGITS);
    } else {
      // erase the display of the high score
      offset = 24 * SCREEN_TILES_H + 11;
      for (uint8_t i = 0; i < 8; ++i)
        vram[offset + i] = FIRST_SKY_TILE + RAM_TILES_COUNT;
    }

    sprites[0].y = (vt2p((17 + selection * 2)) + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;

    for (uint8_t i = 0; i < MONSTERS; ++i)
      monster[i].input(&monster[i]);
    for (uint8_t i = 0; i < MONSTERS; ++i)
      monster[i].update(&monster[i]);
    for (uint8_t i = 0; i < MONSTERS; ++i)
      monster[i].render(&monster[i]);

    if (!fadedIn) {
      FadeIn(1, true);
      fadedIn = true;
    }

    prev = held;
    held = ReadJoypad(0);
    uint16_t pressed = held & (held ^ prev);

#if (PLAYERS == 2)
    // Check for mode switch buttons
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
#endif // (PLAYERS == 2)

    if ((held & BTN_START) == 0)
      wasReleased = true;

    if ((pressed & BTN_START) && wasReleased) {
      TriggerFx(2, 128, true);
#if (PLAYERS == 2)
      for (uint8_t j = 0; j < 3; ++j) {
        if (j != selection) {
          offset = (17 + (2 * j)) * SCREEN_TILES_H + 11;
          for (uint8_t i = 0; i < 8; ++i)
            vram[offset + i] = FIRST_SKY_TILE + RAM_TILES_COUNT;
        }
      }
#endif // (PLAYERS == 2)

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
  uint8_t gameScore[SCORE_DIGITS * PLAYERS] = {0};
  uint8_t levelScore[SCORE_DIGITS * PLAYERS];
  uint8_t highScore[SCORE_DIGITS] = {0};
  uint8_t currentLevel;
  uint16_t levelOffset;
  uint8_t theme;
  uint8_t backgroundFrameCounter;
  uint8_t timer[TIMER_DIGITS];
  uint8_t gameType;
  uint8_t treasuresLeft;
  uint8_t levelEndTimer;

  /* SetUserRamTilesCount(1); */
  SetSpritesTileBank(0, mysprites);
  InitMusicPlayer(patches);

 title_screen:
  levelOffset = theme = levelEndTimer = treasuresLeft = 0;
  currentLevel = 0;
  gameType = GFLAG_1P;

  for (;;) {
    if (levelEndTimer == 0) { // this occurs when loading a level, unless we just legitimately beat a level
      for (uint8_t i = 0; i < MAX_SPRITES; ++i)
        sprites[i].x = OFF_SCREEN; // hide all sprites, avoiding artifacts that SetSpriteVisibility causes when showing them at different positions later
      FadeOut(0, true); // fade to black immediately
    }
    SetTileTable(tileset);

    uint16_t timeBonus = 0;
/* __asm__ __volatile__ ("wdr"); */
/*     // Wait until all sound effects have stopped playing to avoid sound glitches */
/*     while ((tracks[0].flags | tracks[1].flags) & TRACK_FLAGS_PLAYING) */
/*       WaitVsync(1); */
/*     SetRenderingParameters(262 - 80, 80); */

    levelOffset = LoadLevel(currentLevel, &theme, &treasuresLeft, &timeBonus);

    // Convert timeBonus into unpacked BCD and store in timer[TIMER_DIGITS] array (not time critical)
    BCD_zero(timer, TIMER_DIGITS);
    while (timeBonus > BCD_ADD_CONSTANT_MAX) {
      BCD_addConstant(timer, TIMER_DIGITS, BCD_ADD_CONSTANT_MAX);
      timeBonus -= BCD_ADD_CONSTANT_MAX;
    }
    if (timeBonus > 0)
      BCD_addConstant(timer, TIMER_DIGITS, timeBonus);

/*     SetRenderingParameters(FIRST_RENDER_LINE, FRAME_LINES); */
/* __asm__ __volatile__ ("wdr"); */

    /* SetUserPostVsyncCallback(&VsyncCallBack);   */

    // Check the return value of LoadLevel
    if (levelOffset == 0xFFFF)
      goto title_screen;

    backgroundFrameCounter = 0;

    // Initialize players
    for (uint8_t i = 0; i < PLAYERS; ++i)
      spawnPlayer(&player[i], levelOffset, i, gameType);

    // Initialize monsters
    for (uint8_t i = 0; i < MONSTERS; ++i)
      spawnMonster(&monster[i], levelOffset, i);

    levelEndTimer = 0;
    BCD_copy(levelScore, gameScore, SCORE_DIGITS * PLAYERS);

    if (currentLevel == 0) {
      gameType = doTitleScreen(monster, highScore);
      currentLevel = 1;
      BCD_zero(gameScore, SCORE_DIGITS * PLAYERS);
      continue;
    } else {
      if (currentLevel == LEVELS - 1) {
        BCD_zero(timer, TIMER_DIGITS); // don't display the timer or level number on the victory screen
      } else {
        uint8_t levelDisplay[2] = {0};
        BCD_addConstant(levelDisplay, 2, currentLevel);
        BCD_display(2, 0, levelDisplay, 2); // display the level number
      }

      FadeIn(1, true);
    }

    // Display the player numbers
    uint16_t offset = 0 * SCREEN_TILES_H + 11;
    vram[offset] = FIRST_DIGIT_TILE + 10 + RAM_TILES_COUNT;
    vram[offset + 1] = FIRST_DIGIT_TILE + 1 + RAM_TILES_COUNT;

    if (!(gameType & GFLAG_1P)) {
      offset = 0 * SCREEN_TILES_H + 20;
      vram[offset] = FIRST_DIGIT_TILE + 10 + RAM_TILES_COUNT;
      vram[offset + 1] = FIRST_DIGIT_TILE + 2 + RAM_TILES_COUNT;
    }

    // Main game loop
    for (;;) {
      /* static uint8_t localFrameCounter; */
      WaitVsync(1);
/* __asm__ __volatile__ ("wdr"); */

      /* uint8_t* ramTile = GetUserRamTile(0); */
      /* CopyFlashTile(0, 0); */
      /* ramTile[27] = 0x07; */
      /* vram[0] = 0; */

      // Animate all background tiles at once by modifying the tileset pointer
      if ((backgroundFrameCounter % BACKGROUND_FRAME_SKIP) == 0) {
        SetTileTable((tileset + (TILE_WIDTH * TILE_HEIGHT) * ((TILESET_SIZE - TITLE_SCREEN_TILES) / (THEMES_N)) * theme) + 
                     (TILE_WIDTH * TILE_HEIGHT) * ((TILESET_SIZE - TITLE_SCREEN_TILES) / (3 * THEMES_N)) *
                     pgm_read_byte(&backgroundAnimation[backgroundFrameCounter / BACKGROUND_FRAME_SKIP]));

        // Decrement, and display the time bonus timer if its value is greater than zero
        if (timer[0] || timer[1] || timer[2]) {
          BCD_decrement(timer, TIMER_DIGITS);
          BCD_display(6, 0, timer, TIMER_DIGITS);
        }
      }
      // Compile-time assert that we are working with a power of 2
      BUILD_BUG_ON(isNotPowerOf2(BACKGROUND_FRAME_SKIP * NELEMS(backgroundAnimation)));
      backgroundFrameCounter = (backgroundFrameCounter + 1) & (BACKGROUND_FRAME_SKIP * NELEMS(backgroundAnimation) - 1);

      // Display the score(s)
      BCD_display(14, 0, &levelScore[0], SCORE_DIGITS);
#if (PLAYERS == 2)
      if (!(gameType & GFLAG_1P))
        BCD_display(23, 0, &levelScore[SCORE_DIGITS], SCORE_DIGITS);
#endif // (PLAYERS == 2)

/* __asm__ __volatile__ ("wdr"); */

      // Display debugging information
      /* uint16_t sc = StackCount(); */
      /* DisplayNumber(SCREEN_TILES_H - 3, 1, sc, 4); */
      /* DisplayNumber(2, 0, globalFrameCounter, 3); */
      /* DisplayNumber(6, 0, localFrameCounter++, 3); */

      // Proper kill detection requires the previous Y value for each entity
      uint8_t playerPrevY[PLAYERS];

      // Get inputs/update the state of the players
      for (uint8_t i = 0; i < PLAYERS; ++i) {
        ENTITY* e = (ENTITY*)(&player[i]);
        playerPrevY[i] = sprites[i].y; // cache the previous Y value to use for kill detection below
        e->input(e);
  /* __asm__ __volatile__ ("wdr"); */
        e->update(e);
  /* __asm__ __volatile__ ("wdr"); */
        e->render(e);
      }

#if (PLAYERS == 2)
      // Collision check between players (only in versus mode)
      if (gameType & GFLAG_P1_VS_P2) {
        ENTITY* p1 = (ENTITY*)(&player[0]);
        ENTITY* p2 = (ENTITY*)(&player[1]);
        if (p1->interacts && !p1->dead && p2->interacts && !p2->dead &&
            overlap(sprites[0].x, sprites[0].y, TILE_WIDTH, TILE_HEIGHT, sprites[1].x, sprites[1].y, TILE_WIDTH, TILE_HEIGHT)) {
          if (((playerPrevY[0] + TILE_HEIGHT - 1) < (playerPrevY[1])) && !p2->invincible) {
            killPlayer(p2);
            p2->monsterhop = false; // die like a bug
            if (p1->update == player_update)
              p1->monsterhop = true; // player should now do the monster hop, but only if gravity applies
          } else if (((playerPrevY[1] + TILE_HEIGHT - 1) < (playerPrevY[0])) && !p1->invincible) {
            killPlayer(p1);
            p1->monsterhop = false; // die like a bug
            if (p2->update == player_update)
              p2->monsterhop = true; // player should now do the monster hop, but only if gravity applies
          }
        }
      }
#endif // (PLAYERS == 2)

      // Get inputs/update the state of the monsters, and perform collision detection with each player
      for (uint8_t i = 0; i < MONSTERS; ++i) {
        uint8_t monsterPrevY = sprites[PLAYERS + i + 4].y; // cache the previous Y value to use for kill detection below
        monster[i].input(&monster[i]);
        monster[i].update(&monster[i]);
        monster[i].render(&monster[i]);

        // Collision detection (calculation assumes each sprite is WORLD_METER wide, and uses a shrunken hitbox for the monster)
        for (uint8_t p = 0; p < PLAYERS; ++p) {
          ENTITY* const e = (ENTITY*)(&player[p]);
          if (monster[i].interacts && !monster[i].dead && e->interacts && !e->dead &&
              overlap(sprites[p].x, sprites[p].y, TILE_WIDTH, TILE_HEIGHT,
                      sprites[PLAYERS + i + 4].x + 1,
                      sprites[PLAYERS + i + 4].y + 3,
                      TILE_WIDTH - 2, TILE_HEIGHT - 4)) {
            // If a player and a monster overlap, and the bottom pixel of the player's previous Y is above the top
            // of the monster's previous Y then the player kills the monster, otherwise the monster kills the player.
            if (((playerPrevY[p] + TILE_HEIGHT - 1) <= (monsterPrevY + 3)) && !monster[i].invincible) {
              killMonster(&monster[i]);
  /* __asm__ __volatile__ ("wdr"); */
              BCD_addConstant(&levelScore[SCORE_DIGITS * p], SCORE_DIGITS, KILL_MONSTER_POINTS);
  /* __asm__ __volatile__ ("wdr"); */
              if (e->update == player_update)
                e->monsterhop = true; // player should now do the monster hop, but only if gravity applies
            } else {
              killPlayer(e);
            }
          }
        }
      }

      // Check if the dead flag has been set for a monster and/or if we need to respawn a monster
      for (uint8_t i = 0; i < MONSTERS; ++i) {
        if (monster[i].interacts && monster[i].dead)
          killMonster(&monster[i]);
        if (monster[i].dead && monster[i].autorespawn && monster[i].render == null_render) // monster is dead, and its dying animation has finished
          spawnMonster(&monster[i], levelOffset, i);
      }

      // Check if the dead flag has been set for a player
      for (uint8_t i = 0; i < PLAYERS; ++i) {
        ENTITY* const e = (ENTITY*)&player[i];
        if (e->interacts && e->dead)
          killPlayer(e);
      }

      // Check for environmental collisions (treasure, fire) by looping over the interacting players, converting
      // their (x, y) coordinates into tile coordinates, and checking any overlapping tiles.
      for (uint8_t i = 0; i < PLAYERS; ++i) {
        ENTITY* const e = (ENTITY*)(&player[i]);
        if (e->interacts && !e->dead) {
          uint8_t tx = sprites[i].x / TILE_WIDTH;
          uint8_t ty = sprites[i].y / TILE_HEIGHT;
          /* if (tx >= SCREEN_TILES_H || ty >= SCREEN_TILES_V) // bounds check to prevent writing outside of VRAM */
          /*   continue; */

          bool nx = (sprites[i].x % TILE_WIDTH) /* && (tx + 1 < SCREEN_TILES_H) */;  // true if entity overlaps right
          bool ny = (sprites[i].y % TILE_HEIGHT) /* && (ty + 1 < SCREEN_TILES_V) */;  // true if entity overlaps below

          uint8_t treasureCollected = 0;
          bool killedByFire = false;

          uint16_t offset = ty * SCREEN_TILES_H + tx;
          uint8_t t = vram[offset] - RAM_TILES_COUNT; // equiv. GetTile(tx, ty)
          if (isTreasure(t)) {
            vram[offset] += TREASURE_TO_SKY_OFFSET;   // equiv. SetTile(tx, ty, ...
            treasureCollected++;
          } else if (isFire(t)) {
            if (overlap(sprites[i].x, sprites[i].y, TILE_WIDTH, TILE_HEIGHT,
                        (tx    ) * TILE_WIDTH + 1,
                        (ty    ) * TILE_HEIGHT + 5,
                        TILE_WIDTH - 2, 2))
              killedByFire = true;
          }
          t = vram[offset + 1] - RAM_TILES_COUNT;         // equiv. GetTile(tx + 1, ty)
          if (nx) {
            if (isTreasure(t)) {
              vram[offset + 1] += TREASURE_TO_SKY_OFFSET; // equiv. SetTile(tx + 1, ty, ...
              treasureCollected++;
            } else if (isFire(t)) {
              if (overlap(sprites[i].x, sprites[i].y, TILE_WIDTH, TILE_HEIGHT,
                          (tx + 1) * TILE_WIDTH + 1,
                          (ty    ) * TILE_HEIGHT + 5,
                          TILE_WIDTH - 2, 2))
                killedByFire = true;
            }
          }
          t = vram[offset + SCREEN_TILES_H] - RAM_TILES_COUNT;         // equiv. GetTile(tx, ty + 1)
          if (ny) {
            if (isTreasure(t)) {
              vram[offset + SCREEN_TILES_H] += TREASURE_TO_SKY_OFFSET; // equiv. SetTile(tx, ty + 1, ...
              treasureCollected++;
            } else if (isFire(t)) {
              if (overlap(sprites[i].x, sprites[i].y, TILE_WIDTH, TILE_HEIGHT,
                          (tx    ) * TILE_WIDTH + 1,
                          (ty + 1) * TILE_HEIGHT + 5,
                          TILE_WIDTH - 2, 2))
                killedByFire = true;
            }
          }
          t = vram[offset + SCREEN_TILES_H + 1] - RAM_TILES_COUNT;         // equiv. GetTile(tx + 1, ty + 1)
          if (nx && ny) {
            if (isTreasure(t)) {
              vram[offset + SCREEN_TILES_H + 1] += TREASURE_TO_SKY_OFFSET; // equiv. SetTile(tx + 1, ty + 1, ...
              treasureCollected++;
            } else if (isFire(t)) {
              if (overlap(sprites[i].x, sprites[i].y, TILE_WIDTH, TILE_HEIGHT,
                          (tx + 1) * TILE_WIDTH + 1,
                          (ty + 1) * TILE_HEIGHT + 5,
                          TILE_WIDTH - 2, 2))
                killedByFire = true;
            }
          }
          if (treasureCollected) {
            TriggerFx(2, 128, true);
            treasuresLeft -= treasureCollected;
            BCD_addConstant(&levelScore[SCORE_DIGITS * i], SCORE_DIGITS, treasureCollected * COLLECT_TREASURE_POINTS);

            // Check to see if the last treasure has just been collected
            if (treasuresLeft == 0) {
              entityInitialXY(levelOffset, 0, &tx, &ty);
              show_exit_sign(tx, ty - 1);
            }
          }
          if (killedByFire && !e->invincible)
            e->dead = true;
        }
      }

      if (treasuresLeft == 0) {
        if (levelEndTimer == 0) {
          bool overlapsPortal = false;
          for (uint8_t i = 0; i < PLAYERS; ++i) {
            ENTITY* e = (ENTITY*)&player[i];
            if (!e->dead && overlap(sprites[i].x, sprites[i].y, TILE_WIDTH, TILE_HEIGHT,
                                    sprites[PLAYERS].x, sprites[PLAYERS].y, 2 * TILE_WIDTH, 2 * TILE_HEIGHT))
              overlapsPortal = true;
          }
          if (overlapsPortal) {
            // Ensure players can't die while the physics engine keeps running
            for (uint8_t i = 0; i < PLAYERS; ++i) {
              ENTITY* e = (ENTITY*)&player[i];
              e->interacts = false;
              e->invincible = true;
            }


            BCD_copy(gameScore, levelScore, SCORE_DIGITS * PLAYERS);
            BCD_addBCD(&gameScore[0], SCORE_DIGITS, timer, TIMER_DIGITS); // add time bonus
#if (PLAYERS == 2)
            BCD_addBCD(&gameScore[SCORE_DIGITS], SCORE_DIGITS, timer, TIMER_DIGITS); // add time bonus
#endif // (PLAYERS == 2)
              

            BCD_zero(timer, TIMER_DIGITS);
            ++levelEndTimer; // initiate level end sequence
          }
        } else if (levelEndTimer++ == WORLD_FALLING_GRACE_FRAMES + 1) { // ensure portal is shown (albiet briefly) if a player overlaps it before it is displayed
          TriggerFx(2, 128, true); // maybe add a victory 'level complete' sound effect?
          hide_exit_sign();
          FadeOut(1, false); // asynchronous fade to black
        } else if (levelEndTimer == 60) {
          if ((gameType & GFLAG_1P) && (BCD_compare(gameScore, highScore, SCORE_DIGITS) > 0))
            SaveHighScore(gameScore);
          for (uint8_t i = 0; i < MAX_SPRITES; ++i)
            sprites[i].x = OFF_SCREEN;
          if (++currentLevel == LEVELS)
            currentLevel = 0;
          break; // since levelEndTimer is not zero (this is a legit level complete, not a skip), the instant fade out at the top of the for loop will be skipped
        }
      }

      // Check for level select buttons (hold select, and press a left or right shoulder button)
      uint16_t held = player[0].buttons.held;
      uint16_t pressed = player[0].buttons.pressed;

      if (held & BTN_SELECT) {
        if (pressed & BTN_SL) {
          if (gameType & GFLAG_1P)
            BCD_zero(gameScore, SCORE_DIGITS * PLAYERS);
          if (--currentLevel == 0)
            currentLevel = LEVELS - 2;
          break; // load previous level
        } else if (pressed & BTN_SR) {
          if (gameType & GFLAG_1P)
            BCD_zero(gameScore, SCORE_DIGITS * PLAYERS);
          if (++currentLevel == LEVELS - 1)
            currentLevel = 1;
/* __asm__ __volatile__ ("wdr"); */
          break; // load next level
        } else if (pressed & BTN_START) {
          currentLevel = 0;
          break; // return to title screen
        }
      }

      if (gameType & GFLAG_1P) {
        // Check for level restart button
        if (pressed & BTN_START) {
          if (currentLevel == LEVELS - 1) // victory level
            goto title_screen;
          else
            break; // restart level
        }
      } else {
        // Stop showing the victory level when any player presses START
        if ((currentLevel == LEVELS - 1) && ((pressed & BTN_START) || (player[PLAYERS - 1].buttons.pressed & BTN_START)))
          goto title_screen;
        
        // Check for both players holding level restart button at the same time
        if (((pressed & BTN_START) && (player[PLAYERS - 1].buttons.held & BTN_START)) ||
            ((held & BTN_START) && (player[PLAYERS - 1].buttons.pressed & BTN_START))) {
          break;
        }

        // Check for respawn button (any button other than START)
        for (uint8_t i = 0; i < PLAYERS; ++i) {
          ENTITY* e = (ENTITY*)&player[i];
          if (e->dead && (e->render == null_render) && (player[i].buttons.held && (player[i].buttons.held & ~BTN_START))) {
            // Respawning in multiplayer mode resets your score for that level
            BCD_copy(&levelScore[SCORE_DIGITS * i], &gameScore[SCORE_DIGITS * i], SCORE_DIGITS); 
            spawnPlayer((PLAYER*)e, levelOffset, i, gameType);
          }
        }
      }

/* __asm__ __volatile__ ("wdr"); */

    }
  }
}
