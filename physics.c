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

*/

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <stdlib.h>

#include "entity.h"
#include "data/level.inc"

extern const char mysprites[] PROGMEM;
extern const struct PatchStruct patches[] PROGMEM;

const uint16_t monsterInitialX[] PROGMEM = {
  ((SCREEN_TILES_H - 4) * (TILE_WIDTH << FP_SHIFT) - (8 << FP_SHIFT)),
  ((TILE_WIDTH << FP_SHIFT) * 28),
  ((TILE_WIDTH << FP_SHIFT) * 9),
  ((TILE_WIDTH << FP_SHIFT) * 16),
  ((TILE_WIDTH << FP_SHIFT) * 19),
};

const uint16_t monsterInitialY[] PROGMEM = {
  ((SCREEN_TILES_V - 15) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
  ((SCREEN_TILES_V - 5) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
  ((SCREEN_TILES_V - 8) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
  ((SCREEN_TILES_V - 10) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
  ((SCREEN_TILES_V - 19) * (TILE_HEIGHT << FP_SHIFT) - (8 << FP_SHIFT)),
};

const uint8_t monsterInitialDirection[] PROGMEM = {
  1,
  0,
  0,
  0,
  0,
};

int main()
{
  PLAYER player[PLAYERS];
  ENTITY monster[MONSTERS];
  
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
      entity_init(&monster[i], monster_input, entity_update, monster_render, PLAYERS + i,
                  pgm_read_word(&monsterInitialX[i]),
                  pgm_read_word(&monsterInitialY[i]),
                  WORLD_METER * 2);
      if (pgm_read_byte(&monsterInitialDirection[i]))
        monster[i].right = true;
      else
        monster[i].left = true;
  }

  ClearVram();
  DrawMap(0, 0, level1);

  for (;;) {
    WaitVsync(1);

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
