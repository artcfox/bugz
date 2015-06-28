/*

  entity.c

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

#include "entity.h"
#include "data/sprites.inc"
#include "data/patches.inc"

#define vt2p(t) ((t) * (TILE_HEIGHT << FP_SHIFT))
#define ht2p(t) ((t) * (TILE_WIDTH << FP_SHIFT))
#define p2vt(p) ((p) / (TILE_HEIGHT << FP_SHIFT))
#define p2ht(p) ((p) / (TILE_WIDTH << FP_SHIFT))
#define nv(p) ((p) % (TILE_HEIGHT << FP_SHIFT))
#define nh(p) ((p) % (TILE_WIDTH << FP_SHIFT))

// Maps tile number to solidity
const uint8_t isSolid[] PROGMEM = {0, 0, 1, 0, 1, 1};

void null_input(ENTITY* e) { }
void null_update(ENTITY* e) { }
void null_render(ENTITY* e) { }

void entity_init(ENTITY* e, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx)
{
  e->input = input;
  e->update = update;
  e->render = render;
  e->tag = tag;
  e->x = x;
  e->y = y;
  e->maxdx = maxdx;
  e->dx = e->dy = e->ddx = e->ddy = e->falling = e->jumping = e->left = e->right = e->jump = 0;
}

void monster_input(ENTITY* e)
{
  // Collision Detection for X
  uint8_t tx;
  if (e->left)
    tx = p2ht(e->x - (1 << FP_SHIFT) + 1);
  else
    tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);
  uint8_t cellleft  = pgm_read_byte(&isSolid[GetTile(tx , ty)]);
  uint8_t cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);

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
  uint8_t tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);
  uint8_t nx = nh(e->x);  // true if entity overlaps right
  uint8_t ny = nv(e->y);  // true if entity overlaps below
  uint8_t cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  uint8_t cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  uint8_t celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  uint8_t celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

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

void monster_render(ENTITY* e)
{
  MapSprite2(e->tag, ant_side, e->right ? SPRITE_FLIP_X : 0);
  MoveSprite(e->tag, (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, 1, 1);
}

// ---------- PLAYER

void player_init(PLAYER* p, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx)
{
  entity_init((ENTITY*)p, input, update, render, tag, x, y, maxdx);
  memset(&p->buttons, 0, sizeof(p->buttons));
  p->jumpReleased = true;
  p->framesFalling = 0;
}

void player_input(ENTITY* e)
{
  PLAYER* p = (PLAYER*)e; // upcast

  // Read the current state of the player's controller
  p->buttons.prev = p->buttons.held;
  p->buttons.held = ReadJoypad(e->tag); // tag will be set to 0 or 1, depending on which player we are
  p->buttons.pressed = p->buttons.held & (p->buttons.held ^ p->buttons.prev);
  p->buttons.released = p->buttons.prev & (p->buttons.held ^ p->buttons.prev);

  e->left = (bool)(p->buttons.held & BTN_LEFT);
  e->right = (bool)(p->buttons.held & BTN_RIGHT);

  // Improve the user experience, by allowing players to jump by holding the jump
  // button before landing, but require them to release it before jumping again
  if (p->jumpReleased) {                                      // Jumping multiple times requires releasing the jump button between jumps
    e->jump = (bool)(p->buttons.held & BTN_A);                      // player[i].jump can only be true if BTN_A has been released from the previous jump
    if (e->jump && !(e->jumping || (e->falling && p->framesFalling > WORLD_FALLING_GRACE_FRAMES))) { // if player[i] is currently holding BTN_A, (and is on the ground)
      p->jumpReleased = false;                                // a jump will occur during the next call to update(), so clear the jumpReleased flag.
      TriggerFx(0, 128, false);
    }
  } else {                                                           // Otherwise, it means that we just jumped, and BTN_A is still being held down
    e->jump = false;                                          // so explicitly disallow any additional jumps until
    if (p->buttons.released & BTN_A)                                 // BTN_A is released again
      p->jumpReleased = true;                                 // at which point reset the jumpReleased flag so another jump may occur.
  }
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
  if (e->jumping && p->jumpReleased && (e->dy < -WORLD_CUT_JUMP_SPEED_LIMIT))
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
  uint8_t tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);
  uint8_t nx = nh(e->x);  // true if player overlaps right
  uint8_t ny = nv(e->y); // true if player overlaps below
  uint8_t cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  uint8_t cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  uint8_t celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  uint8_t celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

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
