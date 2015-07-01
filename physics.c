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
const uint8_t monsterInitialD[] PROGMEM = {  3,  0,  0,  0,  0 };

// How many treasures are in the level
#define TREASURE_COUNT 10
// How many frames to wait between 
#define TREASURE_FRAME_SKIP 15
// X coordinates of each treasure for this level
const uint8_t  treasureX[] PROGMEM = {  1,  7,  4, 12, 18,  6, 24, 27, 21, 28, };
// Y coordinates of each treasure for this level
const uint8_t  treasureY[] PROGMEM = { 26,  5,  8, 11, 17,  3,  4, 18,  7, 12, };
// Starting tile number of each treasure tile to render (animation frames follow directly after this number)
const uint8_t treasureFG[] PROGMEM = {  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4, };
// Background tile number that should be used to replace each treasure after collected
const uint8_t treasureBG[] PROGMEM = {  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3, };
// As the animation frame counter advances, this array will be indexed, and its value will be added to the
// treasureFG value for each treasure to compute the next tile number that the treasure will use
const uint8_t treasureAnimation[] PROGMEM = { 0, 1, 2, 3, 4, 3, 2, 1 };

bool detectKills(PLAYER* player, ENTITY* monster)
{
    // Check for player collisions with monsters
    for (uint8_t p = 0; p < PLAYERS; ++p) {
      if (((ENTITY*)(&player[p]))->enabled == true) {
        for (uint8_t i = 0; i < MONSTERS; ++i) {
          if (((ENTITY*)(&monster[i]))->enabled == true) {
            // The calculation below assumes each sprite is WORLD_METER wide, and uses a shrunken hitbox for the monster
            // If the player is moving down really fast, and an entity is moving up really fast, there is a slight chance
            // that it will kill you, because the entity might pass far enough through you that it's y position is above
            // you. To fix this, we might have to also check for collisions before updating the monsters positions (or cache
            // the previous x and y, and check that first)
            if (overlap(((ENTITY*)(&player[p]))->x,
                        ((ENTITY*)(&player[p]))->y,
                        WORLD_METER,
                        WORLD_METER,
                        ((ENTITY*)(&monster[i]))->x + (1 << FP_SHIFT),
                        ((ENTITY*)(&monster[i]))->y + (3 << FP_SHIFT),
                        WORLD_METER - (2 << FP_SHIFT),
                        WORLD_METER - (4 << FP_SHIFT))) {
              if ( (((ENTITY*)(&player[p]))->dy > 0) &&
                   ((((ENTITY*)(&monster[i]))->y + (3 << FP_SHIFT) - ((ENTITY*)(&player[p]))->y) > (WORLD_METER - (4 << FP_SHIFT)))) {
                TriggerFx(1, 128, false);                               // play the monster death sound
                ((ENTITY*)(&monster[i]))->enabled = false;              // make sure we don't consider the entity again for collisions
                ((ENTITY*)(&monster[i]))->input = null_input;           // disable the entity's ai
                ((ENTITY*)(&monster[i]))->update = entity_update_dying; // disable normal physics
                ((ENTITY*)(&player[p]))->monsterhop = true;             // player should now do the monster hop
                /* while (ReadJoypad(((ENTITY*)(&player[p]))->tag) != BTN_B) { */
                /*   // TODO: figure out how to get note to stop playing */
                /* } */
              } else {
                TriggerFx(3, 128, false);
                ((ENTITY*)(&player[p]))->enabled = false;
                ((ENTITY*)(&player[p]))->input = null_input;
                ((ENTITY*)(&player[p]))->update = null_update;
                while (ReadJoypad(((ENTITY*)(&player[p]))->tag) != BTN_START) {
                  // TODO: figure out how to get note to stop playing
                }
                return true;
              }
            }
          }
        }
      }
    }
    return false;
}

int main()
{
  PLAYER player[PLAYERS];
  ENTITY monster[MONSTERS];
  
  SetTileTable(level);
  SetSpritesTileBank(0, mysprites);
  InitMusicPlayer(patches);

  ClearVram();

  DrawMap(0, 0, level1);

 start:

  // Initialize players
  for (uint8_t i = 0; i < PLAYERS; ++i) {
    player_init(&player[i], player_input, player_update, player_render, i,
                (int16_t)(pgm_read_byte(&playerInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                (int16_t)(pgm_read_byte(&playerInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                WORLD_MAXDX /*WORLD_METER * 12*/,
                WORLD_JUMP_IMPULSE);
    ((ENTITY*)(&player[i]))->enabled = true;

  }

  /*
      // FAST MOVING SPIDER
      entity_init(&monster[i], ai_fly_vertical, entity_update_flying, spider_render, PLAYERS + i,
                  (int16_t)(pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                  WORLD_METER * 12,
                  (uint16_t)(23 << 8) | 16);

   */
  // Initialize monsters
  for (uint8_t i = 0; i < MONSTERS; ++i) {
    if (i == 1)
      entity_init(&monster[i], ai_hop_until_blocked, entity_update, cricket_render, PLAYERS + i,
                  (int16_t)(pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                  WORLD_METER * 1,
                  WORLD_JUMP_IMPULSE >> 1);
    else if (i == 0)
      entity_init(&monster[i], ai_fly_vertical, entity_update_flying, bee_render, PLAYERS + i,
                  (int16_t)(pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                  WORLD_METER * 2,
                  (uint16_t)(23 << 8) | 16);
    else if (i == 2)
      entity_init(&monster[i], ai_walk_until_blocked_or_ledge, entity_update, ladybug_render, PLAYERS + i,
                  (int16_t)(pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                  WORLD_METER * 3,
                  0);
    else if (i == 3)
      entity_init(&monster[i], ai_hop_until_blocked, entity_update, grasshopper_render, PLAYERS + i,
                  (int16_t)(pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                  WORLD_METER * 2,
                  WORLD_JUMP_IMPULSE);
    else
      entity_init(&monster[i], ai_walk_until_blocked, entity_update, ant_render, PLAYERS + i,
                  (int16_t)(pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                  WORLD_METER * 2,
                  0);

    uint8_t monsterDirection = pgm_read_byte(&monsterInitialD[i]);
    if (monsterDirection == 0)
      monster[i].left = true;
    else if (monsterDirection == 1)
      monster[i].right = true;
    else if (monsterDirection == 2)
      monster[i].up = true;
    else if (monsterDirection == 3)
      monster[i].down = true;

    monster[i].enabled = true;
  }

  bool treasureCollected[TREASURE_COUNT];

  // Initialize treasure
  for (uint8_t i = 0; i < TREASURE_COUNT; ++i) {
    SetTile(pgm_read_byte(&treasureX[i]), pgm_read_byte(&treasureY[i]), (uint16_t)pgm_read_byte(&treasureFG[i]));
    treasureCollected[i] = false;
  }

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

    // First detect any kills when only the player is moved
    if (detectKills(player, monster))
      goto start;

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

    // Now detect any kills when only the monster is moved
    if (detectKills(player, monster))
      goto start;

    // Animate treasure
    static uint8_t treasureFrameCounter = 0;
    for (uint8_t i = 0; i < TREASURE_COUNT; ++i) {
      if (treasureCollected[i]) {
        SetTile(pgm_read_byte(&treasureX[i]), pgm_read_byte(&treasureY[i]), (uint16_t)pgm_read_byte(&treasureBG[i]));
      } else {
        if (treasureFrameCounter % TREASURE_FRAME_SKIP == 0)
          SetTile(pgm_read_byte(&treasureX[i]), pgm_read_byte(&treasureY[i]),
                  (uint16_t)(pgm_read_byte(&treasureFG[i]) + pgm_read_byte(&treasureAnimation[treasureFrameCounter / TREASURE_FRAME_SKIP])));
      }
    }
    if (++treasureFrameCounter == TREASURE_FRAME_SKIP * NELEMS(treasureAnimation))
      treasureFrameCounter = 0;

    // Check for collisions with treasure
    for (uint8_t p = 0; p < PLAYERS; ++p) {
      if (((ENTITY*)(&player[p]))->enabled == true) {
        for (uint8_t i = 0; i < TREASURE_COUNT; ++i) {
          if (treasureCollected[i])
            continue;

          // The calculation below assumes each sprite is WORLD_METER wide
          if (overlap(((ENTITY*)(&player[p]))->x,
                      ((ENTITY*)(&player[p]))->y,
                      WORLD_METER,
                      WORLD_METER,
                      (uint16_t)pgm_read_byte(&treasureX[i]) * (TILE_WIDTH << FP_SHIFT),
                      (uint16_t)pgm_read_byte(&treasureY[i]) * (TILE_HEIGHT << FP_SHIFT),
                      WORLD_METER,
                      WORLD_METER)) {
              TriggerFx(2, 128, false);
              treasureCollected[i] = true;
          }
          
        }
      }
    }

    if (ReadJoypad(0) & BTN_START)
      goto start;
  }
}
