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
const uint8_t isSolid[] PROGMEM = { 0, 1, 1, 0, 0, 0, 0, 0, 0 };

void null_input(ENTITY* e) { }
void null_update(ENTITY* e) { }
void null_render(ENTITY* e) { }

bool overlap(uint16_t x1, uint16_t y1, uint8_t w1, uint8_t h1, uint16_t x2, uint16_t y2, uint8_t w2, uint8_t h2)
{
  return !(((x1 + w1 - 1) < x2) ||
           ((x2 + w2 - 1) < x1) ||
           ((y1 + h1 - 1) < y2) ||
           ((y2 + h2 - 1) < y1));
}

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
  e->dx = e->dy = e->ddx = e->ddy = e->falling = e->jumping = e->left = e->right = e->up = e->down = e->jump = e->animationFrameCounter = 0;
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
  uint8_t cell      = pgm_read_byte(&isSolid[GetTile(tx    , ty)]);
  uint8_t cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);

  if (e->left) {
    if (cell) {
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
  uint8_t cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  uint8_t cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  uint8_t celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  uint8_t celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

  if (e->left) {
    if (cell || (!celldown && !e->falling)) {
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
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
  // Cast impulse back into Y limits
  uint8_t yMin = (uint8_t)e->impulse;
  uint8_t yMax = (uint8_t)(((uint16_t)e->impulse) >> 8);

  if (e->up) {
    int16_t yBound = vt2p(yMin);
    if (e->y <= yBound) {
      e->up = false;
      e->down = true;
    }
  } else if (e->down) {
    int16_t yBound = vt2p(yMax);
    if (e->y >= yBound) {
      e->down = false;
      e->up = true;
    }
  }
}

void ai_fly_horizontal(ENTITY* e)
{
  // Cast impulse back into X limits
  uint8_t xMin = (uint8_t)e->impulse;
  uint8_t xMax = (uint8_t)(((uint16_t)e->impulse) >> 8);

  if (e->left) {
    int16_t xBound = ht2p(xMin);
    if (e->x <= xBound) {
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    int16_t xBound = ht2p(xMax);
    if (e->x >= xBound) {
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
    e->ddy -= e->impulse; // apply an instantaneous (large) vertical impulse
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

void entity_update_dying(ENTITY* e)
{
  // Check to see if we should hide the entity now
  if (!e->visible) {
    sprites[e->tag].x = OFF_SCREEN;
    e->render = null_render;
    return;
  }

  e->ddy = WORLD_GRAVITY;

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->y += (e->dy / WORLD_FPS);
  e->dy += (e->ddy / WORLD_FPS);
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

void entity_update_flying(ENTITY* e)
{
  bool wasLeft = e->dx < 0;
  bool wasRight = e->dx > 0;
  bool wasUp = e->dy < 0;
  bool wasDown = e->dy > 0;

  e->ddx = 0;
  e->ddy = 0;

  if (e->left)
    e->ddx -= WORLD_ACCEL;    // entity wants to go left
  else if (wasLeft)
    e->ddx += WORLD_FRICTION; // entity was going left, but not anymore

  if (e->right)
    e->ddx += WORLD_ACCEL;    // entity wants to go right
  else if (wasRight)
    e->ddx -= WORLD_FRICTION; // entity was going right, but not anymore

  if (e->up)
    e->ddy -= WORLD_ACCEL;    // entity wants to go up
  else if (wasUp)
    e->ddy += WORLD_FRICTION; // entity was going up, but not anymore

  if (e->down)
    e->ddy += WORLD_ACCEL;    // entity wants to go down
  else if (wasDown)
    e->ddy -= WORLD_FRICTION; // entity was going down, but not anymore

  // Clamp horizontal velocity to zero if we detect that the entities direction has changed
  if ((wasLeft && (e->dx > 0)) || (wasRight && (e->dx < 0))) {
    e->dx = 0; // clamp at zero to prevent friction from making the entity jiggle side to side
  }

  // Clamp vertical velocity to zero if we detect that the entities direction has changed
  if ((wasUp && (e->dy > 0)) || (wasDown && (e->dy < 0))) {
    e->dy = 0; // clamp at zero to prevent friction from making the entity jiggle up and down
  }

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (e->ddx / WORLD_FPS);
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
  e->y += (e->dy / WORLD_FPS);
  e->dy += (e->ddy / WORLD_FPS);
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

#define LADYBUG_START 5

void ladybug_render(ENTITY* e)
{
  if (e->update == entity_update_dying) {
    sprites[e->tag].tileIndex = LADYBUG_START - 1; // assumes dead sprite comes directly before start
  } else {
    sprites[e->tag].tileIndex = LADYBUG_START;
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define ANT_START 7

void ant_render(ENTITY* e)
{
  if (e->update == entity_update_dying) {
    sprites[e->tag].tileIndex = ANT_START - 1; // assumes dead sprite comes directly before start
  } else {
    sprites[e->tag].tileIndex = ANT_START;
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define CRICKET_START 9

void cricket_render(ENTITY* e)
{
  if (e->update == entity_update_dying) {
    sprites[e->tag].tileIndex = CRICKET_START - 1; // assumes dead sprite comes directly before start
  } else {
    if (e->dy >= 0)
      sprites[e->tag].tileIndex = CRICKET_START;
    else
      sprites[e->tag].tileIndex = CRICKET_START + 1;
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define GRASSHOPPER_START 12

void grasshopper_render(ENTITY* e)
{
  if (e->update == entity_update_dying) {
    sprites[e->tag].tileIndex = GRASSHOPPER_START - 1; // assumes dead sprite comes directly before start
  } else {
    if (e->dy >= 0)
      sprites[e->tag].tileIndex = GRASSHOPPER_START;
    else
      sprites[e->tag].tileIndex = GRASSHOPPER_START + 1;
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define FRUITFLY_ANIMATION_START 15
#define FRUITFLY_ANIMATION_FRAME_SKIP 2
const uint8_t fruitflyAnimation[] PROGMEM = { 0, 1, 2, 3, 2, 1 };

void fruitfly_render(ENTITY* e)
{
  if (e->update == entity_update_dying) {
    sprites[e->tag].tileIndex = FRUITFLY_ANIMATION_START - 1; // assumes dead sprite comes directly before animation
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

#define BEE_ANIMATION_START 20
#define BEE_ANIMATION_FRAME_SKIP 2
const uint8_t beeAnimation[] PROGMEM = { 0, 1, 2, 3, 2, 1 };

void bee_render(ENTITY* e)
{
  if (e->update == entity_update_dying) {
    sprites[e->tag].tileIndex = BEE_ANIMATION_START - 1; // assumes dead sprite comes directly before animation
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

#define SPIDER_ANIMATION_START 25
#define SPIDER_ANIMATION_FRAME_SKIP 8
const uint8_t spiderAnimation[] PROGMEM = { 0, 1 };

void spider_render(ENTITY* e)
{
  if (e->update == entity_update_dying) {
    sprites[e->tag].tileIndex = SPIDER_ANIMATION_START - 1; // assumes dead sprite comes directly before animation
  } else {
    if ((e->animationFrameCounter % SPIDER_ANIMATION_FRAME_SKIP) == 0)
      sprites[e->tag].tileIndex = SPIDER_ANIMATION_START + pgm_read_byte(&spiderAnimation[e->animationFrameCounter / SPIDER_ANIMATION_FRAME_SKIP]);
    if (++e->animationFrameCounter == SPIDER_ANIMATION_FRAME_SKIP * NELEMS(spiderAnimation))
      e->animationFrameCounter = 0;

    sprites[e->tag].flags = e->right ? SPRITE_FLIP_X : 0;    
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
  e->up = (bool)(p->buttons.held & BTN_UP);
  e->down = (bool)(p->buttons.held & BTN_DOWN);
  e->turbo = (bool)(p->buttons.held & BTN_B);

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
    e->ddy -= e->impulse; // apply an instantaneous (large) vertical impulse
    e->jumping = true;
  }

  // Bounce a bit when you stomp a monster
  if (e->monsterhop) {
    e->monsterhop = e->jumping = e->falling = false;
    p->jumpReleased = true;
    e->dy = p->framesFalling = 0;
    e->ddy -= (e->impulse / 2);
  }

  // Variable height jumping
  if (e->jumping && p->jumpReleased && (e->dy < -WORLD_CUT_JUMP_SPEED_LIMIT))
      e->dy = -WORLD_CUT_JUMP_SPEED_LIMIT;

  // Clamp horizontal velocity to zero if we detect that the players direction has changed
  if ((wasLeft && (e->dx > 0)) || (wasRight && (e->dx < 0))) {
    e->dx = 0; // clamp at zero to prevent friction from making us jiggle side to side
  }

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (e->ddx / WORLD_FPS);
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
  uint8_t nx = nh(e->x);  // true if player overlaps right
  uint8_t ny = nv(e->y); // true if player overlaps below
  uint8_t cell      = pgm_read_byte(&isSolid[GetTile(tx,     ty)]);
  uint8_t cellright = pgm_read_byte(&isSolid[GetTile(tx + 1, ty)]);
  uint8_t celldown  = pgm_read_byte(&isSolid[GetTile(tx,     ty + 1)]);
  uint8_t celldiag  = pgm_read_byte(&isSolid[GetTile(tx + 1, ty + 1)]);

  if (e->dx > 0) {
    if ((cellright && !cell) ||
        (celldiag  && !celldown && ny)) {
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
      e->y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      e->dy = 0;           // stop updard velocity
      ny = 0;                     // player no longer overlaps the cells below
    }
  }

  e->falling = !(celldown || (nx && celldiag)) && !e->jumping; // detect if we're now falling or not
  if (e->falling)
    p->framesFalling++;
}

#define PLAYER_START 0

void player_render(ENTITY* e)
{
  if (e->left == e->right) {
    sprites[e->tag].tileIndex = 1 + PLAYER_START + (e->tag * 2);
    sprites[e->tag].flags = 0;
  } else {
    sprites[e->tag].tileIndex = PLAYER_START + (e->tag * 2);

    if (e->left)
      sprites[e->tag].flags = 0;
    if (e->right)
      sprites[e->tag].flags = SPRITE_FLIP_X;
  }

  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}
