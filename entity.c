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

void null_input(ENTITY* const e) { (void)e; }
void null_render(ENTITY* const e) { sprites[e->tag].x = OFF_SCREEN; }

void entity_init(ENTITY* const e, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), const uint8_t tag, const uint16_t x, const uint16_t y, const int16_t maxdx, const int16_t impulse)
{
  memset(e, 0, sizeof(ENTITY));
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
  e->interacts = true;
}

__attribute__(( always_inline ))
static inline bool isSolidForEntity(const uint16_t offset, const uint8_t ty, const int16_t prevY, const uint8_t entityHeight, const bool down)
{
  uint8_t t = vram[offset] - RAM_TILES_COUNT; // equiv. GetTile(tx, ty)
  // One-way tiles are only solid for Y collisions where your previous Y puts your feet above the tile, and you're not currently pressing down
  return (isSolid(t) || (isOneWay(t) && !down && ((prevY + entityHeight - 1) < vt2p(ty))));
}

void ai_walk_until_blocked(ENTITY* const e)
{
  // Collision Detection for X
  uint8_t tx;
  if (e->left)
    tx = p2ht(e->x - (1 << FP_SHIFT) + 1);
  else
    tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);

  if (e->left) {
    uint16_t offset = ty * SCREEN_TILES_H + tx;
    if ((e->x == 0) || isSolid(vram[offset] - RAM_TILES_COUNT)) { // cell, equiv. GetTile(tx, ty)
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    uint16_t offset = ty * SCREEN_TILES_H + tx + 1;
    if ((tx == SCREEN_TILES_H - 1) || isSolid(vram[offset] - RAM_TILES_COUNT)) { // cellright, equiv. GetTile(tx + 1, ty)
      e->right = false;
      e->left = true;
    }
  }
}

void ai_hop_until_blocked(ENTITY* const e)
{
  ai_walk_until_blocked(e);
  e->jump = true;
}

void ai_walk_until_blocked_or_ledge(ENTITY* const e)
{
  // Collision Detection for X and Y
  uint8_t tx;
  if (e->left)
    tx = p2ht(e->x - (1 << FP_SHIFT) + 1);
  else
    tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);

  uint16_t offset = ty * SCREEN_TILES_H + tx;
  if (e->left) {
    if ((e->x == 0) || !(e->falling ||
                         isSolidForEntity(offset + SCREEN_TILES_H, ty + 1, e->y, WORLD_METER, e->down)) || // celldown, equiv. tx, ty + 1
        isSolid(vram[offset] - RAM_TILES_COUNT)) {                                                         // cell,     equiv. tx, ty
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    if ((tx == SCREEN_TILES_H - 1) || !(e->falling ||
                                        isSolidForEntity(offset + SCREEN_TILES_H + 1, ty + 1, e->y, WORLD_METER, e->down)) || // celldiag,  equiv. tx + 1, ty + 1
        isSolid(vram[offset + 1] - RAM_TILES_COUNT)) {                                                                        // cellright, equiv. tx + 1, ty
      e->right = false;
      e->left = true;
    }
  }
}

void ai_hop_until_blocked_or_ledge(ENTITY* const e)
{
  if (!e->jumping)
    ai_walk_until_blocked_or_ledge(e);
  else
    ai_walk_until_blocked(e);
  e->jump = true;
}

void ai_fly_vertical(ENTITY* const e)
{
  // The high/low bytes of e->impulse are used to store the bounds for changing direction
  if (e->up) {
    uint8_t yMin = (uint8_t)e->impulse; // low byte
    int16_t yBound = vt2p(yMin);
    if (e->y <= yBound) {
      e->up = false;
      e->down = true;
    }
  } else if (e->down) {
    uint8_t yMax = (uint8_t)(((uint16_t)e->impulse) >> 8); // high byte, e->impulse is originally signed, so cast before shift
    int16_t yBound = vt2p(yMax);
    if (e->y >= yBound) {
      e->down = false;
      e->up = true;
    }
  }
}

void ai_fly_horizontal(ENTITY* const e)
{
  // The high/low bytes of e->impulse are used to store the bounds for changing direction
  if (e->left) {
    uint8_t xMin = (uint8_t)e->impulse; // low byte
    int16_t xBound = ht2p(xMin);
    if (e->x <= xBound) {
      e->left = false;
      e->right = true;
    }
  } else if (e->right) {
    uint8_t xMax = (uint8_t)(((uint16_t)e->impulse) >> 8); // high byte, e->impulse is originally signed, so cast before shift
    int16_t xBound = ht2p(xMax);
    if (e->x >= xBound) {
      e->right = false;
      e->left = true;
    }
  }
}

/* struct horiz { */
/*   unsigned int left : 1; */
/*   unsigned int wasLeft : 1; */
/*   unsigned int right : 1; */
/*   unsigned int wasRight : 1; */
/*   unsigned int reserved : 4; */
/* } __attribute__ ((packed)); */

/* // e->wasRight, e->right, e->wasLeft, e->left */
/* uint16_t ddx_table[] = { */
/*   0, // 0 0 0 0 */
/*   -WORLD_ACCEL / WORLD_FPS,   // 0 0 0 1 */
/*   WORLD_FRICTION / WORLD_FPS,   // 0 0 1 0 */
/*   -WORLD_ACCEL / WORLD_FPS,  // 0 0 1 1 */
/*   WORLD_ACCEL / WORLD_FPS,   // 0 1 0 0 */
/*   0,   // 0 1 0 1 */
/*   (WORLD_FRICTION + WORLD_ACCEL) / WORLD_FPS,  // 0 1 1 0 */
/*   0,   // 0 1 1 1 */
/*   -WORLD_FRICTION / WORLD_FPS,   // 1 0 0 0 */
/*   (-WORLD_ACCEL - WORLD_FRICTION) / WORLD_FPS,  // 1 0 0 1 */
/*   0,   // 1 0 1 0 (impossible) */
/*   0,   // 1 0 1 1 (impossible) */
/*   WORLD_ACCEL / WORLD_FPS,   // 1 1 0 0 */
/*   0,   // 1 1 0 1 */
/*   0,   // 1 1 1 0 (impossible) */
/*   0,   // 1 1 1 1 (impossible) */
/* }; */

void entity_update(ENTITY* const e)
{
  bool wasLeft = (e->dx < 0);
  bool wasRight = (e->dx > 0);

  int16_t ddx = 0;

  if (e->left)
    ddx -= WORLD_ACCEL;    // entity wants to go left
  else if (wasLeft)
    ddx += WORLD_FRICTION; // entity was going left, but not anymore

  if (e->right)
    ddx += WORLD_ACCEL;    // entity wants to go right
  else if (wasRight)
    ddx -= WORLD_FRICTION; // entity was going right, but not anymore

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);
  e->dx += (ddx / WORLD_FPS);
  /* struct horiz h = { e->left, wasLeft, e->right, wasRight, 0 }; */
  /* e->dx += ddx_table[*((uint8_t*)&h)]; */
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
  } else if (e->x < 0) {
    e->x = 0;
    e->dx = 0;
  }

  // Clamp horizontal velocity to zero if we detect that the direction has changed
  if ((wasLeft && (e->dx > 0)) || (wasRight && (e->dx < 0)))
    e->dx = 0; // clamp at zero to prevent friction from making the entity jiggle side to side

  // Collision Detection for X
  uint8_t tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);
  bool ny = (bool)nv(e->y); // true if entity overlaps below
  uint16_t offset = ty * SCREEN_TILES_H + tx;
  bool cell      = isSolid(vram[offset                     ] - RAM_TILES_COUNT); // equiv. GetTile(tx,     ty    )
  uint8_t tright =         vram[offset + 1                 ] - RAM_TILES_COUNT;  // equiv. GetTile(tx + 1, ty    )
  bool cellright = isSolid(tright); 
  bool celldown  = isSolid(vram[offset + SCREEN_TILES_H    ] - RAM_TILES_COUNT); // equiv. GetTile(tx,     ty + 1)
  uint8_t tdiag  =         vram[offset + SCREEN_TILES_H + 1] - RAM_TILES_COUNT;  // equiv. GetTile(tx + 1, ty + 1)
  bool celldiag  = isSolid(tdiag);

  if (e->dx > 0) {
    if ((      cellright && !cell) ||
        (ny && celldiag  && !celldown)) {
      e->x = ht2p(tx);     // clamp the x position to avoid moving into the platform just hit
      e->dx = 0;           // stop horizontal velocity
    }
  } else if (e->dx < 0) {
    if ((      cell     && !cellright && !isLadder(tright)) ||
        (ny && celldown && !celldiag  && !isLadder(tdiag))) { // isLadder() checks avoid glitch
      e->x = ht2p(tx + 1); // clamp the x position to avoid moving into the platform just hit
      e->dx = 0;           // stop horizontal velocity
    }
  }


  int16_t ddy = WORLD_GRAVITY;

  // Bounce a bit when you stomp a monster
  if (e->monsterhop) {
    e->monsterhop = e->jumping = e->falling = false;
    e->jumpReleased = true;
    e->framesFalling = 0;
    if (e->dy > 0)
      e->dy = 0;           // if falling down, reset vertical velocity
    ddy -= (WORLD_JUMP >> 1);
  }

  // Jump logic
  if (e->jump && !e->jumping && !(e->falling ? (e->framesFalling > WORLD_FALLING_GRACE_FRAMES) : false)) {
    if (e->tag < PLAYERS)  // only play the jump sound effect when a human player is jumping
      TriggerFx(0, 128, true);
    if (e->dy > 0)
      e->dy = 0;           // if falling down, reset vertical velocity so jumps during grace period are consistent with jumps from ground
    ddy -= e->impulse;     // apply an instantaneous (large) vertical impulse
    e->jumping = true;
    e->jumpReleased = e->falling = false;
  }

  // Variable height jumping
  if (e->jumping && e->jumpReleased && (e->dy < -WORLD_CUT_JUMP_SPEED_LIMIT))
      e->dy = -WORLD_CUT_JUMP_SPEED_LIMIT;

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  int16_t prevY = e->y; // cache previous Y value for one-way tiles
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
    // Kill the entity if it would have fallen through the bottom of the screen
    if (!e->invincible)
      e->dead = true;
    return;
  } else if (e->y < 0) {
    e->y = 0;
    e->dy = 0;
  }

  // Collision Detection for Y
  tx = p2ht(e->x);
  ty = p2vt(e->y);
  bool nx = (bool)nh(e->x);  // true if entity overlaps right
  offset = ty * SCREEN_TILES_H + tx;
  cell      = isSolidForEntity(offset,                      ty,     prevY, WORLD_METER, e->down); // equiv. ... tx,     ty
  cellright = isSolidForEntity(offset + 1,                  ty,     prevY, WORLD_METER, e->down); // equiv. ... tx + 1, ty
  celldown  = isSolidForEntity(offset + SCREEN_TILES_H,     ty + 1, prevY, WORLD_METER, e->down); // equiv. ... tx,     ty + 1
  celldiag  = isSolidForEntity(offset + SCREEN_TILES_H + 1, ty + 1, prevY, WORLD_METER, e->down); // equiv. ... tx + 1, ty + 1

  if (e->dy > 0) {
    if ((      celldown && !cell) ||
        (nx && celldiag && !cellright)) {
      e->y = vt2p(ty);     // clamp the y position to avoid falling into platform below
      e->dy = 0;           // stop downward velocity
      e->jumping = false;  // no longer jumping
      e->framesFalling = 0;
    }
  } else if (e->dy < 0) {
    if ((      cell      && !celldown) ||
        (nx && cellright && !celldiag)) {
      e->y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      e->dy = 0;           // stop updard velocity
    }
  }

  e->falling = !(celldown || (nx && celldiag)) && !e->jumping; // detect if we're now falling or not
  if (e->falling && e->framesFalling <= WORLD_FALLING_GRACE_FRAMES)
    e->framesFalling++;

  // Collision detection with ladders
  if (e->up || e->down) {
    if (e->down) {
      ty = p2vt(e->y + 1);     // the tile one sub-sub-pixel below current position
      ny = (bool)nv(e->y + 1); // true if player overlaps below
    } else { // e->up
      ty = p2vt(e->y - 1);     // the tile one sub-sub-pixel above current position
      ny = (bool)nv(e->y - 1); // true if entity overlaps above
    }
    
    uint16_t offset = ty * SCREEN_TILES_H + tx;
    if ((            isLadder(vram[offset                     ] - RAM_TILES_COUNT)) || // cell,      equiv. ... GetTile(tx,     ty    ) ...
        (nx &&       isLadder(vram[offset + 1                 ] - RAM_TILES_COUNT)) || // cellright, equiv. ... GetTile(tx + 1, ty    ) ...
        (ny &&       isLadder(vram[offset + SCREEN_TILES_H    ] - RAM_TILES_COUNT)) || // celldown,  equiv. ... GetTile(tx,     ty + 1) ...
        (nx && ny && isLadder(vram[offset + SCREEN_TILES_H + 1] - RAM_TILES_COUNT))) { // celldiag,  equiv. ... GetTile(tx + 1, ty + 1) ...
      if (e->down)
        e->y++; // allow entity to join a ladder directly below them
      else // e->up
        e->y--; // allow entity to join a ladder directly above them
      e->update = entity_update_ladder;
      e->animationFrameCounter = 0;
      e->jumping = false;
      e->dx = e->dy = 0;
    }
  }
}

void entity_update_dying(ENTITY* const e)
{
  // Check to see if we should hide the entity now
  if (!e->visible) {
    e->render = null_render;
    return;
  }

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->x += (e->dx / WORLD_FPS);

  // Decelerate any motion along the X axis
  if (e->dx < 0)
    e->dx += WORLD_FRICTION / WORLD_FPS;
  else if (e->dx > 0)
    e->dx -= WORLD_FRICTION / WORLD_FPS;
  
  // When dying, we can only decelerate, so it is safe to skip the bounds check for dx

  // Clamp X to within screen bounds
  if (e->x > ((SCREEN_TILES_H - 1) * (TILE_WIDTH << FP_SHIFT))) {
    e->x = ((SCREEN_TILES_H - 1) * (TILE_WIDTH << FP_SHIFT));
    e->dx = 0;
  } else if (e->x < 0) {
    e->x = 0;
    e->dx = 0;
  }

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->y += (e->dy / WORLD_FPS);

  // Repurpose the monsterhop flag to bounce up a bit when you die
  if (e->monsterhop) {
    e->monsterhop = false;
    e->dy += (WORLD_GRAVITY - (WORLD_JUMP >> 1)) / WORLD_FPS;
  } else {
    e->dy += WORLD_GRAVITY / WORLD_FPS;
  }

  if (e->dy < -WORLD_MAXDY)
    e->dy = -WORLD_MAXDY;
  else if (e->dy > WORLD_MAXDY)
    e->dy = WORLD_MAXDY;

  // Clamp Y to within screen bounds
  if (e->y > ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT))) {
    e->y = ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT));
    e->dy = 0;
    e->visible = false; // we hit the bottom of the screen, so now hide the entity
  } else if (e->y < 0) {
    e->y = 0;
    e->dy = 0;
  }
}

void entity_update_flying(ENTITY* const e)
{
  bool wasLeft = (e->dx < 0);
  bool wasRight = (e->dx > 0);

  int16_t ddx = 0;

  if (e->left)
    ddx -= WORLD_ACCEL;    // entity wants to go left
  else if (wasLeft)
    ddx += WORLD_FRICTION; // entity was going left, but not anymore

  if (e->right)
    ddx += WORLD_ACCEL;    // entity wants to go right
  else if (wasRight)
    ddx -= WORLD_FRICTION; // entity was going right, but not anymore

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

  // Clamp horizontal velocity to zero if we detect that the entities direction has changed
  if ((wasLeft && (e->dx > 0)) || (wasRight && (e->dx < 0)))
    e->dx = 0; // clamp at zero to prevent friction from making the entity jiggle side to side

  // Clamp X to within screen bounds
  if (e->x > ((SCREEN_TILES_H - 1) * (TILE_WIDTH << FP_SHIFT))) {
    e->x = ((SCREEN_TILES_H - 1) * (TILE_WIDTH << FP_SHIFT));
    e->dx = 0;
  } else if (e->x < 0) {
    e->x = 0;
    e->dx = 0;
  }


  bool wasUp = (e->dy < 0);
  bool wasDown = (e->dy > 0);

  int16_t ddy = 0;

  if (e->up)
    ddy -= WORLD_ACCEL;    // entity wants to go up
  else if (wasUp)
    ddy += WORLD_FRICTION; // entity was going up, but not anymore

  if (e->down)
    ddy += WORLD_ACCEL;    // entity wants to go down
  else if (wasDown)
    ddy -= WORLD_FRICTION; // entity was going down, but not anymore

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  e->y += (e->dy / WORLD_FPS);
  e->dy += (ddy / WORLD_FPS);
  if (e->turbo) {
    if (e->dy < -(e->maxdx + WORLD_METER)) // use e->maxdx as maxdy, since there is no gravity
      e->dy = -(e->maxdx + WORLD_METER);
    else if (e->dy > (e->maxdx + WORLD_METER))
      e->dy = (e->maxdx + WORLD_METER);
  } else {
    if (e->dy < -e->maxdx)
      e->dy = -e->maxdx;
    else if (e->dy > e->maxdx)
      e->dy = e->maxdx;
  }

  // Clamp vertical velocity to zero if we detect that the entities direction has changed
  if ((wasUp && (e->dy > 0)) || (wasDown && (e->dy < 0)))
    e->dy = 0; // clamp at zero to prevent friction from making the entity jiggle up and down

  // Clamp Y to within screen bounds
  if (e->y > ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT))) {
    e->y = ((SCREEN_TILES_V - 1) * (TILE_HEIGHT << FP_SHIFT));
    e->dy = 0;
  } else if (e->y < 0) {
    e->y = 0;
    e->dy = 0;
  }
}

void entity_update_ladder(ENTITY* const e)
{
  entity_update_flying(e);

  // Collision detection for ladders
  uint8_t tx = p2ht(e->x);
  uint8_t ty = p2vt(e->y);
  bool nx = (bool)nh(e->x); // true if entity overlaps right
  bool ny = (bool)nv(e->y); // true if entity overlaps below

  // Check to see if the entity has left the ladder
  uint16_t offset = ty * SCREEN_TILES_H + tx;
  if (!(             isLadder(vram[offset                     ] - RAM_TILES_COUNT) ||   // equiv. ... GetTile(tx, ty) ..
        (nx &&       isLadder(vram[offset + 1                 ] - RAM_TILES_COUNT)) ||  // equiv. ... GetTile(tx + 1, ty) ...
        (ny &&       isLadder(vram[offset + SCREEN_TILES_H    ] - RAM_TILES_COUNT)) ||  // equiv. ... GetTile(tx, ty + 1) ...
        (nx && ny && isLadder(vram[offset + SCREEN_TILES_H + 1] - RAM_TILES_COUNT)))) { // equiv. ... GetTile(tx + 1, ty + 1) ...
    e->update = entity_update;
    e->animationFrameCounter = 0;
    e->framesFalling = 0;   // reset the counter so a grace jump is allowed if moving off the ladder causes the entity to fall
    e->jumpReleased = true; // set this flag so attempting to jump off a ladder while moving up or down happens as soon as the entity leaves the ladder
  }

  // Allow jumping off ladder if the entity is not moving up or down
  if (e->jump && (!(e->up || e->down))) { // don't allow jumping if up or down is being held, because the entity would immediately rejoin the ladder
    e->jumping = e->falling = false; // ensure jump happens when calling entity_update
    e->jump = true;
    e->update = entity_update;
    e->animationFrameCounter = 0;
    entity_update(e); // run this inline, so the jump and proper collision can happen this frame
  }
}

#define GENERIC_OFFSET_DEAD (-3)
#define GENERIC_OFFSET_STATIONARY (-2)
#define GENERIC_OFFSET_JUMP (-1)
#define GENERIC_ANIMATION_FRAME_SKIP 4
const uint8_t genericAnimation[] PROGMEM = { 0, -2, 1, 2 };

static void generic_render(ENTITY* const e, const uint8_t animationStart)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = animationStart + GENERIC_OFFSET_DEAD;
  } else {
    if (e->jumping || e->falling || e->update == entity_update_flying) {
      if (e->dy >= 0)
        sprites[e->tag].tileIndex = animationStart + GENERIC_OFFSET_STATIONARY;
      else
        sprites[e->tag].tileIndex = animationStart + GENERIC_OFFSET_JUMP;
    } else {
      if (!e->left && !e->right) {
        sprites[e->tag].tileIndex = animationStart + GENERIC_OFFSET_STATIONARY;
      } else {
        for (uint8_t i = (e->turbo ? 2 : 1); i; --i) { // turbo makes animations faster
          if ((e->animationFrameCounter % GENERIC_ANIMATION_FRAME_SKIP) == 0)
            sprites[e->tag].tileIndex = animationStart + pgm_read_byte(&genericAnimation[e->animationFrameCounter / GENERIC_ANIMATION_FRAME_SKIP]);
          // Compile-time assert that we are working with a power of 2
          BUILD_BUG_ON(isNotPowerOf2(GENERIC_ANIMATION_FRAME_SKIP * NELEMS(genericAnimation)));
          e->animationFrameCounter = (e->animationFrameCounter + 1) & (GENERIC_ANIMATION_FRAME_SKIP * NELEMS(genericAnimation) - 1);
        }
      }
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  else if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  // Round x and y to the nearest whole pixel for rendering purposes only
  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define LADYBUG_ANIMATION_START 25

void ladybug_render(ENTITY* const e)
{
  generic_render(e, LADYBUG_ANIMATION_START);
}

#define ANT_ANIMATION_START 31

void ant_render(ENTITY* const e)
{
  generic_render(e, ANT_ANIMATION_START);
}

#define CRICKET_ANIMATION_START 37

void cricket_render(ENTITY* const e)
{
  generic_render(e, CRICKET_ANIMATION_START);
}

#define GRASSHOPPER_ANIMATION_START 43

void grasshopper_render(ENTITY* const e)
{
  generic_render(e, GRASSHOPPER_ANIMATION_START);
}

#define GENERIC_FLYING_OFFSET_DEAD (-1)
#define GENERIC_FLYING_ANIMATION_FRAME_SKIP 2
const uint8_t genericFlyingAnimation[] PROGMEM = { 0, 1, 2, 3, 2, 1 };

static void generic_flying_render(ENTITY* const e, const uint8_t animationStart)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = animationStart + GENERIC_FLYING_OFFSET_DEAD;
  } else {
    if ((e->animationFrameCounter % GENERIC_FLYING_ANIMATION_FRAME_SKIP) == 0)
      sprites[e->tag].tileIndex = animationStart + pgm_read_byte(&genericFlyingAnimation[e->animationFrameCounter / GENERIC_FLYING_ANIMATION_FRAME_SKIP]);
    if (++e->animationFrameCounter == GENERIC_FLYING_ANIMATION_FRAME_SKIP * NELEMS(genericFlyingAnimation))
      e->animationFrameCounter = 0;
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  else if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  // Round x and y to the nearest whole pixel for rendering purposes only
  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define FRUITFLY_ANIMATION_START 47

void fruitfly_render(ENTITY* const e)
{
  generic_flying_render(e, FRUITFLY_ANIMATION_START);
}

#define BEE_ANIMATION_START 52

void bee_render(ENTITY* const e)
{
  generic_flying_render(e, BEE_ANIMATION_START);
}

#define SPIDER_ANIMATION_START 57
#define SPIDER_DEAD (SPIDER_ANIMATION_START - 1)
#define SPIDER_ANIMATION_FRAME_SKIP 8
const uint8_t spiderAnimation[] PROGMEM = { 0, 1 };

void spider_render(ENTITY* const e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = SPIDER_DEAD;
  } else {
    for (uint8_t i = (e->turbo ? 2 : 1); i; --i) { // turbo makes animations faster
      if ((e->animationFrameCounter % SPIDER_ANIMATION_FRAME_SKIP) == 0)
        sprites[e->tag].tileIndex = SPIDER_ANIMATION_START + pgm_read_byte(&spiderAnimation[e->animationFrameCounter / SPIDER_ANIMATION_FRAME_SKIP]);
      // Compile-time assert that we are working with a power of 2
      BUILD_BUG_ON(isNotPowerOf2(SPIDER_ANIMATION_FRAME_SKIP * NELEMS(spiderAnimation)));
      e->animationFrameCounter = (e->animationFrameCounter + 1) & (SPIDER_ANIMATION_FRAME_SKIP * NELEMS(spiderAnimation) - 1);
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  // Round x and y to the nearest whole pixel for rendering purposes only
  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

// ---------- PLAYER

void player_init(PLAYER* const p, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), const uint8_t tag, const uint16_t x, const uint16_t y, const int16_t maxdx, const int16_t impulse)
{
  memset(&(p->buttons), 0, sizeof(BUTTON_INFO));
  entity_init((ENTITY*)p, input, update, render, tag, x, y, maxdx, impulse);
}

void player_input(ENTITY* const e)
{
  PLAYER* const p = (PLAYER*)e; // upcast

  // Read the current state of the player's controller
  p->buttons.prev = p->buttons.held;
  p->buttons.held = ReadJoypad(e->tag); // tag will be set to 0 or 1, depending on which player we are
  p->buttons.pressed = p->buttons.held & (p->buttons.held ^ p->buttons.prev);
  //p->buttons.released = p->buttons.prev & (p->buttons.held ^ p->buttons.prev);

  e->left = (bool)(p->buttons.held & BTN_LEFT);
  e->right = (bool)(p->buttons.held & BTN_RIGHT);
  e->up = (bool)(p->buttons.held & BTN_UP);
  e->down = (bool)(p->buttons.held & BTN_DOWN);
  e->turbo = (bool)(p->buttons.held & BTN_B);
  
  // Improve the user experience, by allowing players to jump by holding the jump button before landing, but require them to release it before they can jump again
  if (e->jumpReleased) {                                      // jumping multiple times requires releasing the jump button between jumps
    e->jump = (bool)(p->buttons.held & BTN_A);                // player[i].jump can only be true if BTN_A has been released from the previous jump

    if (e->jump && e->update == entity_update) {
      // Look at the tile(s) above the player's head. If the jump would not be allowed, then don't set jump to true until they can actually jump
      // This allows the player to hold the jump button early while jump-restricted, but still make the jump as soon as it is allowed
      uint8_t tx = p2ht(e->x);
      uint8_t ty = p2vt(e->y - 1);
      uint16_t offset = ty * SCREEN_TILES_H + tx;
      if (                   isSolid(vram[offset    ] - RAM_TILES_COUNT) || // cellup,     equiv. ... GetTile(tx,     ty) ...
          ((bool)nh(e->x) && isSolid(vram[offset + 1] - RAM_TILES_COUNT)))  // cellupdiag, equiv. ... GetTile(tx + 1, ty) ...
        e->jump = false;
    }

  } else {                                                    // otherwise, it means that we just jumped, and BTN_A is still being held down
    e->jump = false;                                          // explicitly disallow any additional jumps
    uint16_t released = p->buttons.prev & (p->buttons.held ^ p->buttons.prev);
    if (released & BTN_A)                                     // until BTN_A has been released
      e->jumpReleased = true;                                 // reset the jumpReleased flag so another jump may occur.
  }
}

#define PLAYER_ANIMATION_START 3
#define PLAYER_LADDER_ANIMATION_START 6
#define PLAYER_DEAD (PLAYER_ANIMATION_START - 3)
#define PLAYER_STATIONARY (PLAYER_ANIMATION_START - 2)
#define PLAYER_JUMP (PLAYER_ANIMATION_START - 1)
#define PLAYER_ANIMATION_FRAME_SKIP 4
#define PLAYER_LADDER_ANIMATION_FRAME_SKIP 4
#define PLAYER_NUM_SPRITES 11
const uint8_t playerAnimation[] PROGMEM = { 0, 1, 2, 1 };
const uint8_t playerLadderAnimation[] PROGMEM = { 0, 1, 2, 1, 0, 3, 4, 3 };

void player_render(ENTITY* const e)
{
  if (e->dead) {
    sprites[e->tag].tileIndex = PLAYER_DEAD + e->tag * PLAYER_NUM_SPRITES;
  } else if (e->update == entity_update_ladder) {
    if (e->up || e->down || e->left || e->right) {
      for (uint8_t i = (e->turbo ? 2 : 1); i; --i) { // turbo makes animations faster
        if ((e->animationFrameCounter % PLAYER_ANIMATION_FRAME_SKIP) == 0)
          sprites[e->tag].tileIndex = PLAYER_LADDER_ANIMATION_START + pgm_read_byte(&playerLadderAnimation[e->animationFrameCounter / PLAYER_LADDER_ANIMATION_FRAME_SKIP]) + e->tag * PLAYER_NUM_SPRITES;
        // Compile-time assert that we are working with a power of 2
        BUILD_BUG_ON(isNotPowerOf2(PLAYER_LADDER_ANIMATION_FRAME_SKIP * NELEMS(playerLadderAnimation)));
        e->animationFrameCounter = (e->animationFrameCounter + 1) & (PLAYER_LADDER_ANIMATION_FRAME_SKIP * NELEMS(playerLadderAnimation) - 1);
      }
    } else {
      sprites[e->tag].tileIndex = PLAYER_LADDER_ANIMATION_START + e->tag * PLAYER_NUM_SPRITES;
    }
  } else {
    if (e->jumping || e->falling || e->update == entity_update_flying) {
      if (e->dy >= 0)
        sprites[e->tag].tileIndex = PLAYER_STATIONARY + e->tag * PLAYER_NUM_SPRITES;
      else
        sprites[e->tag].tileIndex = PLAYER_JUMP + e->tag * PLAYER_NUM_SPRITES;
    } else {
      if (!e->left && !e->right) {
        sprites[e->tag].tileIndex = PLAYER_STATIONARY + e->tag * PLAYER_NUM_SPRITES;
      } else {
        for (uint8_t i = (e->turbo ? 2 : 1); i; --i) { // turbo makes animations faster
          if ((e->animationFrameCounter % PLAYER_ANIMATION_FRAME_SKIP) == 0)
            sprites[e->tag].tileIndex = PLAYER_ANIMATION_START + pgm_read_byte(&playerAnimation[e->animationFrameCounter / PLAYER_ANIMATION_FRAME_SKIP]) + e->tag * PLAYER_NUM_SPRITES;
          // Compile-time assert that we are working with a power of 2
          BUILD_BUG_ON(isNotPowerOf2(PLAYER_ANIMATION_FRAME_SKIP * NELEMS(playerAnimation)));
          e->animationFrameCounter = (e->animationFrameCounter + 1) & (PLAYER_ANIMATION_FRAME_SKIP * NELEMS(playerAnimation) - 1);
        }
      }
    }
  }

  if (e->left)
    sprites[e->tag].flags = 0;
  else if (e->right)
    sprites[e->tag].flags = SPRITE_FLIP_X;

  // Round x and y to the nearest whole pixel for rendering purposes only
  sprites[e->tag].x = (e->x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
  sprites[e->tag].y = (e->y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT;
}

#define EXIT_SIGN_START 59

void show_exit_sign(const uint8_t tx, const uint8_t ty)
{
  sprites[PLAYERS    ].tileIndex = EXIT_SIGN_START;
  sprites[PLAYERS + 1].tileIndex = EXIT_SIGN_START + 1;
  sprites[PLAYERS + 2].tileIndex = EXIT_SIGN_START + 2;
  sprites[PLAYERS + 3].tileIndex = EXIT_SIGN_START + 3;

  sprites[PLAYERS].flags = sprites[PLAYERS + 1].flags = sprites[PLAYERS + 2].flags = sprites[PLAYERS + 3].flags = 0;

  sprites[PLAYERS    ].x = sprites[PLAYERS + 2].x = (tx    ) * TILE_WIDTH;
  sprites[PLAYERS + 1].x = sprites[PLAYERS + 3].x = (tx + 1) * TILE_WIDTH;
  sprites[PLAYERS    ].y = sprites[PLAYERS + 1].y = (ty    ) * TILE_HEIGHT;
  sprites[PLAYERS + 2].y = sprites[PLAYERS + 3].y = (ty + 1) * TILE_HEIGHT;
}

void hide_exit_sign(void)
{
  for (uint8_t i = PLAYERS; i < PLAYERS + 4; ++i)
    sprites[i].x = OFF_SCREEN;
}
