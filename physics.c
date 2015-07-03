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

const uint8_t  playerInitialX[] PROGMEM = {  1, 2 };
const uint8_t  playerInitialY[] PROGMEM = { 25, 25 };
const uint8_t monsterInitialX[] PROGMEM = { 25, 28,  9, 16, 19,  7,  9, 23, 45, 9 };
const uint8_t monsterInitialY[] PROGMEM = { 12, 22, 19, 17,  8, 25, 25, 5, 6, 7 };
const uint8_t monsterInitialD[] PROGMEM = {  3,  0,  0,  0,  0,  1,  0, 0, 1, 0 };

// How many treasures are in the level
#define TREASURE_COUNT 1
// How many frames to wait between 
#define TREASURE_FRAME_SKIP 15
// X coordinates of each treasure for this level
const uint8_t  treasureX[] PROGMEM = {  1,  7,  4, 12, 18,  6, 24, 27, 21, 28, };
// Y coordinates of each treasure for this level
const uint8_t  treasureY[] PROGMEM = { 26,  5,  8, 11, 17,  3,  4, 18,  7, 12, };
// Starting tile number of each treasure tile to render (animation frames follow directly after this number)
const uint8_t treasureFG[] PROGMEM = { 6,  6,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, };
// Background tile number that should be used to replace each treasure after collected
const uint8_t treasureBG[] PROGMEM = { 14,  14,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11, };
// As the animation frame counter advances, this array will be indexed, and its value will be added to the
// treasureFG value for each treasure to compute the next tile number that the treasure will use
const uint8_t treasureAnimation[] PROGMEM = { 0, 1, 2, 3, 4, 3, 2, 1 };

#define BitArray_numBits(bits) (((bits) >> 3) + 1 * (((bits) & 7) ? 1 : 0))
#define BitArray_setBit(array, index) ((array)[(index) >> 3] |= (1 << ((index) & 7)))
#define BitArray_clearBit(array, index) ((array)[(index) >> 3] &= ((1 << ((index) & 7)) ^ 0xFF))
#define BitArray_readBit(array, index) ((bool)((array)[(index) >> 3] & (1 << ((index) & 7))))

void killPlayer(PLAYER* p)
{
  ENTITY* e = (ENTITY*)p;
  TriggerFx(3, 128, false);
  e->dead = true;
  e->up = true;                   // player dies upwards
  e->dy = 0;
  e->enabled = false;
  e->input = null_input;
  e->update = entity_update_dying;
  /* e->render(e); // since we just died, call the render function */
  /* while (ReadJoypad(e->tag) != BTN_START) { */
  /*   // TODO: figure out how to get note to stop playing */
  /* } */
}

void killMonster(ENTITY* e)
{
  TriggerFx(1, 128, false);        // play the monster death sound
  e->dead = true;                  // kill the monster
  e->up = false;                   // die downwards
  e->enabled = false;              // make sure we don't consider the entity again for collisions
  e->input = null_input;           // disable the entity's ai
  e->update = entity_update_dying; // disable normal physics
}

bool detectKills(PLAYER* players, ENTITY* monsters)
{
    // Check for player collisions with monsters
    for (uint8_t p = 0; p < PLAYERS; ++p) {
      if (((ENTITY*)(&players[p]))->enabled) {
        // Check if the dead flag has been set for a player
        if (((ENTITY*)(&players[p]))->dead) {
          killPlayer(&players[p]);
          //return true;
        }

        for (uint8_t i = 0; i < MONSTERS; ++i) {
          if (monsters[i].enabled) {

            // Check if the dead flag has been set for a monster
            if (monsters[i].dead) {
              killMonster(&monsters[i]);
            }

            // The calculation below assumes each sprite is WORLD_METER wide, and uses a shrunken hitbox for the monster
            // If the player is moving down really fast, and an entity is moving up really fast, there is a slight chance
            // that it will kill you, because the entity might pass far enough through you that it's y position is above
            // you. To fix this, we might have to also check for collisions before updating the monsters positions (or cache
            // the previous x and y, and check that first)
            if (overlap(((ENTITY*)(&players[p]))->x,
                        ((ENTITY*)(&players[p]))->y,
                        WORLD_METER,
                        WORLD_METER,
                        monsters[i].x + (1 << FP_SHIFT),
                        monsters[i].y + (3 << FP_SHIFT),
                        WORLD_METER - (2 << FP_SHIFT),
                        WORLD_METER - (4 << FP_SHIFT))) {
              if ( /*(((ENTITY*)(&players[p]))->dy > 0) && */
                   ((monsters[i].y + (3 << FP_SHIFT) - ((ENTITY*)(&players[p]))->y) > (WORLD_METER - (4 << FP_SHIFT)))) {
                killMonster(&monsters[i]);
                ((ENTITY*)(&players[p]))->monsterhop = true;             // player should now do the monster hop
                /* while (ReadJoypad(((ENTITY*)(&players[p]))->tag) != BTN_B) { */
                /*   // TODO: figure out how to get note to stop playing */
                /* } */
              } else {
                killPlayer(&players[p]);
                //return true;
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
                WORLD_MAXDX/*WORLD_METER * 12*/,
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
      entity_init(&monster[i], ai_fly_vertical, entity_update_flying, spider_render, PLAYERS + i,
                  (int16_t)(pgm_read_byte(&monsterInitialX[i]) * (TILE_WIDTH << FP_SHIFT)),
                  (int16_t)(pgm_read_byte(&monsterInitialY[i]) * (TILE_HEIGHT << FP_SHIFT)),
                  WORLD_METER * 12,
                  (uint16_t)((23) << 8) | 16);
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
                  WORLD_METER * 1,
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

  //bool treasureCollected[TREASURE_COUNT];
  uint8_t treasureCollected[BitArray_numBits(TREASURE_COUNT)] = {0}; // bit array

  // Initialize treasure
  for (uint8_t i = 0; i < TREASURE_COUNT; ++i) {
    SetTile(pgm_read_byte(&treasureX[i]), pgm_read_byte(&treasureY[i]), (uint16_t)pgm_read_byte(&treasureFG[i]));
  }

  for (;;) {
    WaitVsync(1);

    // Get the inputs for every entity
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->input((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      monster[i].input(&monster[i]);
    }
    
    // Update the state of every entity
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->update((ENTITY*)(&player[i]));
    }

    // First detect any kills when only the player is moved
    if (detectKills(player, monster))
      goto start;

    for (uint8_t i = 0; i < MONSTERS; ++i) {
      monster[i].update(&monster[i]);
    }
    
    // Render every entity
    for (uint8_t i = 0; i < PLAYERS; ++i) {
      ((ENTITY*)(&player[i]))->render((ENTITY*)(&player[i]));
    }
    for (uint8_t i = 0; i < MONSTERS; ++i) {
      monster[i].render(&monster[i]);
    }

    // Now detect any kills when only the monster is moved
    if (detectKills(player, monster))
      goto start;

    // Animate treasure
    static uint8_t treasureFrameCounter = 0;
    for (uint8_t i = 0; i < TREASURE_COUNT; ++i) {
      if (BitArray_readBit(treasureCollected, i)) {
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
          if (BitArray_readBit(treasureCollected, i))
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
              BitArray_setBit(treasureCollected, i);
          }
          
        }
      }
    }

    if (ReadJoypad(0) & BTN_START)
      goto start;
  }
}
