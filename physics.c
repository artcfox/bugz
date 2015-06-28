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

const uint8_t  playerInitialX[] PROGMEM = {  4, 25 };
const uint8_t  playerInitialY[] PROGMEM = { 26, 25 };
const uint8_t monsterInitialX[] PROGMEM = { 25, 28,  9, 16, 19 };
const uint8_t monsterInitialY[] PROGMEM = { 12, 22, 19, 17,  8 };
const uint8_t monsterInitialD[] PROGMEM = {  0,  0,  0,  0,  0 };

int main()
{
  PLAYER player[PLAYERS];
  ENTITY monster[MONSTERS];
  
  SetTileTable(level);
  SetSpritesTileBank(0, mysprites);
  InitMusicPlayer(patches);

  // Initialize players
  for (uint8_t i = 0; i < PLAYERS; ++i) {
    player_init(&player[i], player_input, player_update, player_render, i,
                pgm_read_byte(&playerInitialX[i]) * (TILE_WIDTH << FP_SHIFT),
                pgm_read_byte(&playerInitialY[i]) * (TILE_HEIGHT << FP_SHIFT),
                WORLD_MAXDX,
                WORLD_JUMP_IMPULSE);
  }

  // Initialize monsters
  for (uint8_t i = 0; i < MONSTERS; ++i) {
    if (i == 1)
      entity_init(&monster[i], ai_hop_until_blocked, entity_update, cricket_render, PLAYERS + i,
                  pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT),
                  pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT),
                  WORLD_METER * 1,
                  WORLD_JUMP_IMPULSE >> 1);
    else if (i == 0)
      entity_init(&monster[i], ai_hop_until_blocked, entity_update, grasshopper_render, PLAYERS + i,
                  pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT),
                  pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT),
                  WORLD_METER * 2,
                  WORLD_JUMP_IMPULSE);
    else if (i == 2)
      entity_init(&monster[i], ai_walk_until_blocked_or_ledge, entity_update, ladybug_render, PLAYERS + i,
                  pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT),
                  pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT),
                  WORLD_METER * 1,
                  0);
    else
      entity_init(&monster[i], ai_walk_until_blocked, entity_update, ant_render, PLAYERS + i,
                  pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT),
                  pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT),
                  WORLD_METER * 2,
                  0);
      if (pgm_read_byte(&monsterInitialD[i]))
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
