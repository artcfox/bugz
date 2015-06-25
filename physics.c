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

#include <stdbool.h>
#include <avr/io.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <uzebox.h>
#include <string.h>

#include "data/level.inc"
#include "data/patches.inc"

#define PLAYERS 2

struct BUTTON_INFO {
  int held;
  int pressed;
  int released;
  int prev;
};

struct PLAYER_INFO {
  u16 x;
  u16 y;
  int16_t dx;
  int16_t dy;
  int16_t ddx;
  int16_t ddy;
  bool falling;
  bool jumping;

  bool left; // if the input was BTN_LEFT, then set this to true
  bool right; // if the input was BTN_RIGHT, then set this to true
  bool jump; // if the input was BTN_A, then set this to true

  // Enhancement to improve jumping experience
  bool jumpAllowed; // if BTN_A was released, then set this to true

  uint16_t framesFalling;

  // Debugging purposes
  bool clamped;

  /* u8 w; */
  /* u8 h; */
};

struct PLAYER_INFO player[PLAYERS];
struct BUTTON_INFO buttons[PLAYERS];

// Maps tile number to solidity
u8 isSolid[] = {0, 0, 1, 0, 1, 1};

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

#define vt2p(t) ((t) * (TILE_HEIGHT << FP_SHIFT))
#define ht2p(t) ((t) * (TILE_WIDTH << FP_SHIFT))
#define p2vt(p) ((p) / (TILE_HEIGHT << FP_SHIFT))
#define p2ht(p) ((p) / (TILE_WIDTH << FP_SHIFT))
#define nv(p) ((p) % (TILE_HEIGHT << FP_SHIFT))
#define nh(p) ((p) % (TILE_WIDTH << FP_SHIFT))

/* Calculate forces that apply to the player
   Apply the forces to move and accelerate the player
   Collision detection (and resolution) */
void update(u8 i)
{
  bool wasLeft = player[i].dx < 0;
  bool wasRight = player[i].dx > 0;
  bool falling = player[i].falling ? (player[i].framesFalling > WORLD_FALLING_GRACE_FRAMES) : false;

  player[i].ddx = 0;
  player[i].ddy = WORLD_GRAVITY;

  if (player[i].left)
    player[i].ddx -= WORLD_ACCEL; // player wants to go left
  else if (wasLeft)
    player[i].ddx += WORLD_FRICTION; // player was going left, but not anymore

  if (player[i].right)
    player[i].ddx += WORLD_ACCEL; // player wants to go right
  else if (wasRight)
    player[i].ddx -= WORLD_FRICTION; // player was going right, but not anymore

  if (player[i].jump && !player[i].jumping && !falling) {
    player[i].dy = 0;            // reset vertical velocity so jumps during grace period are consistent with jumps from ground
    player[i].ddy -= WORLD_JUMP; // apply an instantaneous (large) vertical impulse
    player[i].jumping = true;
  }

  // Variable height jumping
  if (player[i].jumping && (buttons[i].released & BTN_A) && (player[i].dy < -WORLD_CUT_JUMP_SPEED_LIMIT))
      player[i].dy = -WORLD_CUT_JUMP_SPEED_LIMIT;

  // Clamp horizontal velocity to zero if we detect that the players direction has changed
  if ((wasLeft && (player[i].dx > 0)) || (wasRight && (player[i].dx < 0))) {
    player[i].dx = 0; // clamp at zero to prevent friction from making us jiggle side to side
  }

  player[i].clamped = false;

  // Integrate the X forces to calculate the new position (x,y) and the new velocity (dx,dy)
  player[i].x += (player[i].dx / WORLD_FPS);
  player[i].dx += (player[i].ddx / WORLD_FPS);
  if (player[i].dx < -WORLD_MAXDX)
    player[i].dx = -WORLD_MAXDX;
  else if (player[i].dx > WORLD_MAXDX)
    player[i].dx = WORLD_MAXDX;

  // Collision Detection for X
  u8 tx = p2ht(player[i].x);
  u8 ty = p2vt(player[i].y);
  u8 nx = nh(player[i].x);  // true if player overlaps right
  u8 ny = nv(player[i].y); // true if player overlaps below
  u8 cell      = isSolid[GetTile(tx,     ty)];
  u8 cellright = isSolid[GetTile(tx + 1, ty)];
  u8 celldown  = isSolid[GetTile(tx,     ty + 1)];
  u8 celldiag  = isSolid[GetTile(tx + 1, ty + 1)];

  if (player[i].dx > 0) {
    if ((cellright && !cell) ||
        (celldiag  && !celldown && ny)) {
      player[i].clamped = true;
      player[i].x = ht2p(tx);     // clamp the x position to avoid moving into the platform we just hit
      player[i].dx = 0;           // stop horizontal velocity
      nx = 0;                     // player no longer overlaps the adjacent cell
      tx = p2ht(player[i].x);
      celldown  = isSolid[GetTile(tx,     ty + 1)];
      celldiag  = isSolid[GetTile(tx + 1, ty + 1)];
    }
  } else if (player[i].dx < 0) {
    if ((cell     && !cellright) ||
        (celldown && !celldiag && ny)) {
      player[i].clamped = true;
      player[i].x = ht2p(tx + 1); // clamp the x position to avoid moving into the platform we just hit
      player[i].dx = 0;           // stop horizontal velocity
      nx = 0;                     // player no longer overlaps the adjacent cell
      tx = p2ht(player[i].x);
      celldown  = isSolid[GetTile(tx,     ty + 1)];
      celldiag  = isSolid[GetTile(tx + 1, ty + 1)];
    }
  }

  // Integrate the Y forces to calculate the new position (x,y) and the new velocity (dx,dy)
  player[i].y += (player[i].dy / WORLD_FPS);
  player[i].dy += (player[i].ddy / WORLD_FPS);
  if (player[i].dy < -WORLD_MAXDY)
    player[i].dy = -WORLD_MAXDY;
  else if (player[i].dy > WORLD_MAXDY)
    player[i].dy = WORLD_MAXDY;

  // Collision Detection for Y
  tx = p2ht(player[i].x);
  ty = p2vt(player[i].y);
  nx = nh(player[i].x);  // true if player overlaps right
  ny = nv(player[i].y); // true if player overlaps below
  cell      = isSolid[GetTile(tx,     ty)];
  cellright = isSolid[GetTile(tx + 1, ty)];
  celldown  = isSolid[GetTile(tx,     ty + 1)];
  celldiag  = isSolid[GetTile(tx + 1, ty + 1)];

  if (player[i].dy > 0) {
    if ((celldown && !cell) ||
        (celldiag && !cellright && nx)) {
      player[i].clamped = true;
      player[i].y = vt2p(ty);     // clamp the y position to avoid falling into platform below
      player[i].dy = 0;           // stop downward velocity
      player[i].falling = false;  // no longer falling
      player[i].framesFalling = 0;
      player[i].jumping = false;  // (or jumping)
      ny = 0;                     // no longer overlaps the cells below
    }
  } else if (player[i].dy < 0) {
    if ((cell      && !celldown) ||
        (cellright && !celldiag && nx)) {
      player[i].clamped = true;
      player[i].y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      player[i].dy = 0;           // stop updard velocity
      ny = 0;                     // player no longer overlaps the cells below
    }
  }

  player[i].falling = !(celldown || (nx && celldiag)) && !player[i].jumping; // detect if we're now falling or not
  if (player[i].falling)
    player[i].framesFalling++;
}

int main()
{
  // Sets tile table to the specified tilesheet
  SetTileTable(tiles);

  SetSpritesTileBank(0, tiles);

  InitMusicPlayer(patches);

  for (u8 i = 0; i < PLAYERS; ++i) {
    /* player[i].w = PLAYER_START_WIDTH; */
    /* player[i].h = PLAYER_START_HEIGHT; */
    if (i == 0) {
      player[i].x = PLAYER_0_START_X;
      player[i].y = PLAYER_0_START_Y;
    } else if (i == 1) {
      player[i].x = PLAYER_1_START_X;
      player[i].y = PLAYER_1_START_Y;
    }

    player[i].jumpAllowed = true; // we haven't jumped yet, so this is true
    player[i].framesFalling = 0;

    MapSprite2(i, yellow_front, 0);
  }

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
  DrawMap(0, 0, level);

  //  struct BUTTON_INFO buttons[PLAYERS];
  memset(buttons, 0, sizeof(buttons));

  for (;;) {
    WaitVsync(1);

    // Read the current state of each controller
    for (u8 i = 0; i < PLAYERS; ++i) {
      buttons[i].prev = buttons[i].held;
      buttons[i].held = ReadJoypad(i);
      buttons[i].pressed = buttons[i].held & (buttons[i].held ^ buttons[i].prev);
      buttons[i].released = buttons[i].prev & (buttons[i].held ^ buttons[i].prev);

      player[i].left = (buttons[i].held & BTN_LEFT);
      player[i].right = (buttons[i].held & BTN_RIGHT);

      // Improve the user experience, by allowing players to jump by holding the jump
      // button before landing, but require them to release it before jumping again
      if (player[i].jumpAllowed) {                                      // Jumping multiple times requires releasing the jump button between jumps
        player[i].jump = (buttons[i].held & BTN_A);                      // player[i].jump can only be true if BTN_A has been released from the previous jump
        if (player[i].jump && !(player[i].jumping || (player[i].falling && player[i].framesFalling > WORLD_FALLING_GRACE_FRAMES))) { // if player[i] is currently holding BTN_A, (and is on the ground)
          player[i].jumpAllowed = false;                                // a jump will occur during the next call to update(), so clear the jumpAllowed flag.
          TriggerFx(0, 128, false);
        }
      } else {                                                           // Otherwise, it means that we just jumped, and BTN_A is still being held down
        player[i].jump = false;                                          // so explicitly disallow any additional jumps until
        if (buttons[i].released & BTN_A)                                 // BTN_A is released again
          player[i].jumpAllowed = true;                                 // at which point reset the jumpAllowed flag so another jump may occur.
      }

    }

    // Update the state of the players
    for (u8 i = 0; i < PLAYERS; ++i) {
      update(i);
    }
    
    // Render the world
    for (u8 i = 0; i < PLAYERS; ++i) {
      if (player[i].left == player[i].right) {
        if (player[i].clamped)
          MapSprite2(i, orange_front, 0);
        else
          MapSprite2(i, yellow_front, 0);
      } else {
        if (player[i].clamped)
          MapSprite2(i, orange_side, player[i].right ? SPRITE_FLIP_X : 0);
        else
          MapSprite2(i, yellow_side, player[i].right ? SPRITE_FLIP_X : 0);
      }
      MoveSprite(i, (player[i].x + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, (player[i].y + (1 << (FP_SHIFT - 1))) >> FP_SHIFT, 1, 1);
    }
  }


}
