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

#include "entity.h"

#include "data/level.inc"

extern const char mysprites[] PROGMEM;
extern const struct PatchStruct patches[] PROGMEM;

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


int main()
{
  PLAYER player[PLAYERS];
  ENTITY monster[MONSTERS];
  //uint8_t* entities[6] = {0};
  
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
  }

  // XXX: BROKEN - Initialize entities array
  /* if (PLAYERS > 0) */
  /*   entities[0] = (uint8_t*)(&player[0]); */
  /* if (PLAYERS > 1) */
  /*   entities[1] = (uint8_t*)(&player[1]); */

  /* for (uint8_t i = PLAYERS; i < PLAYERS + MONSTERS; ++i) { */
  /*   entities[i] = (uint8_t*)(&monster[i - PLAYERS]); */
  /* } */

  // Fills the video RAM with the first tile (0, 0)
  ClearVram();

  /* // Draw a solid border around the edges of the screen */
  /* for (uint8_t i = 0; i < SCREEN_TILES_H; i++) { */
  /*   for (uint8_t j = 0; j < SCREEN_TILES_V; j++) { */
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

  for (;;) {
    WaitVsync(1);

    /* // XXX: BROKEN - Read the current state of each controller */
    /* for (uint8_t i = 0; i < PLAYERS + MONSTERS; ++i) { */
    /*   if (entities[i]) */
    /*     ((ENTITY*)(&entities + i))->input((ENTITY*)(&entities + i)); */
    /* } */

    /* // Update the state of the players */
    /* for (uint8_t i = 0; i < PLAYERS + MONSTERS; ++i) { */
    /*   if (entities[i]) */
    /*     ((ENTITY*)(&entities + i))->update((ENTITY*)(&entities + i)); */
    /* } */
    
    /* // Render the world */
    /* for (uint8_t i = 0; i < PLAYERS + MONSTERS; ++i) { */
    /*   if (entities[i]) */
    /*     ((ENTITY*)(&entities + i))->render((ENTITY*)(&entities + i)); */
    /* } */


    // Get the inputs for every entity
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->input((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      ((ENTITY*)(&monster[i]))->input((ENTITY*)(&monster[i]));
    }
    
    // Update the state of every entity
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->update((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      ((ENTITY*)(&monster[i]))->update((ENTITY*)(&monster[i]));
    }
    
    // Render every entity
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->render((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      ((ENTITY*)(&monster[i]))->render((ENTITY*)(&monster[i]));
    }
  }


}
