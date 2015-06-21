#include <stdbool.h>
#include <avr/io.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <uzebox.h>

#include "data/level.inc"

#define NELEMS(x) (sizeof(x)/sizeof(x[0]))

#define PLAYERS 2

struct BUTTON_INFO {
  int held;
  int pressed;
  int released;
  int prev;
};

struct PLAYER_INFO {
  u8 x;
  u8 y;
  int16_t dx;
  int16_t dy;
  int16_t ddx;
  int16_t ddy;
  bool falling;
  bool jumping;

  bool left; // if the input was BTN_LEFT, then set this to true
  bool right; // if the input was BTN_RIGHT, then set this to true
  bool jump; // if the input was BTN_A, then set this to true

  /* u8 w; */
  /* u8 h; */
};

struct PLAYER_INFO player[PLAYERS];

u8 isSolid[] = {0, 0, 1, 0};

#define PLAYER_START_WIDTH  8
#define PLAYER_START_HEIGHT 8
#define PLAYER_0_START_X    (4 * TILE_WIDTH)
#define PLAYER_0_START_Y    ((SCREEN_TILES_V - 1) * TILE_HEIGHT  - PLAYER_START_HEIGHT)
#define PLAYER_1_START_X    ((SCREEN_TILES_H - 4) * TILE_WIDTH - PLAYER_START_WIDTH)
#define PLAYER_1_START_Y    ((SCREEN_TILES_V - 1) * TILE_HEIGHT  - PLAYER_START_HEIGHT)

// 1/30th of a second per frame
#define WORLD_FPS 30
// arbitrary choice for 1m
#define WORLD_METER 8
// very exagerated gravity (6x)
#define WORLD_GRAVITY (WORLD_METER * 9.8 * 4)
// max horizontal speed (20 tiles per second)
#define WORLD_MAXDX (WORLD_METER * 6)
// max vertical speed (60 tiles per second)
#define WORLD_MAXDY (WORLD_METER * 20)
// horizontal acceleration - take 1/2 second to reach maxdx
#define WORLD_ACCEL (WORLD_MAXDX * 8)
// horizontal friction - take 1/6 second to stop from maxdx
#define WORLD_FRICTION (WORLD_MAXDX * 6)
// (a large) instantaneous jump impulse
#define WORLD_JUMP (WORLD_METER * 600)

#define vt2p(t) ((t) * TILE_HEIGHT)
#define ht2p(t) ((t) * TILE_WIDTH)
#define p2vt(p) ((p) / TILE_HEIGHT)
#define p2ht(p) ((p) / TILE_WIDTH)

/* Calculate forces that apply to the player
   Apply the forces to move and accelerate the player
   Collision detection (and resolution) */
void update(u8 i)
{
  bool wasLeft = player[i].dx < 0;
  bool wasRight = player[i].dx > 0;
  bool falling = player[i].falling;

  player[i].ddx = 0;
  player[i].ddy = WORLD_GRAVITY;

  if (player[i].left)
    player[i].ddx = player[i].ddx - WORLD_ACCEL; // player wants to go left
  else if (wasLeft)
    player[i].ddx = player[i].ddx + WORLD_FRICTION; // player was going left, but not anymore

  if (player[i].right)
    player[i].ddx = player[i].ddx + WORLD_ACCEL; // player wants to go right
  else if (wasRight)
    player[i].ddx = player[i].ddx - WORLD_FRICTION; // player was going right, but not anymore

  if (player[i].jump && !player[i].jumping && !falling) {
    player[i].ddy = player[i].ddy - WORLD_JUMP; // apply an instantaneous (large) vertical impulse
    player[i].jumping = true;
  }

  // Integrate the forces to calculate the new position (x,y) and velocity (dx,dy)
  player[i].y = player[i].y + (player[i].dy / WORLD_FPS);
  player[i].x = player[i].x + (player[i].dx / WORLD_FPS);

  player[i].dx = player[i].dx + (player[i].ddx / WORLD_FPS);
  if (player[i].dx < -WORLD_MAXDX)
    player[i].dx = -WORLD_MAXDX;
  else if (player[i].dx > WORLD_MAXDX)
    player[i].dx = WORLD_MAXDX;

  player[i].dy = player[i].dy + (player[i].ddy / WORLD_FPS);
  if (player[i].dy < -WORLD_MAXDY)
    player[i].dy = -WORLD_MAXDY;
  else if (player[i].dy > WORLD_MAXDY)
    player[i].dy = WORLD_MAXDY;

  // Clamp horizontal velocity to zero if we detect that the players direction has changed
  if ((wasLeft && (player[i].dx > 0)) || (wasRight && (player[i].dx < 0))) {
    player[i].dx = 0; // clamp at zero to prevent friction from maxing us jiggle side to side
  }

  // Collision Detection
  u8 tx = p2ht(player[i].x);
  u8 ty = p2vt(player[i].y);
  u8 nx = player[i].x % TILE_WIDTH;  // true if player overlaps right
  u8 ny = player[i].y % TILE_HEIGHT; // true if player overlaps below
  u8 cell = isSolid[GetTile(tx, ty)];
  u8 cellright = isSolid[GetTile(tx + 1, ty)];
  u8 celldown = isSolid[GetTile(tx, ty + 1)];
  u8 celldiag = isSolid[GetTile(tx + 1, ty + 1)];

  if (player[i].dy > 0) {
    if ((celldown && !cell) ||
        (celldiag && !cellright && nx)) {
      player[i].y = vt2p(ty);     // clamp the y position to avoid falling into platform below
      player[i].dy = 0;           // stop downward velocity
      player[i].falling = false;  // no longer falling
      player[i].jumping = false;  // (or jumping)
      ny = 0;                     // no longer overlaps the cells below
    }
  } else if (player[i].dy < 0) {
    if ((cell      && !celldown) ||
        (cellright && !celldiag && nx)) {
      player[i].y = vt2p(ty + 1); // clamp the y position to avoid jumping into platform above
      player[i].dy = 0;           // stop updard velocity
      cell = celldown;            // player is no longer really in that cell, we clamped them to the cell below
      cellright = celldiag;       // (ditto)
      ny = 0;                     // player no longer overlaps the cells below
    }
  }

  if (player[i].dx > 0) {
    if ((cellright && !cell) ||
        (celldiag  && !celldown && ny)) {
      player[i].x = ht2p(tx);     // clamp the x position to avoid moving into the platform we just hit
      player[i].dx = 0;           // stop horizontal velocity
    }
  } else if (player[i].dx < 0) {
    if ((cell     && !cellright) ||
        (celldown && !celldiag && ny)) {
      player[i].x = ht2p(tx + 1); // clamp the x position to avoid moving into the platform we just hit
      player[i].dx = 0;           // stop horizontal velocity
    }
  }

  player[i].falling = !(celldown || (nx && celldiag));
}

int main()
{
  // Sets tile table to the specified tilesheet
  SetTileTable(tiles);

  SetSpritesTileBank(0, tiles);

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

    MapSprite2(i, yellow_front, 0);
  }

  // Fills the video RAM with the first tile (0, 0)
  ClearVram();

  /* // Draw a solid border around the edges of the screen */
  /* for (u8 i = 0; i < SCREEN_TILES_H; i++) { */
  /*   for (u8 j = 0; j < SCREEN_TILES_V; j++) { */
  /*     if (i == 0 || i == SCREEN_TILES_H - 1) { */
  /*       SetTile(i, j, 1); */
  /*       continue; */
  /*     } */
  /*     if (j == 0 || j == SCREEN_TILES_V - 1) { */
  /*       SetTile(i, j, 1); */
  /*       continue; */
  /*     } */
  /*     SetTile(i, j, 2); */
  /*   } */
  /* } */
  DrawMap(0, 0, level);

  struct BUTTON_INFO buttons[PLAYERS] = {0};

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
      player[i].jump = (buttons[i].pressed & BTN_A);
    }

    for (u8 i = 0; i < PLAYERS; ++i) {
      update(i);
    }
      
    for (u8 i = 0; i < PLAYERS; ++i) {
      if (player[i].left == player[i].right)
        MapSprite2(i, yellow_front, 0);
      else
        MapSprite2(i, yellow_side, player[i].right ? SPRITE_FLIP_X : 0);

      MoveSprite(i, player[i].x, player[i].y, 1, 1);
    }
  }


}
