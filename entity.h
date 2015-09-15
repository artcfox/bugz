/*

  entity.h

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

#ifndef __ENTITY_H__
#define __ENTITY_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <uzebox.h>

#define NELEMS(x) (sizeof(x)/sizeof(x[0]))

// Gives a compile-time error if condition is not satisfied, produces no code otherwise
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#define isNotPowerOf2(x) (((x) & ((x) - 1)) != 0)

#define LO8(x) ((uint8_t)((x) & 0xFF))
#define HI8(x) ((uint8_t)(((x) >> 8) & 0xFF))

#define vt2p(t) ((t) * (TILE_HEIGHT << FP_SHIFT))
#define ht2p(t) ((t) * (TILE_WIDTH << FP_SHIFT))
#define p2vt(p) ((p) / (TILE_HEIGHT << FP_SHIFT))
#define p2ht(p) ((p) / (TILE_WIDTH << FP_SHIFT))
#define nv(p) ((p) % (TILE_HEIGHT << FP_SHIFT))
#define nh(p) ((p) % (TILE_WIDTH << FP_SHIFT))

#define nearestScreenPixel(p) (((p) + (1 << (FP_SHIFT - 1))) >> FP_SHIFT)

#define PLAYERS 2
#define MONSTERS 6

// Include the auto-generated definition for LEVELS
#include "editor/levels/num_levels.inc"

// Fixed point shift
#define FP_SHIFT 2

// 1/32th of a second per frame (not really, but it makes the math faster, and the constants below have been adjusted to compensate)
#define WORLD_FPS 32
// Arbitrary choice for 1m
#define WORLD_METER (8 << FP_SHIFT)
// Very exagerated gravity
#define WORLD_GRAVITY 615
//(WORLD_METER * 18)
// Max horizontal speed
#define WORLD_MAXDX (WORLD_METER * 4)
// Max vertical speed
#define WORLD_MAXDY (WORLD_METER * 16)
// Horizontal acceleration - take 1/2 second to reach maxdx
#define WORLD_ACCEL 655
//(WORLD_MAXDX * 6)
// Horizontal friction - take 1/6 second to stop from maxdx
#define WORLD_FRICTION 437
//(WORLD_MAXDX * 4)
// (A large) instantaneous jump impulse
#define WORLD_JUMP 13500
//(WORLD_METER * 382)
// How many frames you can be falling and still jump
#define WORLD_FALLING_GRACE_FRAMES 6
// Parameter used for variable jumping (gravity / 10 is a good default)
#define WORLD_CUT_JUMP_SPEED_LIMIT (WORLD_GRAVITY / 10)

#define THEMES_N 3

#define TREASURE_TILES_IN_THEME 5
#define FIRST_TREASURE_TILE 0
#define LAST_TREASURE_TILE ((FIRST_TREASURE_TILE + TREASURE_TILES_IN_THEME) - 1)

#define SKY_TILES_IN_THEME 5
#define FIRST_SKY_TILE (LAST_TREASURE_TILE + 1)
#define LAST_SKY_TILE ((FIRST_SKY_TILE + SKY_TILES_IN_THEME) - 1)

#define UNDERGROUND_TILES_IN_THEME 1
#define FIRST_UNDERGROUND_TILE (LAST_SKY_TILE + 1)
#define LAST_UNDERGROUND_TILE ((FIRST_UNDERGROUND_TILE + UNDERGROUND_TILES_IN_THEME) - 1)

#define ABOVEGROUND_TILES_IN_THEME 1
#define FIRST_ABOVEGROUND_TILE (LAST_UNDERGROUND_TILE + 1)
#define LAST_ABOVEGROUND_TILE ((FIRST_ABOVEGROUND_TILE + ABOVEGROUND_TILES_IN_THEME) - 1)

#define UNDERGROUND_LADDER_TOP_TILES_IN_THEME 1
#define FIRST_UNDERGROUND_LADDER_TOP_TILE (LAST_ABOVEGROUND_TILE + 1)
#define LAST_UNDERGROUND_LADDER_TOP_TILE ((FIRST_UNDERGROUND_LADDER_TOP_TILE + UNDERGROUND_LADDER_TOP_TILES_IN_THEME) - 1)

#define ABOVEGROUND_LADDER_TOP_TILES_IN_THEME 1
#define FIRST_ABOVEGROUND_LADDER_TOP_TILE (LAST_UNDERGROUND_LADDER_TOP_TILE + 1)
#define LAST_ABOVEGROUND_LADDER_TOP_TILE ((FIRST_ABOVEGROUND_LADDER_TOP_TILE + ABOVEGROUND_LADDER_TOP_TILES_IN_THEME) - 1)

#define UNDERGROUND_LADDER_MIDDLE_TILES_IN_THEME 1
#define FIRST_UNDERGROUND_LADDER_MIDDLE_TILE (LAST_ABOVEGROUND_LADDER_TOP_TILE + 1)
#define LAST_UNDERGROUND_LADDER_MIDDLE_TILE ((FIRST_UNDERGROUND_LADDER_MIDDLE_TILE + UNDERGROUND_LADDER_MIDDLE_TILES_IN_THEME) - 1)

#define ABOVEGROUND_ONE_WAY_TILES_IN_THEME 1
#define FIRST_ABOVEGROUND_ONE_WAY_TILE (LAST_UNDERGROUND_LADDER_MIDDLE_TILE + 1)
#define LAST_ABOVEGROUND_ONE_WAY_TILE ((FIRST_ABOVEGROUND_ONE_WAY_TILE + ABOVEGROUND_ONE_WAY_TILES_IN_THEME) - 1)

#define ABOVEGROUND_ONE_WAY_LADDER_TOP_TILES_IN_THEME 1
#define FIRST_ABOVEGROUND_ONE_WAY_LADDER_TOP_TILE (LAST_ABOVEGROUND_ONE_WAY_TILE + 1)
#define LAST_ABOVEGROUND_ONE_WAY_LADDER_TOP_TILE ((FIRST_ABOVEGROUND_ONE_WAY_LADDER_TOP_TILE + ABOVEGROUND_ONE_WAY_LADDER_TOP_TILES_IN_THEME) - 1)

#define SKY_LADDER_TOP_TILES_IN_THEME 5
#define FIRST_SKY_LADDER_TOP_TILE (LAST_ABOVEGROUND_ONE_WAY_LADDER_TOP_TILE + 1)
#define LAST_SKY_LADDER_TOP_TILE ((FIRST_SKY_LADDER_TOP_TILE + SKY_LADDER_TOP_TILES_IN_THEME) - 1)

#define SKY_LADDER_MIDDLE_TILES_IN_THEME 5
#define FIRST_SKY_LADDER_MIDDLE_TILE (LAST_SKY_LADDER_TOP_TILE + 1)
#define LAST_SKY_LADDER_MIDDLE_TILE ((FIRST_SKY_LADDER_MIDDLE_TILE + SKY_LADDER_MIDDLE_TILES_IN_THEME) - 1)

#define DIGIT_TILES_IN_THEME 11
#define FIRST_DIGIT_TILE (LAST_SKY_LADDER_MIDDLE_TILE + 1)
#define LAST_DIGIT_TILE ((FIRST_DIGIT_TILE + DIGIT_TILES_IN_THEME) - 1)

#define FIRE_TILES_IN_THEME 1
#define FIRST_FIRE_TILE (LAST_DIGIT_TILE + 1)
#define LAST_FIRE_TILE ((FIRST_FIRE_TILE + FIRE_TILES_IN_THEME) - 1)

#define TITLE_SCREEN_TILES (10 + 29)

#define TREASURE_TO_SKY_OFFSET (FIRST_SKY_TILE - FIRST_TREASURE_TILE)

#define TREASURE_TO_SKY_LADDER_TOP_OFFSET (FIRST_SKY_LADDER_TOP_TILE - FIRST_TREASURE_TILE)
#define SKY_TO_SKY_LADDER_TOP_OFFSET (FIRST_SKY_LADDER_TOP_TILE - FIRST_SKY_TILE)
#define UNDERGROUND_TO_UNDERGROUND_LADDER_TOP_OFFSET (FIRST_UNDERGROUND_LADDER_TOP_TILE - FIRST_UNDERGROUND_TILE)
#define ABOVEGROUND_TO_ABOVEGROUND_LADDER_TOP_OFFSET (FIRST_ABOVEGROUND_LADDER_TOP_TILE - FIRST_ABOVEGROUND_TILE)
#define ABOVEGROUND_ONE_WAY_TO_ABOVEGROUND_ONE_WAY_LADDER_TOP_OFFSET (FIRST_ABOVEGROUND_ONE_WAY_LADDER_TOP_TILE - FIRST_ABOVEGROUND_ONE_WAY_TILE)

#define TREASURE_TO_SKY_LADDER_MIDDLE_OFFSET (FIRST_SKY_LADDER_MIDDLE_TILE - FIRST_TREASURE_TILE)
#define SKY_TO_SKY_LADDER_MIDDLE_OFFSET (FIRST_SKY_LADDER_MIDDLE_TILE - FIRST_SKY_TILE)
#define UNDERGROUND_TO_UNDERGROUND_LADDER_MIDDLE_OFFSET (FIRST_UNDERGROUND_LADDER_MIDDLE_TILE - FIRST_UNDERGROUND_TILE)

#define ABOVEGROUND_TO_ABOVEGROUND_ONE_WAY_OFFSET (FIRST_ABOVEGROUND_ONE_WAY_TILE - FIRST_ABOVEGROUND_TILE)

#if (FIRST_TREASURE_TILE == 0)
#define isTreasure(t) ((t) <= LAST_TREASURE_TILE)
#else // FIRST_TREASURE_TILE
#define isTreasure(t) (((t) >= FIRST_TREASURE_TILE) && ((t) <= LAST_TREASURE_TILE))
#endif // FIRST_TREASURE_TILE

#define isSolid(t) (((t) >= FIRST_UNDERGROUND_TILE) && ((t) <= LAST_UNDERGROUND_LADDER_MIDDLE_TILE))
#define isOneWay(t) (((t) >= FIRST_ABOVEGROUND_ONE_WAY_TILE) && ((t) <= LAST_SKY_LADDER_TOP_TILE))
#define isLadder(t) ((((t) >= FIRST_UNDERGROUND_LADDER_TOP_TILE) && ((t) <= LAST_UNDERGROUND_LADDER_MIDDLE_TILE)) || (((t) >= FIRST_ABOVEGROUND_ONE_WAY_LADDER_TOP_TILE) && ((t) <= LAST_SKY_LADDER_MIDDLE_TILE)))

// As long as the last (non-title screen) tile is a fire tile, we can skip the '&& ((t) <= LAST_FIRE_TILE)' part of the condition
#define isFire(t) ((t) >= FIRST_FIRE_TILE)
//#define isFire(t) (((t) >= FIRST_FIRE_TILE) && ((t) <= LAST_FIRE_TILE))

struct ENTITY;
typedef struct ENTITY ENTITY;

struct ENTITY {
  void (*input)(ENTITY*);
  void (*update)(ENTITY*);
  void (*render)(ENTITY*);
  uint8_t initialX;
  uint8_t initialY;
  uint8_t tag;
  int16_t x;
  int16_t y;
  int16_t dx;
  int16_t dy;
  int16_t maxdx;
  int16_t impulse; // used for jumping, and since flying entities don't jump, the high/low bytes are used to store flying limits
  uint8_t animationFrameCounter;
  uint8_t framesFalling; // used for allowing late jumps immediately after falling
  unsigned int interacts:1;
  unsigned int falling:1;
  unsigned int jumping:1;
  unsigned int left:1;
  unsigned int right:1;
  unsigned int up:1;
  unsigned int down:1;
  unsigned int jump:1;
  unsigned int jumpReleased:1; // state variable used for allowing jump to be pressed early, and for implementing variable jumping
  unsigned int turbo:1;
  unsigned int monsterhop:1;
  unsigned int visible:1;
  unsigned int dead:1;
  unsigned int autorespawn:1;
  unsigned int invincible:1;
} __attribute__ ((packed));

struct BUTTON_INFO;
typedef struct BUTTON_INFO BUTTON_INFO;

struct BUTTON_INFO {
  uint16_t held;
  uint16_t prev;
  uint16_t pressed;
  //uint16_t released;
} __attribute__ ((packed));

struct PLAYER;
typedef struct PLAYER PLAYER;

struct PLAYER { ENTITY entity;
  BUTTON_INFO buttons;
} __attribute__ ((packed));

// Default functions that do nothing
void null_input(ENTITY* const e);
#define null_update null_input
void null_render(ENTITY* const e);

void entity_init(ENTITY* const e, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), const uint8_t tag, const uint8_t x, const uint8_t y, const int16_t maxdx, const int16_t impulse);
void player_init(PLAYER* const p, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), const uint8_t tag, const uint8_t x, const uint8_t y, const int16_t maxdx, const int16_t impulse);

void player_input(ENTITY* const e);
void ai_walk_until_blocked(ENTITY* const e);
void ai_hop_until_blocked(ENTITY* const e);
void ai_walk_until_blocked_or_ledge(ENTITY* const e);
void ai_hop_until_blocked_or_ledge(ENTITY* const e); // impulse and maxdx should be small enough so ledge detection doesn't trigger while jumping, and it doesn't jump off the ledge
void ai_fly_vertical(ENTITY* const e);
void ai_fly_horizontal(ENTITY* const e);
void ai_fly_vertical_undulate(ENTITY* const e);
void ai_fly_horizontal_undulate(ENTITY* const e);
void ai_fly_vertical_erratic(ENTITY* const e);
void ai_fly_horizontal_erratic(ENTITY* const e);
void ai_fly_circle_cw(ENTITY* const e);
void ai_fly_circle_ccw(ENTITY* const e);

void entity_update(ENTITY* const e);
void entity_update_dying(ENTITY* const e);
void entity_update_flying(ENTITY* const e);
void entity_update_ladder(ENTITY* const e);

void player_render(ENTITY* const e);
void ladybug_render(ENTITY* const e);
void ant_render(ENTITY* const e);
void cricket_render(ENTITY* const e);
void grasshopper_render(ENTITY* const e);
void fruitfly_render(ENTITY* const e);
void bee_render(ENTITY* const e);
void moth_render(ENTITY* const e);
void butterfly_render(ENTITY* const e);
void spider_render(ENTITY* const e);

void show_exit_sign(const uint8_t tx, const uint8_t ty);
void hide_exit_sign(void);

#endif // __ENTITY_H__
