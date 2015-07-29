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

#define LO8(x) ((uint8_t)((x) & 0xFF))
#define HI8(x) ((uint8_t)(((x) >> 8) & 0xFF))

#define vt2p(t) ((t) * (TILE_HEIGHT << FP_SHIFT))
#define ht2p(t) ((t) * (TILE_WIDTH << FP_SHIFT))
#define p2vt(p) ((p) / (TILE_HEIGHT << FP_SHIFT))
#define p2ht(p) ((p) / (TILE_WIDTH << FP_SHIFT))
#define nv(p) ((p) % (TILE_HEIGHT << FP_SHIFT))
#define nh(p) ((p) % (TILE_WIDTH << FP_SHIFT))

#define FAST_INTEGRATION 1

#define PLAYERS 1
#define MONSTERS 6

#define FP_SHIFT   2

#if (FAST_INTEGRATION)

// 1/30th of a second per frame
#define WORLD_FPS 32
// arbitrary choice for 1m
#define WORLD_METER (8 << FP_SHIFT)
// very exagerated gravity (6x)
#define WORLD_GRAVITY 615
//(WORLD_METER * 18)
// max horizontal speed (20 tiles per second)
#define WORLD_MAXDX 128
//(WORLD_METER * 3)
// max vertical speed (60 tiles per second). If the jump impulse is increased, this should be increased as well.
#define WORLD_MAXDY 512
//(WORLD_METER * 15)
// horizontal acceleration - take 1/2 second to reach maxdx
#define WORLD_ACCEL 655
//(WORLD_MAXDX * 6)
// horizontal friction - take 1/6 second to stop from maxdx
#define WORLD_FRICTION 437
//(WORLD_MAXDX * 4)
// (a large) instantaneous jump impulse
#define WORLD_JUMP_IMPULSE 13500
//(WORLD_METER * 382)
// how many frames you can be falling and still jump
#define WORLD_FALLING_GRACE_FRAMES 6
// parameter used for variable jumping (gravity / 10 is a good default)
#define WORLD_CUT_JUMP_SPEED_LIMIT (WORLD_GRAVITY / 10)

#else // FAST_INTEGRATION

// 1/30th of a second per frame
#define WORLD_FPS 30
// arbitrary choice for 1m
#define WORLD_METER (8 << FP_SHIFT)
// very exagerated gravity (6x)
#define WORLD_GRAVITY (WORLD_METER * 18)
// max horizontal speed (20 tiles per second)
#define WORLD_MAXDX (WORLD_METER * 3)
// max vertical speed (60 tiles per second). If the jump impulse is increased, this should be increased as well.
#define WORLD_MAXDY (WORLD_METER * 15)
// horizontal acceleration - take 1/2 second to reach maxdx
#define WORLD_ACCEL (WORLD_MAXDX * 6)
// horizontal friction - take 1/6 second to stop from maxdx
#define WORLD_FRICTION (WORLD_MAXDX * 4)
// (a large) instantaneous jump impulse
#define WORLD_JUMP_IMPULSE (WORLD_METER * 382)
// how many frames you can be falling and still jump
#define WORLD_FALLING_GRACE_FRAMES 6
// parameter used for variable jumping (gravity / 10 is a good default)
#define WORLD_CUT_JUMP_SPEED_LIMIT (WORLD_GRAVITY / 10)

#endif // FAST_INTEGRATION

#define TILESETS_N 3
#define TREASURE_TILES_IN_TILESET 5
#define FIRST_TREASURE_TILE 0
#define LAST_TREASURE_TILE ((FIRST_TREASURE_TILE + TILESETS_N * TREASURE_TILES_IN_TILESET) - 1)
#define SKY_TILES_IN_TILESET 5
#define SOLID_TILES_IN_TILESET 2
#define FIRST_SOLID_TILE (FIRST_TREASURE_TILE + TREASURE_TILES_IN_TILESET * TILESETS_N + SKY_TILES_IN_TILESET * TILESETS_N)
#define LAST_SOLID_TILE ((FIRST_SOLID_TILE + TILESETS_N * SOLID_TILES_IN_TILESET) - 1)
#define FIRST_ONE_WAY_TILE (FIRST_SOLID_TILE + TILESETS_N * SOLID_TILES_IN_TILESET)
#define ONE_WAY_TILES_IN_TILESET 1
#define ONE_WAY_LADDER_TILES_IN_TILESET 3
#define LADDER_TILES_IN_TILESET 2
#define LAST_ONE_WAY_TILE ((FIRST_ONE_WAY_TILE + TILESETS_N * ONE_WAY_TILES_IN_TILESET + TILESETS_N * ONE_WAY_LADDER_TILES_IN_TILESET) - 1)
#define FIRST_LADDER_TILE (FIRST_ONE_WAY_TILE + TILESETS_N * ONE_WAY_TILES_IN_TILESET)
#define LAST_LADDER_TILE ((FIRST_LADDER_TILE + TILESETS_N * ONE_WAY_LADDER_TILES_IN_TILESET + TILESETS_N * LADDER_TILES_IN_TILESET) - 1)
#define FIRE_TILES_IN_TILESET 1
#define FIRST_FIRE_TILE (LAST_LADDER_TILE + 1)
#define LAST_FIRE_TILE ((FIRST_FIRE_TILE + TILESETS_N * FIRE_TILES_IN_TILESET) - 1)

// Ladder tiles must come immediately after one way tiles, because they partially overlap (the topmost ladder tiles are also one way tiles).

#define isTreasure(t) (((t) >= FIRST_TREASURE_TILE) && ((t) <= LAST_TREASURE_TILE))
#define isSolid(t) (((t) >= FIRST_SOLID_TILE) && ((t) <= LAST_SOLID_TILE))
#define isOneWay(t) (((t) >= FIRST_ONE_WAY_TILE) && ((t) <= LAST_ONE_WAY_TILE))
#define isLadder(t) (((t) >= FIRST_LADDER_TILE) && ((t) <= LAST_LADDER_TILE))
#define isFire(t) (((t) >= FIRST_FIRE_TILE) && ((t) <= LAST_FIRE_TILE))
/*
  One way of implementing everthing is to extend the idea that I'm
  already using, which is to have an array of PLAYER structs for each
  player, and I could have a fixed size array of MONSTER structs whose
  size is the maximum number of monsters that I can have on the screen
  at any time, and i could have a BOSS struct for when there is a boss
  fight. Each one would have an interacts flag, so they could be turned
  on or off as needed.
 */

struct ENTITY;
typedef struct ENTITY ENTITY;

struct ENTITY {
  void (*input)(ENTITY*);
  void (*update)(ENTITY*);
  void (*render)(ENTITY*);
  uint8_t tag;
  int16_t x;
  int16_t y;
  int16_t dx;
  int16_t dy;
  int16_t maxdx;
  int16_t impulse; // used for jumping, and since flying entities don't jump, the high/low bytes are used to store flying limits
  uint8_t animationFrameCounter;
  unsigned int interacts : 1;
  unsigned int falling : 1;
  unsigned int jumping : 1;
  unsigned int left : 1;
  unsigned int right : 1;
  unsigned int up : 1;
  unsigned int down : 1;
  unsigned int jump : 1;
  unsigned int jumpReleased : 1; // state variable used for allowing jump to be pressed early, and for implementing variable jumping
  unsigned int turbo : 1;
  unsigned int monsterhop : 1;
  unsigned int visible : 1;
  unsigned int dead : 1;
  unsigned int autorespawn : 1;
  unsigned int invincible : 1;
} __attribute__ ((packed));

// Default functions that do nothing
void null_input(ENTITY* e);
void null_update(ENTITY* e);
void null_render(ENTITY* e);

void entity_init(ENTITY* e, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx, int16_t impulse);
void ai_walk_until_blocked(ENTITY* e);
void ai_hop_until_blocked(ENTITY* e);
void ai_walk_until_blocked_or_ledge(ENTITY* e);
void ai_hop_until_blocked_or_ledge(ENTITY* e); // hop should be small enough that the ledge detection doesn't trigger while hopping, and maxdx doesn't hop you off the ledge
void ai_fly_vertical(ENTITY* e);
void ai_fly_horizontal(ENTITY* e);
void entity_update(ENTITY* e);
void entity_update_dying(ENTITY* e);
void entity_update_flying(ENTITY* e);
void ladybug_render(ENTITY* e);
void ant_render(ENTITY* e);
void cricket_render(ENTITY* e);
void grasshopper_render(ENTITY* e);
void fruitfly_render(ENTITY* e);
void bee_render(ENTITY* e);
void spider_render(ENTITY* e);


struct BUTTON_INFO;
typedef struct BUTTON_INFO BUTTON_INFO;

struct BUTTON_INFO {
  uint16_t held;
  //uint16_t pressed;
  //uint16_t released;
  uint16_t prev;
};

struct PLAYER;
typedef struct PLAYER PLAYER;

struct PLAYER { ENTITY entity;
  BUTTON_INFO buttons;
  uint8_t framesFalling; // used for allowing late jumps immediately after falling
} __attribute__ ((packed));

void player_init(PLAYER* p, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx, int16_t impulse);
void player_input(ENTITY* e);
void player_update(ENTITY* e);
void player_update_ladder(ENTITY* e);
void player_render(ENTITY* e);

#endif // __ENTITY_H__
