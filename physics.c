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

  This file incorporates work covered by the following copyright and
  permission notice:

    Copyright (c) 2013 Jake Gordon and contributors

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.
  
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
  
*/

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <uzebox.h>
#include <string.h>

#include "data/level.inc"
#include "data/sprites.inc"
#include "data/patches.inc"

#include "entity.h"

#define NELEMS(x) (sizeof(x)/sizeof(x[0]))

#define PLAYERS 1
#define MONSTERS 5

struct BUTTON_INFO;
typedef struct BUTTON_INFO BUTTON_INFO;

struct BUTTON_INFO {
  int held;
  int pressed;
  int released;
  int prev;
};

/* struct PLAYER_INFO { */
/*   u16 x; */
/*   u16 y; */
/*   int16_t dx; */
/*   int16_t dy; */
/*   int16_t ddx; */
/*   int16_t ddy; */
/*   bool falling; */
/*   bool jumping; */

/*   bool left; // if the input was BTN_LEFT, then set this to true */
/*   bool right; // if the input was BTN_RIGHT, then set this to true */
/*   bool jump; // if the input was BTN_A, then set this to true */

/*   // Enhancement to improve jumping experience */
/*   bool jumpAllowed; // if BTN_A was released, then set this to true */

/*   uint16_t framesFalling; */

/*   // Debugging purposes */
/*   bool clamped; */

/*   /\* u8 w; *\/ */
/*   /\* u8 h; *\/ */
/* }; */

BUTTON_INFO buttons[PLAYERS];

// Maps tile number to solidity
const uint8_t isSolid[] PROGMEM = {0, 0, 1, 0, 1, 1};

#define FP_SHIFT   2
#define PLAYER_START_WIDTH  8
#define PLAYER_START_HEIGHT 8
#define PLAYER_0_START_X    (4 * (TILE_WIDTH << FP_SHIFT))
#define PLAYER_0_START_Y    ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT) - (PLAYER_START_HEIGHT << FP_SHIFT))
#define PLAYER_1_START_X    ((SCREEN_TILES_H - 4) * (TILE_WIDTH << FP_SHIFT) - (PLAYER_START_WIDTH << FP_SHIFT))
#define PLAYER_1_START_Y    ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT) - (PLAYER_START_HEIGHT << FP_SHIFT))

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
#define WORLD_JUMP (WORLD_METER * 382)
// how many frames you can be falling and still jump
#define WORLD_FALLING_GRACE_FRAMES 6
// parameter used for variable jumping (gravity / 10 is a good default)
#define WORLD_CUT_JUMP_SPEED_LIMIT (WORLD_GRAVITY / 10)

/* uint16_t monster_start_x[] = { */
/*   ((SCREEN_TILES_H - 1) * (TILE_WIDTH << FP_SHIFT) - (PLAYER_START_WIDTH << FP_SHIFT)), */
/*   ((SCREEN_TILES_H - 7) * (TILE_WIDTH << FP_SHIFT) - (PLAYER_START_WIDTH << FP_SHIFT)), */
/*   (2 * (TILE_WIDTH << FP_SHIFT)), */
/*   (17 * (TILE_WIDTH << FP_SHIFT)), */
/*   (6 * (TILE_WIDTH << FP_SHIFT)), */
/* }; */

/* uint16_t monster_start_y[] = { */
/*   ((SCREEN_TILES_V - 5) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)), */
/*   ((SCREEN_TILES_V - 19) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)), */
/*   ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)), */
/*   ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)), */
/*   ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)), */
/* }; */

#define vt2p(t) ((t) * (TILE_HEIGHT << FP_SHIFT))
#define ht2p(t) ((t) * (TILE_WIDTH << FP_SHIFT))
#define p2vt(p) ((p) / (TILE_HEIGHT << FP_SHIFT))
#define p2ht(p) ((p) / (TILE_WIDTH << FP_SHIFT))
#define nv(p) ((p) % (TILE_HEIGHT << FP_SHIFT))
#define nh(p) ((p) % (TILE_WIDTH << FP_SHIFT))


void entity_update(ENTITY* e)
{
  bool wasLeft = e->dx < 0;
  bool wasRight = e->dx > 0;
  bool falling = e->falling;

  e->ddx = 0;
  e->ddy = WORLD_GRAVITY;

  if (e->left)
    e->ddx -= WORLD_ACCEL;    // entity wants to go left
  else if (wasLeft)
    e->ddx += WORLD_FRICTION; // entity was going left, but not anymore

  if (e->right)
    e->ddx += WORLD_ACCEL;    // entity wants to go right
  else if (wasRight)
    e->ddx -= WORLD_FRICTION; // entity was going right, but not anymore

  if (e->jump && !e->jumping && !falling) {
    e->dy = 0;            // reset vertical velocity so jumps during grace period are consistent with jumps from ground
    e->ddy -= WORLD_JUMP; // apply an instantaneous (large) vertical impulse
    e->jumping = true;
  }

  // Clamp horizontal velocity to zero if we detect that the entities direction has changed
  if ((wasLeft && (e->dx > 0)) || (wasRight && (e->dx < 0))) {
    e->dx = 0; // clamp at zero to prevent friction from making the entity jiggle side to side
  }

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (e->ddx / WORLD_FPS);
  if (e->dx < -e->maxdx)
    e->dx = -e->maxdx;
  else if (e->dx > e->maxdx)
    e->dx = e->maxdx;

  // Collision Detection for X
  u8 tx = p2ht(e->x);
  u8 ty = p2vt(e->y);
  u8 nx = nh(e->x);  // true if entity overlaps right
  u8 ny = nv(e->y);  // true if entity overlaps below
  u8 cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  u8 cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  u8 celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  u8 celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

  if (e->dx > 0) {
    if ((cellright && !cell) ||
        (celldiag  && !celldown && ny)) {
      e->x = ht2p(tx);     // clamp the x position to avoid moving into the platform just hit
      e->dx = 0;           // stop horizontal velocity
      nx = 0;              // entity no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
      celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);
    }
  } else if (e->dx < 0) {
    if ((cell     && !cellright) ||
        (celldown && !celldiag && ny)) {
      e->x = ht2p(tx + 1); // clamp the x position to avoid moving into the platform just hit
      e->dx = 0;           // stop horizontal velocity
      nx = 0;              // entity no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
      celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);
    }
  }

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->y += (e->dy / WORLD_FPS);
  e->dy += (e->ddy / WORLD_FPS);
  if (e->dy < -WORLD_MAXDY)
    e->dy = -WORLD_MAXDY;
  else if (e->dy > WORLD_MAXDY)
    e->dy = WORLD_MAXDY;

  // Collision Detection for Y
  tx = p2ht(e->x);
  ty = p2vt(e->y);
  nx = nh(e->x);  // true if entity overlaps right
  ny = nv(e->y);  // true if entity overlaps below
  cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

  if (e->dy > 0) {
    if ((celldown && !cell) ||
        (celldiag && !cellright && nx)) {
      e->y = vt2p(ty);     // clamp the y position to avoid falling into platform below
      e->dy = 0;           // stop downward velocity
      e->falling = false;  // no longer falling
      e->jumping = false;  // (or jumping)
      ny = 0;              // no longer overlaps the cells below
    }
  } else if (e->dy < 0) {
    if ((cell      && !celldown) ||
        (cellright && !celldiag && nx)) {
      e->y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      e->dy = 0;           // stop updard velocity
      ny = 0;              // no longer overlaps the cells below
    }
  }

  e->falling = !(celldown || (nx && celldiag)) && !e->jumping; // detect if we're now falling or not
}

/* Calculate forces that apply to the player
   Apply the forces to move and accelerate the player
   Collision detection (and resolution) */
void player_update(ENTITY* e)
{
  PLAYER* p = (PLAYER*)e; // upcast

  bool wasLeft = e->dx < 0;
  bool wasRight = e->dx > 0;
  bool falling = e->falling ? (p->framesFalling > WORLD_FALLING_GRACE_FRAMES) : false;

  e->ddx = 0;
  e->ddy = WORLD_GRAVITY;

  if (e->left)
    e->ddx -= WORLD_ACCEL; // player wants to go left
  else if (wasLeft)
    e->ddx += WORLD_FRICTION; // player was going left, but not anymore

  if (e->right)
    e->ddx += WORLD_ACCEL; // player wants to go right
  else if (wasRight)
    e->ddx -= WORLD_FRICTION; // player was going right, but not anymore

  if (e->jump && !e->jumping && !falling) {
    e->dy = 0;            // reset vertical velocity so jumps during grace period are consistent with jumps from ground
    e->ddy -= WORLD_JUMP; // apply an instantaneous (large) vertical impulse
    e->jumping = true;
  }

  // Variable height jumping
  if (e->jumping && (buttons[e->tag].released & BTN_A) && (e->dy < -WORLD_CUT_JUMP_SPEED_LIMIT))
      e->dy = -WORLD_CUT_JUMP_SPEED_LIMIT;

  // Clamp horizontal velocity to zero if we detect that the players direction has changed
  if ((wasLeft && (e->dx > 0)) || (wasRight && (e->dx < 0))) {
    e->dx = 0; // clamp at zero to prevent friction from making us jiggle side to side
  }

  /* p->clamped = false; */

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (e->ddx / WORLD_FPS);
  if (e->dx < -e->maxdx)
    e->dx = -e->maxdx;
  else if (e->dx > e->maxdx)
    e->dx = e->maxdx;

  // Collision Detection for X
  u8 tx = p2ht(e->x);
  u8 ty = p2vt(e->y);
  u8 nx = nh(e->x);  // true if player overlaps right
  u8 ny = nv(e->y); // true if player overlaps below
  u8 cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  u8 cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  u8 celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  u8 celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

  if (e->dx > 0) {
    if ((cellright && !cell) ||
        (celldiag  && !celldown && ny)) {
      /* p->clamped = true; */
      e->x = ht2p(tx);     // clamp the x position to avoid moving into the platform we just hit
      e->dx = 0;           // stop horizontal velocity
      nx = 0;                     // player no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
      celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);
    }
  } else if (e->dx < 0) {
    if ((cell     && !cellright) ||
        (celldown && !celldiag && ny)) {
      /* p->clamped = true; */
      e->x = ht2p(tx + 1); // clamp the x position to avoid moving into the platform we just hit
      e->dx = 0;           // stop horizontal velocity
      nx = 0;                     // player no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
      celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);
    }
  }

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->y += (e->dy / WORLD_FPS);
  e->dy += (e->ddy / WORLD_FPS);
  if (e->dy < -WORLD_MAXDY)
    e->dy = -WORLD_MAXDY;
  else if (e->dy > WORLD_MAXDY)
    e->dy = WORLD_MAXDY;

  // Collision Detection for Y
  tx = p2ht(e->x);
  ty = p2vt(e->y);
  nx = nh(e->x);  // true if player overlaps right
  ny = nv(e->y); // true if player overlaps below
  cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

  if (e->dy > 0) {
    if ((celldown && !cell) ||
        (celldiag && !cellright && nx)) {
      /* p->clamped = true; */
      e->y = vt2p(ty);     // clamp the y position to avoid falling into platform below
      e->dy = 0;           // stop downward velocity
      e->falling = false;  // no longer falling
      p->framesFalling = 0;
      e->jumping = false;  // (or jumping)
      ny = 0;                     // no longer overlaps the cells below
    }
  } else if (e->dy < 0) {
    if ((cell      && !celldown) ||
        (cellright && !celldiag && nx)) {
      /* p->clamped = true; */
      e->y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      e->dy = 0;           // stop updard velocity
      ny = 0;                     // player no longer overlaps the cells below
    }
  }

  e->falling = !(celldown || (nx && celldiag)) && !e->jumping; // detect if we're now falling or not
  if (e->falling)
    p->framesFalling++;
}

void player_input(ENTITY* e)
{
  PLAYER* p = (PLAYER*)e; // upcast

  uint8_t i = e->tag;

  // Read the current state of the player's controller
  buttons[i].prev = buttons[i].held;
  buttons[i].held = ReadJoypad(i);
  buttons[i].pressed = buttons[i].held & (buttons[i].held ^ buttons[i].prev);
  buttons[i].released = buttons[i].prev & (buttons[i].held ^ buttons[i].prev);

  e->left = (bool)(buttons[i].held & BTN_LEFT);
  e->right = (bool)(buttons[i].held & BTN_RIGHT);

  // Improve the user experience, by allowing players to jump by holding the jump
  // button before landing, but require them to release it before jumping again
  if (p->jumpAllowed) {                                      // Jumping multiple times requires releasing the jump button between jumps
    e->jump = (bool)(buttons[i].held & BTN_A);                      // player[i].jump can only be true if BTN_A has been released from the previous jump
    if (e->jump && !(e->jumping || (e->falling && p->framesFalling > WORLD_FALLING_GRACE_FRAMES))) { // if player[i] is currently holding BTN_A, (and is on the ground)
      p->jumpAllowed = false;                                // a jump will occur during the next call to update(), so clear the jumpAllowed flag.
      TriggerFx(0, 128, false);
    }
  } else {                                                           // Otherwise, it means that we just jumped, and BTN_A is still being held down
    e->jump = false;                                          // so explicitly disallow any additional jumps until
    if (buttons[i].released & BTN_A)                                 // BTN_A is released again
      p->jumpAllowed = true;                                 // at which point reset the jumpAllowed flag so another jump may occur.
  }
}

void player_render(ENTITY* e)
{
  /* PLAYER* p = (PLAYER*)e; // upcast */

  /* uint8_t i = e->tag; */

  if (e->left == e->right) {
    /* if (p->clamped) */
    /*   MapSprite2(i, orange_front, 0); */
    /* else */
      MapSprite2(e->tag, yellow_front, 0);
  } else {
    /* if (p->clamped) */
    /*   MapSprite2(i, orange_side, e->right ? SPRITE_FLIP_X : 0); */
    /* else */
      MapSprite2(e->tag, yellow_side, e->right ? SPRITE_FLIP_X : 0);
  }
  MoveSprite(e->tag, (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, 1, 1);
}

void monster_input(ENTITY* e)
{
  // Collision Detection for X
  u8 tx;
  if (e->left)
    tx = p2ht(e->x - (1 << FP_SHIFT) + 1);
  else
    tx = p2ht(e->x);
  u8 ty = p2vt(e->y);
  u8 cellleft  = pgm_read_byte(&isSolid[GetTile(tx , ty)]);
  u8 cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);

  if (e->left) {
    if (cellleft) {
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    if (cellright) {
      e->right = false;
      e->left = true;
    }
  }
}

void monster_render(ENTITY* e)
{
  MapSprite2(e->tag, monster_side, e->right ? SPRITE_FLIP_X : 0);
  MoveSprite(e->tag, (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, 1, 1);
}

int main()
{
  PLAYER player[PLAYERS];
  ENTITY monster[MONSTERS];

  /* memset(player, 0, sizeof(PLAYER) * PLAYERS); */
  
  /* ENTITY* entities[] = { (void*)0, (void*)0, (void*)0 }; */
  /* ENTITY* entities[1] = {(ENTITY*)&player[0]}; // for some reason this has to point to something at compile time */
  /* ENTITY* entities[] = {(ENTITY*)&player[0], (ENTITY*)&monster[0]}; // for some reason this has to point to something at compile time */
  /* ENTITY* entities[] = {(ENTITY*)&player[0], (ENTITY*)&monster[0], (ENTITY*)&monster[1]}; // for some reason this has to point to something at compile time */

  // Sets tile table to the specified tilesheet
  SetTileTable(level);

  SetSpritesTileBank(0, mysprites);

  InitMusicPlayer(patches);

  // Initialize players
  for (uint8_t i = 0; i < PLAYERS; ++i) {
    if (i == 0)
      player_init(&player[i], player_input, player_update, player_render, 0, PLAYER_0_START_X, PLAYER_0_START_Y, WORLD_MAXDX);
    else if (i == 1)
      player_init(&player[i], player_input, player_update, player_render, 1, PLAYER_1_START_X, PLAYER_1_START_Y, WORLD_MAXDX);

    MapSprite2(i, yellow_front, 0);
  }

  // Initialize monsters
  for (uint8_t i = 0; i < MONSTERS; ++i) {
    if (i == 0) {
      entity_init(&monster[i], monster_input, entity_update, monster_render, PLAYERS + i,
                  PLAYER_1_START_X,
                  ((SCREEN_TILES_V - 15) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
                  WORLD_METER * 2);
      monster[i].right = true;
    }
    else if (i == 1) {
      entity_init(&monster[i], monster_input, entity_update, monster_render, PLAYERS + i,
                  (28 * (TILE_WIDTH << FP_SHIFT)),
                  ((SCREEN_TILES_V - 5) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
                  WORLD_METER * 2);
      monster[i].left = true;
    }
    else if (i == 2) {
      entity_init(&monster[i], monster_input, entity_update, monster_render, PLAYERS + i,
                  (9 * (TILE_WIDTH << FP_SHIFT)),
                  ((SCREEN_TILES_V - 8) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
                  WORLD_METER * 2);
      monster[i].left = true;
    }
    else if (i == 3) {
      entity_init(&monster[i], monster_input, entity_update, monster_render, PLAYERS + i,
                  (16 * (TILE_WIDTH << FP_SHIFT)),
                  ((SCREEN_TILES_V - 10) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
                  WORLD_METER * 2);
      monster[i].left = true;
    }
    else if (i == 4) {
      entity_init(&monster[i], monster_input, entity_update, monster_render, PLAYERS + i,
                  (19 * (TILE_WIDTH << FP_SHIFT)),
                  ((SCREEN_TILES_V - 19) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
                  WORLD_METER * 2);
      monster[i].left = true;

    }
    /* monster[i].left = true; */
    /* monster[i].right = false; */
    MapSprite2(monster[i].tag, monster_side, monster[i].left ? 0 : SPRITE_FLIP_X);
    MoveSprite(monster[i].tag, (monster[i].x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, (monster[i].y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, 1, 1);
  }

  /* entities[0] = (ENTITY*)&player[0]; */
  /* entities[1] = (ENTITY*)&player[1]; */
  /* entities[2] = &monster[0]; */

  // Initialize entities array
  /* if (PLAYERS > 0) */
  /*   entities[0] = (ENTITY*)&player[0]; */
  /* if (PLAYERS > 1) */
  /*   entities[1] = (ENTITY*)&player[1]; */

  /* for (uint8_t i = PLAYERS; i < PLAYERS + MONSTERS; ++i) { */
  /*   entities[i] = &monster[i - PLAYERS]; */
  /* } */

  // Fills the video RAM with the first tile (0, 0)
  ClearVram();

  /* // Draw a solid border around the edges of the screen */
  /* for (u8 i = 0; i < SCREEN_TILES_H; i++) { */
  /*   for (u8 j = 0; j < SCREEN_TILES_V; j++) { */
  /*     if (i == 0 || i == SCREEN_TILES_H - 1) { */
  /*       SetTile(i, j, 2); */
  /*       continue; */
  /*     } */
  /*     if (j == 0 || j == SCREEN_TILES_V - 1) { */
  /*       SetTile(i, j, 2); */
  /*       continue; */
  /*     } */
  /*     SetTile(i, j, 3); */
  /*   } */
  /* } */
  DrawMap(0, 0, level1);

  //  struct BUTTON_INFO buttons[PLAYERS];
  memset(buttons, 0, sizeof(buttons));

  for (;;) {
    WaitVsync(1);

    /* // Read the current state of each controller */
    /* for (uint8_t i = 0; i < PLAYERS + MONSTERS; ++i) { */
    /*   //if (entities[i]) */
    /*     entities[i]->input(entities[i]); */
    /* } */

    /* // Update the state of the players */
    /* for (uint8_t i = 0; i < PLAYERS + MONSTERS; ++i) { */
    /*   //if (entities[i]) */
    /*     entities[i]->update(entities[i]); */
    /* } */
    
    /* // Render the world */
    /* for (uint8_t i = 0; i < PLAYERS + MONSTERS; ++i) { */
    /*   //if (entities[i]) */
    /*     entities[i]->render(entities[i]); */
    /* } */



    // Read the current state of each controller
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->input((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      ((ENTITY*)(&monster[i]))->input((ENTITY*)(&monster[i]));
    }
 
    
    // Update the state of the players
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->update((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      ((ENTITY*)(&monster[i]))->update((ENTITY*)(&monster[i]));
    }
    
    // Render the world
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->render((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      ((ENTITY*)(&monster[i]))->render((ENTITY*)(&monster[i]));
    }
  }


}
