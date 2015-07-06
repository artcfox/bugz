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

void null_input(ENTITY* e) { }
void null_update(ENTITY* e) { }
void null_render(ENTITY* e) { }

void entity_init(ENTITY* e, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx, int16_t impulse)
{
  e->input = input;
  e->update = update;
  e->render = render;
  e->tag = tag;
  e->x = x;
  e->y = y;
  e->maxdx = maxdx;
  e->impulse = impulse;
  e->visible = true;
  e->jumpReleased = true;
  e->dx = e->dy = e->falling = e->jumping = e->left = e->right = e->up = e->down = e->jump = e->turbo = e->monsterhop = e->dead = e->animationFrameCounter = e->autorespawn = 0;
}

void ai_walk_until_blocked(ENTITY* e)
{
  // Collision Detection for X
  uint8_t tx;
  if (e->left)
    tx = p2ht(e->x - (1 << FP_SHIFT) + 1);
  else
    tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);

  if (e->left) {
    uint8_t cell = isSolid(GetTile(tx, ty));
    if (cell) {
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    uint8_t cellright = isSolid(GetTile(tx + 1, ty));
    if (cellright) {
      e->right = false;
      e->left = true;
    }
  }
}

void ai_hop_until_blocked(ENTITY* e)
{
  ai_walk_until_blocked(e);
  e->jump = true;
}

void ai_walk_until_blocked_or_ledge(ENTITY* e)
{
  // Collision Detection for X and Y
  uint8_t tx;
  if (e->left)
    tx = p2ht(e->x - (1 << FP_SHIFT) + 1);
  else
    tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);

  if (e->left) {
    uint8_t cell = isSolid(GetTile(tx, ty));
    uint8_t celldown = isSolid(GetTile(tx, ty + 1));
    if (cell || (!celldown && !e->falling)) {
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    uint8_t cellright = isSolid(GetTile(tx + 1, ty));
    uint8_t celldiag  = isSolid(GetTile(tx + 1, ty + 1));
    if (cellright || (!celldiag && !e->falling)) {
      e->right = false;
      e->left = true;
    }
  }
}

void ai_hop_until_blocked_or_ledge(ENTITY* e)
{
  if (!e->jumping)
    ai_walk_until_blocked_or_ledge(e);
  else
    ai_walk_until_blocked(e);
  e->jump = true;
}

void ai_fly_vertical(ENTITY* e)
{
  // The high/low bytes of e->impulse are used to store the bounds for changing direction
  if (e->up) {
    uint8_t yMin = (uint8_t)e->impulse;
    int16_t yBound = vt2p(yMin);
    if (e->y <= yBound) {
      e->up = false;
      e->down = true;
    }
  } else if (e->down) {
    uint8_t yMax = (uint8_t)(((uint16_t)e->impulse) >> 8); // e->impulse is originally signed, so cast before shift
    int16_t yBound = vt2p(yMax);
    if (e->y >= yBound) {
      e->down = false;
      e->up = true;
    }
  }
}

void ai_fly_horizontal(ENTITY* e)
{
  // The high/low bytes of e->impulse are used to store the bounds for changing direction
  if (e->left) {
    uint8_t xMin = (uint8_t)e->impulse;
    int16_t xBound = ht2p(xMin);
    if (e->x <= xBound) {
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    uint8_t xMax = (uint8_t)(((uint16_t)e->impulse) >> 8); // e->impulse is originally signed, so cast before shift
    int16_t xBound = ht2p(xMax);
    if (e->x >= xBound) {
      e->right = false;
      e->left = true;
    }
  }
}

struct UPDATE_BITFLAGS;
typedef struct UPDATE_BITFLAGS UPDATE_BITFLAGS;

struct UPDATE_BITFLAGS {
  unsigned int wasLeft : 1;
  unsigned int wasRight : 1;
  unsigned int nx : 1;
  unsigned int ny : 1;
  unsigned int cell : 1;
  unsigned int cellright : 1;
  unsigned int celldown : 1;
  unsigned int celldiag : 1;
};

void entity_update(ENTITY* e)
{
  UPDATE_BITFLAGS u;

  u.wasLeft = (bool)(e->dx < 0);
  u.wasRight = (bool)(e->dx > 0);

  int16_t ddx = 0;
  int16_t ddy = WORLD_GRAVITY;

  if (e->left)
    ddx -= WORLD_ACCEL;    // entity wants to go left
  else if (u.wasLeft)
    ddx += WORLD_FRICTION; // entity was going left, but not anymore

  if (e->right)
    ddx += WORLD_ACCEL;    // entity wants to go right
  else if (u.wasRight)
    ddx -= WORLD_FRICTION; // entity was going right, but not anymore

  if (e->jump && !e->jumping && !e->falling) {
    e->dy = 0;            // reset vertical velocity so jumps during grace period are consistent with jumps from ground
    ddy -= e->impulse; // apply an instantaneous (large) vertical impulse
    e->jumping = true;
  }

  // Clamp horizontal velocity to zero if we detect that the entities direction has changed
  if ((u.wasLeft && (e->dx > 0)) || (u.wasRight && (e->dx < 0))) {
    e->dx = 0; // clamp at zero to prevent friction from making the entity jiggle side to side
  }

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (ddx / WORLD_FPS);
  if (e->dx < -e->maxdx)
    e->dx = -e->maxdx;
  else if (e->dx > e->maxdx)
    e->dx = e->maxdx;

  // Collision Detection for X
  uint8_t tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);
  //u.nx = (bool)nh(e->x);  // true if entity overlaps right
  u.ny = (bool)nv(e->y);  // true if entity overlaps below
  u.cell      = (bool)isSolid(GetTile(tx,     ty));
  u.cellright = (bool)isSolid(GetTile(tx + 1, ty));
  u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
  u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));

  if (e->dx > 0) {
    if ((u.cellright && !u.cell) ||
        (u.celldiag  && !u.celldown && u.ny)) {
      e->x = ht2p(tx);     // clamp the x position to avoid moving into the platform just hit
      e->dx = 0;           // stop horizontal velocity
      //u.nx = false;          // entity no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
      u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));
    }
  } else if (e->dx < 0) {
    if ((u.cell     && !u.cellright) ||
        (u.celldown && !u.celldiag && u.ny)) {
      e->x = ht2p(tx + 1); // clamp the x position to avoid moving into the platform just hit
      e->dx = 0;           // stop horizontal velocity
      //u.nx = false;              // entity no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
      u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));
    }
  }

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->prevY = e->y;
  e->y += (e->dy / WORLD_FPS);
  e->dy += (ddy / WORLD_FPS);
  if (e->dy < -WORLD_MAXDY)
    e->dy = -WORLD_MAXDY;
  else if (e->dy > WORLD_MAXDY)
    e->dy = WORLD_MAXDY;

  // Has entity fallen down a hole?
  if (e->y > ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT))) {
    e->y = ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT));
    e->dy = 0;
    e->dead = true;
    return;
  }

  // Collision Detection for Y
  tx = p2ht(e->x);
  ty = p2vt(e->y);
  u.nx = (bool)nh(e->x);  // true if entity overlaps right
  //u.ny = (bool)nv(e->y);  // true if entity overlaps below
  u.cell      = (bool)isSolid(GetTile(tx,     ty));
  u.cellright = (bool)isSolid(GetTile(tx + 1, ty));
  u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
  u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));

  if (e->dy > 0) {
    if ((u.celldown && !u.cell) ||
        (u.celldiag && !u.cellright && u.nx)) {
      e->y = vt2p(ty);     // clamp the y position to avoid falling into platform below
      e->dy = 0;           // stop downward velocity
      e->falling = false;  // no longer falling
      e->jumping = false;  // (or jumping)
      //u.ny = false;          // no longer overlaps the cells below
    }
  } else if (e->dy < 0) {
    if ((u.cell      && !u.celldown) ||
        (u.cellright && !u.celldiag && u.nx)) {
      e->y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      e->dy = 0;           // stop updard velocity
      //u.ny = false;          // no longer overlaps the cells below
    }
  }

  e->falling = !(u.celldown || (u.nx && u.celldiag)) && !e->jumping; // detect if we're now falling or not
}

void entity_update_dying(ENTITY* e)
{
  // Check to see if we should hide the entity now
  if (!e->visible) {
    sprites[e->tag].x = OFF_SCREEN;
    e->render = null_render;
    return;
  }

  int16_t ddy;
  if (e->up)
    ddy = -WORLD_GRAVITY / 4;
  else
    ddy = WORLD_GRAVITY;

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->prevY = e->y;
  e->y += (e->dy / WORLD_FPS);
  e->dy += (ddy / WORLD_FPS);
  if (e->dy < -WORLD_MAXDY)
    e->dy = -WORLD_MAXDY;
  else if (e->dy > WORLD_MAXDY)
    e->dy = WORLD_MAXDY;

  // Clamp Y to within screen bounds
  if (e->y > ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT))) {
    e->y = ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT));
    e->dy = 0;
    e->visible = false; // we hit the edge of the screen, so now hide the entity
    
  }
  if (e->y < 0) {
    e->y = 0;
    e->dy = 0;
    e->visible = false; // we hit the edge of the screen, so now hide the entity
  }
}

struct UPDATE_FLYING_BITFLAGS;
typedef struct UPDATE_FLYING_BITFLAGS UPDATE_FLYING_BITFLAGS;

struct UPDATE_FLYING_BITFLAGS {
  unsigned int wasLeft : 1;
  unsigned int wasRight : 1;
  unsigned int wasUp : 1;
  unsigned int wasDown : 1;
};

void entity_update_flying(ENTITY* e)
{
  UPDATE_FLYING_BITFLAGS u;

  u.wasLeft = e->dx < 0;
  u.wasRight = e->dx > 0;
  u.wasUp = e->dy < 0;
  u.wasDown = e->dy > 0;

  int16_t ddx = 0;
  int16_t ddy = 0;

  if (e->left)
    ddx -= WORLD_ACCEL;    // entity wants to go left
  else if (u.wasLeft)
    ddx += WORLD_FRICTION; // entity was going left, but not anymore

  if (e->right)
    ddx += WORLD_ACCEL;    // entity wants to go right
  else if (u.wasRight)
    ddx -= WORLD_FRICTION; // entity was going right, but not anymore

  if (e->up)
    ddy -= WORLD_ACCEL;    // entity wants to go up
  else if (u.wasUp)
    ddy += WORLD_FRICTION; // entity was going up, but not anymore

  if (e->down)
    ddy += WORLD_ACCEL;    // entity wants to go down
  else if (u.wasDown)
    ddy -= WORLD_FRICTION; // entity was going down, but not anymore

  // Clamp horizontal velocity to zero if we detect that the entities direction has changed
  if ((u.wasLeft && (e->dx > 0)) || (u.wasRight && (e->dx < 0))) {
    e->dx = 0; // clamp at zero to prevent friction from making the entity jiggle side to side
  }

  // Clamp vertical velocity to zero if we detect that the entities direction has changed
  if ((u.wasUp && (e->dy > 0)) || (u.wasDown && (e->dy < 0))) {
    e->dy = 0; // clamp at zero to prevent friction from making the entity jiggle up and down
  }

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (ddx / WORLD_FPS);
  if (e->turbo) {
    if (e->dx < -(e->maxdx + WORLD_METER))
      e->dx = -(e->maxdx + WORLD_METER);
    else if (e->dx > (e->maxdx + WORLD_METER))
      e->dx = (e->maxdx + WORLD_METER);
  } else {
    if (e->dx < -e->maxdx)
      e->dx = -e->maxdx;
    else if (e->dx > e->maxdx)
      e->dx = e->maxdx;
  }

  // Clamp X to within screen bounds
  if (e->x > ((SCREEN_TILES_H - 1) * (TILE_WIDTH << FP_SHIFT))) {
    e->x = ((SCREEN_TILES_H - 1) * (TILE_WIDTH << FP_SHIFT));
    e->dx = 0;
  }
  if (e->x < 0) {
    e->x = 0;
    e->dx = 0;
  }

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->prevY = e->y;
  e->y += (e->dy / WORLD_FPS);
  e->dy += (ddy / WORLD_FPS);
  if (e->turbo) {
    if (e->dy < -(e->maxdx + WORLD_METER)) // uses e->maxdx as maxdy, since there is no gravity
      e->dy = -(e->maxdx + WORLD_METER);
    else if (e->dy > (e->maxdx + WORLD_METER))
      e->dy = (e->maxdx + WORLD_METER);
  } else {
    if (e->dy < -e->maxdx)
      e->dy = -e->maxdx;
    else if (e->dy > e->maxdx)
      e->dy = e->maxdx;
  }

  // Clamp Y to within screen bounds
  if (e->y > ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT))) {
    e->y = ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT));
    e->dy = 0;
  }
  if (e->y < 0) {
    e->y = 0;
    e->dy = 0;
  }
}

#define LADYBUG_ANIMATION_START 9
#define LADYBUG_DEAD (LADYBUG_ANIMATION_START - 3)
#define LADYBUG_STATIONARY (LADYBUG_ANIMATION_START - 2)
#define LADYBUG_JUMP (LADYBUG_ANIMATION_START - 1)
#define LADYBUG_ANIMATION_FRAME_SKIP 6
const uint8_t ladybugAnimation[] PROGMEM = { 0, 1, 2 };

void ladybug_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = LADYBUG_DEAD;
  } else {
    if (e->jumping || e->falling || e->update == entity_update_flying) {
      if (e->dy >= 0)
        sprites[e->tag].tileIndex = LADYBUG_STATIONARY;
      else
        sprites[e->tag].tileIndex = LADYBUG_JUMP;
    } else {
      if (!e->left && !e->right) {
        sprites[e->tag].tileIndex = LADYBUG_STATIONARY;
      } else {
        for (uint8_t i = 0; i < (e->turbo ? 2 : 1); ++i) { // turbo makes animations faster
          if ((e->animationFrameCounter % LADYBUG_ANIMATION_FRAME_SKIP) == 0)
            sprites[e->tag].tileIndex = LADYBUG_ANIMATION_START + pgm_read_byte(&ladybugAnimation[e->animationFrameCounter / LADYBUG_ANIMATION_FRAME_SKIP]);
          if (++e->animationFrameCounter == LADYBUG_ANIMATION_FRAME_SKIP * NELEMS(ladybugAnimation))
            e->animationFrameCounter = 0;
        }
      }
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define ANT_ANIMATION_START 15
#define ANT_DEAD (ANT_ANIMATION_START - 3)
#define ANT_STATIONARY (ANT_ANIMATION_START - 2)
#define ANT_JUMP (ANT_ANIMATION_START - 1)
#define ANT_ANIMATION_FRAME_SKIP 6
const uint8_t antAnimation[] PROGMEM = { 0, 1, 2 };

void ant_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = ANT_DEAD;
  } else {
    if (e->jumping || e->falling || e->update == entity_update_flying) {
      if (e->dy >= 0)
        sprites[e->tag].tileIndex = ANT_STATIONARY;
      else
        sprites[e->tag].tileIndex = ANT_JUMP;
    } else {
      if (!e->left && !e->right) {
        sprites[e->tag].tileIndex = ANT_STATIONARY;
      } else {
        for (uint8_t i = 0; i < (e->turbo ? 2 : 1); ++i) { // turbo makes animations faster
          if ((e->animationFrameCounter % ANT_ANIMATION_FRAME_SKIP) == 0)
            sprites[e->tag].tileIndex = ANT_ANIMATION_START + pgm_read_byte(&antAnimation[e->animationFrameCounter / ANT_ANIMATION_FRAME_SKIP]);
          if (++e->animationFrameCounter == ANT_ANIMATION_FRAME_SKIP * NELEMS(antAnimation))
            e->animationFrameCounter = 0;
        }
      }
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define CRICKET_ANIMATION_START 21
#define CRICKET_DEAD (CRICKET_ANIMATION_START - 3)
#define CRICKET_STATIONARY (CRICKET_ANIMATION_START - 2)
#define CRICKET_JUMP (CRICKET_ANIMATION_START - 1)
#define CRICKET_ANIMATION_FRAME_SKIP 6
const uint8_t cricketAnimation[] PROGMEM = { 0, 1, 2 };

void cricket_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = CRICKET_DEAD;
  } else {
    if (e->jumping || e->falling || e->update == entity_update_flying) {
      if (e->dy >= 0)
        sprites[e->tag].tileIndex = CRICKET_STATIONARY;
      else
        sprites[e->tag].tileIndex = CRICKET_JUMP;
    } else {
      if (!e->left && !e->right) {
        sprites[e->tag].tileIndex = CRICKET_STATIONARY;
      } else {
        for (uint8_t i = 0; i < (e->turbo ? 2 : 1); ++i) { // turbo makes animations faster
          if ((e->animationFrameCounter % CRICKET_ANIMATION_FRAME_SKIP) == 0)
            sprites[e->tag].tileIndex = CRICKET_ANIMATION_START + pgm_read_byte(&cricketAnimation[e->animationFrameCounter / CRICKET_ANIMATION_FRAME_SKIP]);
          if (++e->animationFrameCounter == CRICKET_ANIMATION_FRAME_SKIP * NELEMS(cricketAnimation))
            e->animationFrameCounter = 0;
        }
      }
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define GRASSHOPPER_ANIMATION_START 27
#define GRASSHOPPER_DEAD (GRASSHOPPER_ANIMATION_START - 3)
#define GRASSHOPPER_STATIONARY (GRASSHOPPER_ANIMATION_START - 2)
#define GRASSHOPPER_JUMP (GRASSHOPPER_ANIMATION_START - 1)
#define GRASSHOPPER_ANIMATION_FRAME_SKIP 6
const uint8_t grasshopperAnimation[] PROGMEM = { 0, 1, 2 };

void grasshopper_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = GRASSHOPPER_DEAD;
  } else {
    if (e->jumping || e->falling || e->update == entity_update_flying) {
      if (e->dy >= 0)
        sprites[e->tag].tileIndex = GRASSHOPPER_STATIONARY;
      else
        sprites[e->tag].tileIndex = GRASSHOPPER_JUMP;
    } else {
      if (!e->left && !e->right) {
        sprites[e->tag].tileIndex = GRASSHOPPER_STATIONARY;
      } else {
        for (uint8_t i = 0; i < (e->turbo ? 2 : 1); ++i) { // turbo makes animations faster
          if ((e->animationFrameCounter % GRASSHOPPER_ANIMATION_FRAME_SKIP) == 0)
            sprites[e->tag].tileIndex = GRASSHOPPER_ANIMATION_START + pgm_read_byte(&grasshopperAnimation[e->animationFrameCounter / GRASSHOPPER_ANIMATION_FRAME_SKIP]);
          if (++e->animationFrameCounter == GRASSHOPPER_ANIMATION_FRAME_SKIP * NELEMS(grasshopperAnimation))
            e->animationFrameCounter = 0;
        }
      }
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define FRUITFLY_ANIMATION_START 31
#define FRUITFLY_DEAD (FRUITFLY_ANIMATION_START - 1)
#define FRUITFLY_ANIMATION_FRAME_SKIP 2
const uint8_t fruitflyAnimation[] PROGMEM = { 0, 1, 2, 3, 2, 1 };

void fruitfly_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = FRUITFLY_DEAD;
  } else {
    if ((e->animationFrameCounter % FRUITFLY_ANIMATION_FRAME_SKIP) == 0)
      sprites[e->tag].tileIndex = FRUITFLY_ANIMATION_START + pgm_read_byte(&fruitflyAnimation[e->animationFrameCounter / FRUITFLY_ANIMATION_FRAME_SKIP]);
    if (++e->animationFrameCounter == FRUITFLY_ANIMATION_FRAME_SKIP * NELEMS(fruitflyAnimation))
      e->animationFrameCounter = 0;
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define BEE_ANIMATION_START 36
#define BEE_DEAD (BEE_ANIMATION_START - 1)
#define BEE_ANIMATION_FRAME_SKIP 2
const uint8_t beeAnimation[] PROGMEM = { 0, 1, 2, 3, 2, 1 };

void bee_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = BEE_DEAD;
  } else {
    if ((e->animationFrameCounter % BEE_ANIMATION_FRAME_SKIP) == 0)
      sprites[e->tag].tileIndex = BEE_ANIMATION_START + pgm_read_byte(&beeAnimation[e->animationFrameCounter / BEE_ANIMATION_FRAME_SKIP]);
    if (++e->animationFrameCounter == BEE_ANIMATION_FRAME_SKIP * NELEMS(beeAnimation))
      e->animationFrameCounter = 0;
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define SPIDER_ANIMATION_START 41
#define SPIDER_DEAD (SPIDER_ANIMATION_START - 1)
#define SPIDER_ANIMATION_FRAME_SKIP 8
const uint8_t spiderAnimation[] PROGMEM = { 0, 1 };

void spider_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = SPIDER_DEAD;
  } else {
    for (uint8_t i = 0; i < (e->turbo ? 2 : 1); ++i) { // turbo makes animations faster
      if ((e->animationFrameCounter % SPIDER_ANIMATION_FRAME_SKIP) == 0)
        sprites[e->tag].tileIndex = SPIDER_ANIMATION_START + pgm_read_byte(&spiderAnimation[e->animationFrameCounter / SPIDER_ANIMATION_FRAME_SKIP]);
      if (++e->animationFrameCounter == SPIDER_ANIMATION_FRAME_SKIP * NELEMS(spiderAnimation))
        e->animationFrameCounter = 0;
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

// ---------- PLAYER

void player_init(PLAYER* p, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx, int16_t impulse)
{
  entity_init((ENTITY*)p, input, update, render, tag, x, y, maxdx, impulse);
  memset(&p->buttons, 0, sizeof(p->buttons));
  p->framesFalling = 0;
}

void player_input(ENTITY* e)
{
  PLAYER* p = (PLAYER*)e; // upcast

  // Read the current state of the player's controller
  p->buttons.prev = p->buttons.held;
  p->buttons.held = ReadJoypad(e->tag); // tag will be set to 0 or 1, depending on which player we are
  //p->buttons.pressed = p->buttons.held & (p->buttons.held ^ p->buttons.prev);
  //p->buttons.released = p->buttons.prev & (p->buttons.held ^ p->buttons.prev);

  e->left = (bool)(p->buttons.held & BTN_LEFT);
  e->right = (bool)(p->buttons.held & BTN_RIGHT);
  e->up = (bool)(p->buttons.held & BTN_UP);
  e->down = (bool)(p->buttons.held & BTN_DOWN);
  e->turbo = (bool)(p->buttons.held & BTN_B);

  // Improve the user experience, by allowing players to jump by holding the jump
  // button before landing, but require them to release it before jumping again
  if (e->jumpReleased) {                                      // Jumping multiple times requires releasing the jump button between jumps
    e->jump = (bool)(p->buttons.held & BTN_A);                      // player[i].jump can only be true if BTN_A has been released from the previous jump
    if (e->jump && !(e->jumping || (e->falling && p->framesFalling > WORLD_FALLING_GRACE_FRAMES))) { // if player[i] is currently holding BTN_A, (and is on the ground)
      e->jumpReleased = false;                                // a jump will occur during the next call to update(), so clear the jumpReleased flag.
      //TriggerFx(0, 128, false);
    }
  } else {                                                           // Otherwise, it means that we just jumped, and BTN_A is still being held down
    e->jump = false;                                          // so explicitly disallow any additional jumps until
    uint16_t released = p->buttons.prev & (p->buttons.held ^ p->buttons.prev);
    if (released & BTN_A)                                 // BTN_A is released again
      e->jumpReleased = true;                                 // at which point reset the jumpReleased flag so another jump may occur.
  }
}

void player_update(ENTITY* e)
{
  PLAYER* p = (PLAYER*)e; // upcast
  UPDATE_BITFLAGS u;

  u.wasLeft = e->dx < 0;
  u.wasRight = e->dx > 0;

  int16_t ddx = 0;
  int16_t ddy = WORLD_GRAVITY;

  if (e->left)
    ddx -= WORLD_ACCEL;    // player wants to go left
  else if (u.wasLeft)
    ddx += WORLD_FRICTION; // player was going left, but not anymore

  if (e->right)
    ddx += WORLD_ACCEL;    // player wants to go right
  else if (u.wasRight)
    ddx -= WORLD_FRICTION; // player was going right, but not anymore

  if (e->jump && !e->jumping && !(e->falling ? (p->framesFalling > WORLD_FALLING_GRACE_FRAMES) : false)) {
    TriggerFx(0, 128, true);
    e->dy = 0;             // reset vertical velocity so jumps during grace period are consistent with jumps from ground
    ddy -= e->impulse;     // apply an instantaneous (large) vertical impulse
    e->jumping = true;
  }

  // Bounce a bit when you stomp a monster
  if (e->monsterhop) {
    e->monsterhop = e->jumping = e->falling = false;
    e->jumpReleased = true;
    e->dy = p->framesFalling = 0;
    ddy -= (e->impulse / 2);
  }

  // Variable height jumping
  if (e->jumping && e->jumpReleased && (e->dy < -WORLD_CUT_JUMP_SPEED_LIMIT))
      e->dy = -WORLD_CUT_JUMP_SPEED_LIMIT;

  // Clamp horizontal velocity to zero if we detect that the players direction has changed
  if ((u.wasLeft && (e->dx > 0)) || (u.wasRight && (e->dx < 0))) {
    e->dx = 0; // clamp at zero to prevent friction from making us jiggle side to side
  }

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (ddx / WORLD_FPS);
  if (e->turbo) {
    if (e->dx < -(e->maxdx + WORLD_METER))
      e->dx = -(e->maxdx + WORLD_METER);
    else if (e->dx > (e->maxdx + WORLD_METER))
      e->dx = (e->maxdx + WORLD_METER);
  } else {
    if (e->dx < -e->maxdx)
      e->dx = -e->maxdx;
    else if (e->dx > e->maxdx)
      e->dx = e->maxdx;
  }

  // Collision Detection for X
  uint8_t tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);
  //u.nx = (bool)nh(e->x);  // true if player overlaps right
  u.ny = (bool)nv(e->y); // true if player overlaps below
  u.cell      = (bool)isSolid(GetTile(tx,     ty));
  u.cellright = (bool)isSolid(GetTile(tx + 1, ty));
  u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
  u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));

  if (e->dx > 0) {
    if ((u.cellright && !u.cell) ||
        (u.celldiag  && !u.celldown && u.ny)) {
      e->x = ht2p(tx);     // clamp the x position to avoid moving into the platform we just hit
      e->dx = 0;           // stop horizontal velocity
      //u.nx = false;        // player no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
      u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));
    }
  } else if (e->dx < 0) {
    if ((u.cell     && !u.cellright) ||
        (u.celldown && !u.celldiag && u.ny)) {
      e->x = ht2p(tx + 1); // clamp the x position to avoid moving into the platform we just hit
      e->dx = 0;           // stop horizontal velocity
      //u.nx = false;        // player no longer overlaps the adjacent cell
      tx = p2ht(e->x);
      u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
      u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));
    }
  }

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->prevY = e->y;
  e->y += (e->dy / WORLD_FPS);
  e->dy += (ddy / WORLD_FPS);
  if (e->dy < -WORLD_MAXDY)
    e->dy = -WORLD_MAXDY;
  else if (e->dy > WORLD_MAXDY)
    e->dy = WORLD_MAXDY;

  // Has entity fallen down a hole?
  if (e->y > ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT))) {
    e->y = ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT));
    e->dy = 0;
    e->dead = true;
    return;
  }

  // Collision Detection for Y
  tx = p2ht(e->x);
  ty = p2vt(e->y);
  u.nx = (bool)nh(e->x);  // true if player overlaps right
  //u.ny = (bool)nv(e->y); // true if player overlaps below
  u.cell      = (bool)isSolid(GetTile(tx,     ty));
  u.cellright = (bool)isSolid(GetTile(tx + 1, ty));
  u.celldown  = (bool)isSolid(GetTile(tx,     ty + 1));
  u.celldiag  = (bool)isSolid(GetTile(tx + 1, ty + 1));

  if (e->dy > 0) {
    if ((u.celldown && !u.cell) ||
        (u.celldiag && !u.cellright && u.nx)) {
      e->y = vt2p(ty);     // clamp the y position to avoid falling into platform below
      e->dy = 0;           // stop downward velocity
      e->falling = false;  // no longer falling
      p->framesFalling = 0;
      e->jumping = false;  // (or jumping)
      //u.ny = false;        // no longer overlaps the cells below
    }
  } else if (e->dy < 0) {
    if ((u.cell      && !u.celldown) ||
        (u.cellright && !u.celldiag && u.nx)) {
      e->y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      e->dy = 0;           // stop updard velocity
      //u.ny = false;        // player no longer overlaps the cells below
    }
  }

  e->falling = !(u.celldown || (u.nx && u.celldiag)) && !e->jumping; // detect if we're now falling or not
  if (e->falling && p->framesFalling < 255)
    p->framesFalling++;
}

#define PLAYER_ANIMATION_START 3
#define PLAYER_DEAD (PLAYER_ANIMATION_START - 3)
#define PLAYER_STATIONARY (PLAYER_ANIMATION_START - 2)
#define PLAYER_JUMP (PLAYER_ANIMATION_START - 1)
#define PLAYER_ANIMATION_FRAME_SKIP 6
const uint8_t playerAnimation[] PROGMEM = { 0, 1, 2, 1 };

void player_render(ENTITY* e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = PLAYER_DEAD;
  } else {
    if (e->jumping || e->falling || e->update == entity_update_flying) {
      if (e->dy >= 0)
        sprites[e->tag].tileIndex = PLAYER_STATIONARY;
      else
        sprites[e->tag].tileIndex = PLAYER_JUMP;
    } else {
      if (!e->left && !e->right) {
        sprites[e->tag].tileIndex = PLAYER_STATIONARY;
      } else {
        for (uint8_t i = 0; i < (e->turbo ? 2 : 1); ++i) { // turbo makes animations faster
          if ((e->animationFrameCounter % PLAYER_ANIMATION_FRAME_SKIP) == 0)
            sprites[e->tag].tileIndex = PLAYER_ANIMATION_START + pgm_read_byte(&playerAnimation[e->animationFrameCounter / PLAYER_ANIMATION_FRAME_SKIP]);
          if (++e->animationFrameCounter == PLAYER_ANIMATION_FRAME_SKIP * NELEMS(playerAnimation))
            e->animationFrameCounter = 0;
        }
      }
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

/* #define PLAYER_START 0 */

/* void player_render(ENTITY* e) */
/* { */
/*   if (e->left == e->right) { */
/*     sprites[e->tag].tileIndex = 1 + PLAYER_START + (e->tag * 2); */
/*     sprites[e->tag].flags = 0; */
/*   } else { */
/*     sprites[e->tag].tileIndex = PLAYER_START + (e->tag * 2); */

/*     if (e->left) */
/*       sprites[e->tag].flags = 0; */
/*     if (e->right) */
/*       sprites[e->tag].flags = SPRITE_FLIP_X; */
/*   } */

/*   sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT; */
/*   sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT; */
/* } */
