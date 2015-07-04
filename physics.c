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
#include "data/tileset.inc"

extern const char mysprites[] PROGMEM;
extern const struct PatchStruct patches[] PROGMEM;

#define BitArray_numBits(bits) (((bits) >> 3) + 1 * (((bits) & 7) ? 1 : 0))
#define BitArray_setBit(array, index) ((array)[(index) >> 3] |= (1 << ((index) & 7)))
#define BitArray_clearBit(array, index) ((array)[(index) >> 3] &= ((1 << ((index) & 7)) ^ 0xFF))
#define BitArray_readBit(array, index) ((bool)((array)[(index) >> 3] & (1 << ((index) & 7))))

#define LO8(x) ((uint8_t)((x) & 0xFF))
#define HI8(x) ((uint8_t)(((x) >> 8) & 0xFF))

enum INPUT_FUNCTION;
typedef enum INPUT_FUNCTION INPUT_FUNCTION;

enum INPUT_FUNCTION {
  INPUT_NULL_INPUT = 0,
  INPUT_PLAYER_INPUT = 1,
  INPUT_AI_WALK_UNTIL_BLOCKED = 2,
  INPUT_AI_HOP_UNTIL_BLOCKED = 3,
  INPUT_AI_WALK_UNTIL_BLOCKED_OR_LEDGE = 4,
  INPUT_AI_HOP_UNTIL_BLOCKED_OR_LEDGE = 5,
  INPUT_AI_FLY_VERTICAL = 6,
  INPUT_AI_FLY_HORIZONTAL = 7,
};

typedef void (*inputFnPtr)(ENTITY*);
inputFnPtr inputFunc(INPUT_FUNCTION i)
{
  switch (i) {
  case INPUT_PLAYER_INPUT:
    return player_input;
  case INPUT_AI_WALK_UNTIL_BLOCKED:
    return ai_walk_until_blocked;
  case INPUT_AI_HOP_UNTIL_BLOCKED:
    return ai_hop_until_blocked;
  case INPUT_AI_WALK_UNTIL_BLOCKED_OR_LEDGE:
    return ai_walk_until_blocked_or_ledge;
  case INPUT_AI_HOP_UNTIL_BLOCKED_OR_LEDGE:
    return ai_hop_until_blocked_or_ledge;
  case INPUT_AI_FLY_VERTICAL:
    return ai_fly_vertical;
  case INPUT_AI_FLY_HORIZONTAL:
    return ai_fly_horizontal;
  default: // INPUT_NULL_INPUT
    return null_input;
  }
}

enum UPDATE_FUNCTION;
typedef enum UPDATE_FUNCTION UPDATE_FUNCTION;

enum UPDATE_FUNCTION {
  UPDATE_NULL_UPDATE = 0,
  UPDATE_PLAYER_UPDATE = 1,
  UPDATE_ENTITY_UPDATE = 2,
  UPDATE_ENTITY_UPDATE_FLYING = 3,
};

typedef void (*updateFnPtr)(ENTITY*);
updateFnPtr updateFunc(UPDATE_FUNCTION u)
{
  switch (u) {
  case UPDATE_PLAYER_UPDATE:
    return player_update;
  case UPDATE_ENTITY_UPDATE:
    return entity_update;
  case UPDATE_ENTITY_UPDATE_FLYING:
    return entity_update_flying;
  default: // UPDATE_NULL_UPDATE
    return null_update;
  }
}

enum RENDER_FUNCTION;
typedef enum RENDER_FUNCTION RENDER_FUNCTION;

enum RENDER_FUNCTION {
  RENDER_NULL_RENDER = 0,
  RENDER_PLAYER_RENDER = 1,
  RENDER_LADYBUG_RENDER = 2,
  RENDER_ANT_RENDER = 3,
  RENDER_CRICKET_RENDER = 4,
  RENDER_GRASSHOPPER_RENDER = 5,
  RENDER_FRUITFLY_RENDER = 6,
  RENDER_BEE_RENDER = 7,
  RENDER_SPIDER_RENDER = 8,
};

typedef void (*renderFnPtr)(ENTITY*);
renderFnPtr renderFunc(RENDER_FUNCTION r)
{
  switch (r) {
  case RENDER_PLAYER_RENDER:
    return player_render;
  case RENDER_LADYBUG_RENDER:
    return ladybug_render;
  case RENDER_ANT_RENDER:
    return ant_render;
  case RENDER_CRICKET_RENDER:
    return cricket_render;
  case RENDER_GRASSHOPPER_RENDER:
    return grasshopper_render;
  case RENDER_FRUITFLY_RENDER:
    return fruitfly_render;
  case RENDER_BEE_RENDER:
    return bee_render;
  case RENDER_SPIDER_RENDER:
    return spider_render;
  default: // RENDER_NULL_RENDER:
    return null_render;
  }
}

const uint8_t levelData[] PROGMEM = {
  1,      // uint8_t numLevels
  3, 0,   // uint16_t levelOffsets[numLevels] = offsets to each level (little endian)

          // ---------- start of level 0 data
  0,      // uint8_t tileSet;
  255, 255, 255, 254, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 96, 0, 0, 1, 128, 0, 0, 6, 0, 0, 0, 24, 0, 0, 0, 127, 255, 255, 255, // uint8_t map[105];
  1, 2,   // uint8_t playerInitialX[2]
  26, 26, // uint8_t playerInitialY[2]
  25, 28,  9, 16, 19,  7,      // uint8_t monsterInitialX[6]
  12, 22, 19, 17,  8, 25,      // uint8_t monsterInitialY[6]
   3,  0,  0,  0,  0,  1,      // uint8_t monsterInitialD[6]
  12, // uint8_t treasureCount
  1,  7,  4, 12, 18,  6, 24, 27, 21, 28, 13, 18, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // uint8_t treasureX[32]
  24, 5,  8, 11, 17,  3,  4, 18,  7, 12, 22, 23, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // uint8_t treasureY[32]
  LO8(WORLD_MAXDX), HI8(WORLD_MAXDX), LO8(WORLD_MAXDX), HI8(WORLD_MAXDX),                                // int16_t playerMaxDX[2]
  LO8(WORLD_JUMP_IMPULSE), HI8(WORLD_JUMP_IMPULSE), LO8(WORLD_JUMP_IMPULSE), HI8(WORLD_JUMP_IMPULSE),    // int16_t playerImpulse[2]
  INPUT_PLAYER_INPUT, INPUT_PLAYER_INPUT,       // INPUT_FUNCTIONS playerInputFuncs[2]
  UPDATE_PLAYER_UPDATE, UPDATE_PLAYER_UPDATE,   // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  RENDER_PLAYER_RENDER, RENDER_PLAYER_RENDER,   // RENDER_FUNCTIONS playerRenderFuncs[2]
  LO8(WORLD_METER * 12), HI8(WORLD_METER * 12), LO8(WORLD_METER * 1), HI8(WORLD_METER * 1), LO8(WORLD_METER * 3), HI8(WORLD_METER * 3), LO8(WORLD_METER * 2), HI8(WORLD_METER * 2), LO8(WORLD_METER * 1), HI8(WORLD_METER * 1), LO8(WORLD_METER * 1), HI8(WORLD_METER * 1),                                // int16_t monsterMaxDX[6]
  16, 23, LO8(WORLD_JUMP_IMPULSE >> 1), HI8(WORLD_JUMP_IMPULSE >> 1), LO8(WORLD_JUMP_IMPULSE), HI8(WORLD_JUMP_IMPULSE), LO8(WORLD_JUMP_IMPULSE), HI8(WORLD_JUMP_IMPULSE), LO8(WORLD_JUMP_IMPULSE), HI8(WORLD_JUMP_IMPULSE), LO8(WORLD_JUMP_IMPULSE), HI8(WORLD_JUMP_IMPULSE),    // int16_t monsterImpulse[6]
  INPUT_AI_FLY_VERTICAL, INPUT_AI_HOP_UNTIL_BLOCKED, INPUT_AI_WALK_UNTIL_BLOCKED_OR_LEDGE, INPUT_AI_HOP_UNTIL_BLOCKED, INPUT_AI_WALK_UNTIL_BLOCKED, INPUT_AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  UPDATE_ENTITY_UPDATE_FLYING, UPDATE_ENTITY_UPDATE, UPDATE_ENTITY_UPDATE, UPDATE_ENTITY_UPDATE, UPDATE_ENTITY_UPDATE, UPDATE_ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  RENDER_SPIDER_RENDER, RENDER_CRICKET_RENDER, RENDER_LADYBUG_RENDER, RENDER_GRASSHOPPER_RENDER, RENDER_ANT_RENDER, RENDER_ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]

  
 };

#define LEVEL_HEADER_SIZE 1

#define LEVEL_TILESET_START 0
#define LEVEL_TILESET_SIZE 1

#define LEVEL_MAP_START (LEVEL_TILESET_START + LEVEL_TILESET_SIZE)
#define LEVEL_MAP_SIZE 105

#define LEVEL_PLAYER_INITIAL_X_START (LEVEL_MAP_START + LEVEL_MAP_SIZE)
#define LEVEL_PLAYER_INITIAL_X_SIZE 2

#define LEVEL_PLAYER_INITIAL_Y_START (LEVEL_PLAYER_INITIAL_X_START + LEVEL_PLAYER_INITIAL_X_SIZE)
#define LEVEL_PLAYER_INITIAL_Y_SIZE 2

#define LEVEL_MONSTER_INITIAL_X_START (LEVEL_PLAYER_INITIAL_Y_START + LEVEL_PLAYER_INITIAL_Y_SIZE)
#define LEVEL_MONSTER_INITIAL_X_SIZE 6

#define LEVEL_MONSTER_INITIAL_Y_START (LEVEL_MONSTER_INITIAL_X_START + LEVEL_MONSTER_INITIAL_X_SIZE)
#define LEVEL_MONSTER_INITIAL_Y_SIZE 6

#define LEVEL_MONSTER_INITIAL_D_START (LEVEL_MONSTER_INITIAL_Y_START + LEVEL_MONSTER_INITIAL_Y_SIZE)
#define LEVEL_MONSTER_INITIAL_D_SIZE 6

// TREASURE STUFF
#define LEVEL_TREASURE_COUNT_START (LEVEL_MONSTER_INITIAL_D_START + LEVEL_MONSTER_INITIAL_D_SIZE)
#define LEVEL_TREASURE_COUNT_SIZE 1

#define LEVEL_TREASURE_X_START (LEVEL_TREASURE_COUNT_START + LEVEL_TREASURE_COUNT_SIZE)
#define LEVEL_TREASURE_X_SIZE 32

#define LEVEL_TREASURE_Y_START (LEVEL_TREASURE_X_START + LEVEL_TREASURE_X_SIZE)
#define LEVEL_TREASURE_Y_SIZE 32

#define LEVEL_PLAYER_MAXDX_START (LEVEL_TREASURE_Y_START + LEVEL_TREASURE_Y_SIZE)
#define LEVEL_PLAYER_MAXDX_SIZE (2 * sizeof(int16_t))

#define LEVEL_PLAYER_IMPULSE_START (LEVEL_PLAYER_MAXDX_START + LEVEL_PLAYER_MAXDX_SIZE)
#define LEVEL_PLAYER_IMPULSE_SIZE (2 * sizeof(int16_t))

#define LEVEL_PLAYER_INPUT_START (LEVEL_PLAYER_IMPULSE_START + LEVEL_PLAYER_IMPULSE_SIZE)
#define LEVEL_PLAYER_INPUT_SIZE 2

#define LEVEL_PLAYER_UPDATE_START (LEVEL_PLAYER_INPUT_START + LEVEL_PLAYER_INPUT_SIZE)
#define LEVEL_PLAYER_UPDATE_SIZE 2

#define LEVEL_PLAYER_RENDER_START (LEVEL_PLAYER_UPDATE_START + LEVEL_PLAYER_UPDATE_SIZE)
#define LEVEL_PLAYER_RENDER_SIZE 2

#define LEVEL_MONSTER_MAXDX_START (LEVEL_PLAYER_RENDER_START + LEVEL_PLAYER_RENDER_SIZE)
#define LEVEL_MONSTER_MAXDX_SIZE (6 * sizeof(int16_t))

#define LEVEL_MONSTER_IMPULSE_START (LEVEL_MONSTER_MAXDX_START + LEVEL_MONSTER_MAXDX_SIZE)
#define LEVEL_MONSTER_IMPULSE_SIZE (6 * sizeof(int16_t))

#define LEVEL_MONSTER_INPUT_START (LEVEL_MONSTER_IMPULSE_START + LEVEL_MONSTER_IMPULSE_SIZE)
#define LEVEL_MONSTER_INPUT_SIZE 6

#define LEVEL_MONSTER_UPDATE_START (LEVEL_MONSTER_INPUT_START + LEVEL_MONSTER_INPUT_SIZE)
#define LEVEL_MONSTER_UPDATE_SIZE 6

#define LEVEL_MONSTER_RENDER_START (LEVEL_MONSTER_UPDATE_START + LEVEL_MONSTER_UPDATE_SIZE)
#define LEVEL_MONSTER_RENDER_SIZE 6

#define numLevels() ((uint8_t)pgm_read_byte(&levelData[0]))
#define levelOffset(level) ((uint16_t)pgm_read_word(&levelData[LEVEL_HEADER_SIZE + (level) * sizeof(uint16_t)]))
#define tileSet(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TILESET_START]))
#define compressedMapChunk(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MAP_START + (i)]))
#define playerInitialX(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INITIAL_X_START + (i)]))
#define playerInitialY(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INITIAL_Y_START + (i)]))
#define monsterInitialX(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INITIAL_X_START + (i)]))
#define monsterInitialY(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INITIAL_Y_START + (i)]))
#define monsterInitialD(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INITIAL_D_START + (i)]))
#define treasureCount(levelOffset) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TREASURE_COUNT_START]))

#define treasureX(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TREASURE_X_START + (i)]))
#define treasureY(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_TREASURE_Y_START + (i)]))

#define playerMaxDX(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_PLAYER_MAXDX_START + (i) * sizeof(int16_t)]))
#define playerImpulse(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_PLAYER_IMPULSE_START + (i) * sizeof(int16_t)]))
#define playerInput(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_INPUT_START + (i)]))
#define playerUpdate(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_UPDATE_START + (i)]))
#define playerRender(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_PLAYER_RENDER_START + (i)]))
#define monsterMaxDX(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_MONSTER_MAXDX_START + (i) * sizeof(int16_t)]))
#define monsterImpulse(levelOffset, i) ((int16_t)pgm_read_word(&levelData[(levelOffset) + LEVEL_MONSTER_IMPULSE_START + (i) * sizeof(int16_t)]))
#define monsterInput(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_INPUT_START + (i)]))
#define monsterUpdate(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_UPDATE_START + (i)]))
#define monsterRender(levelOffset, i) ((uint8_t)pgm_read_byte(&levelData[(levelOffset) + LEVEL_MONSTER_RENDER_START + (i)]))

// Returns offset into levelData PROGMEM array
uint16_t LoadLevel(uint8_t level, uint8_t* tileSet)
{
  // Bounds check level
  if (level >= numLevels())
    return 0xFFFF; // bogus value

  // Determine the offset that the level data begins at within the levelData PROGMEM array
  // struct LEVEL_HEADER {
  //   uint8_t numLevels;
  //   uint16_t levelOffsets[numLevels];
  // };
  uint16_t levelOffset = levelOffset(level);

  // Now that we have an offset to the actual level data, read the tile set, and draw the map
  // struct LEVEL_DATA {
  //   uint8_t tileSet;
  //   uint8_t map[105];
  //   uint8_t playerInitialX[2];
  //   uint8_t playerInitialY[2];
  // };
  *tileSet = tileSet(levelOffset);

  uint16_t t = 0; // current tile to draw
  for (uint8_t i = 0; i < BitArray_numBits(SCREEN_TILES_H * SCREEN_TILES_V); ++i) {
    // Read 8 tiles worth of data at a time
    uint8_t chunk = compressedMapChunk(levelOffset, i);
    for (int8_t c = 7; c >= 0; c--) {
      if (chunk & (1 << (c & 7)))
        SetTile(t % SCREEN_TILES_H, t / SCREEN_TILES_H, 41 + (*tileSet * 2));
      else
        SetTile(t % SCREEN_TILES_H, t / SCREEN_TILES_H, 31 + (*tileSet * 5));
      t++;
    }
  }
  return levelOffset;
}

//const uint8_t  playerInitialX[] PROGMEM = {  1,  2 };
//const uint8_t  playerInitialY[] PROGMEM = { 25, 25 };
//const uint8_t monsterInitialX[] PROGMEM = { 25, 28,  9, 16, 19,  7 };
//const uint8_t monsterInitialY[] PROGMEM = { 12, 22, 19, 17,  8, 25 };
//const uint8_t monsterInitialD[] PROGMEM = {  3,  0,  0,  0,  0,  1 };

// How many treasures can be in the level
#define MAX_TREASURE_COUNT 32
// How many frames to wait between 
#define TREASURE_FRAME_SKIP 15
// X coordinates of each treasure for this level
//const uint8_t  treasureX[] PROGMEM = {  1,  7,  4, 12, 18,  6, 24, 27, 21, 28, };
// Y coordinates of each treasure for this level
//const uint8_t  treasureY[] PROGMEM = { 24,  5,  8, 11, 17,  3,  4, 18,  7, 12, };
// Starting tile number of each treasure tile to render (animation frames follow directly after this number)
const uint8_t treasureFG[] PROGMEM = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, };
// Background tile number that should be used to replace each treasure after collected
const uint8_t treasureBG[] PROGMEM = { 30, 30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30, };
// As the animation frame counter advances, this array will be indexed, and its value will be added to the
// treasureFG value for each treasure to compute the next tile number that the treasure will use
const uint8_t treasureAnimation[] PROGMEM = { 0, 1, 2, 1 };


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
  
  SetTileTable(tileset);
  SetSpritesTileBank(0, mysprites);
  InitMusicPlayer(patches);

  ClearVram();

  uint8_t currentLevel = 0;

  uint16_t levelOffset;
  uint8_t tileSet;

 start:

  //DrawMap(0, 0, level1);
  levelOffset = LoadLevel(currentLevel, &tileSet);

  // Sanity check for the prototype implementation
  if (levelOffset != 3)
    goto start;

  // Initialize players
  for (uint8_t i = 0; i < PLAYERS; ++i) {
    player_init(&player[i], inputFunc(playerInput(levelOffset, i)), updateFunc(playerUpdate(levelOffset, i)), renderFunc(playerRender(levelOffset, i)), i,
                (int16_t)(playerInitialX(levelOffset, i) * (TILE_WIDTH << FP_SHIFT)),
                (int16_t)(playerInitialY(levelOffset, i) * (TILE_HEIGHT << FP_SHIFT)),
                (int16_t)(playerMaxDX(levelOffset, i)),
                (int16_t)(playerImpulse(levelOffset, i)));
    ((ENTITY*)(&player[i]))->enabled = true;
    ((ENTITY*)(&player[i]))->left = true;

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
    entity_init(&monster[i],
                inputFunc(monsterInput(levelOffset, i)),
                updateFunc(monsterUpdate(levelOffset, i)),
                renderFunc(monsterRender(levelOffset, i)), PLAYERS + i,
                (int16_t)(monsterInitialX(levelOffset, i) * (TILE_WIDTH << FP_SHIFT)),
                (int16_t)(monsterInitialY(levelOffset, i) * (TILE_HEIGHT << FP_SHIFT)),
                (int16_t)(monsterMaxDX(levelOffset, i)),
                (int16_t)(monsterImpulse(levelOffset, i)));
    uint8_t monsterDirection = monsterInitialD(levelOffset, i);
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
  uint8_t treasureCollected[BitArray_numBits(MAX_TREASURE_COUNT)] = {0}; // bit array

  // Initialize treasure
  uint8_t tcount = treasureCount(levelOffset);
  for (uint8_t i = 0; i < tcount; ++i) {
    //SetTile(pgm_read_byte(&treasureX[i]), pgm_read_byte(&treasureY[i]), (uint16_t)pgm_read_byte(&treasureFG[i]) + tileSet * 15 + 1);
    SetTile(treasureX(levelOffset, i), treasureY(levelOffset, i), tileSet * 15 /*TREASURE_TILES_PER_TILESET*/+ 1 /*TREASURE_START_TILE*/);
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
    for (uint8_t i = 0; i < tcount; ++i) {
      if (BitArray_readBit(treasureCollected, i)) {
        SetTile(treasureX(levelOffset, i), treasureY(levelOffset, i), 30 /*BACKGROUND_START*/+ tileSet * 5 /*BACKGROUND_TILES*/ + 1 /*TREASURE_START_TILE*/);
        //        SetTile(treasureX(levelOffset, i), treasureY(levelOffset, i), (uint16_t)pgm_read_byte(&treasureBG[i]) + tileSet * 5 + 1);
      } else {
        if (treasureFrameCounter % TREASURE_FRAME_SKIP == 0)
          SetTile(treasureX(levelOffset, i), treasureY(levelOffset, i),
                  (uint16_t)tileSet * 15 + 1 + pgm_read_byte(&treasureAnimation[treasureFrameCounter / TREASURE_FRAME_SKIP]));
          /* SetTile(pgm_read_byte(&treasureX[i]), pgm_read_byte(&treasureY[i]), */
          /*         (uint16_t)(pgm_read_byte(&treasureFG[i]) + tileSet * 15 + 1 + pgm_read_byte(&treasureAnimation[treasureFrameCounter / TREASURE_FRAME_SKIP]))); */
      }
    }
    if (++treasureFrameCounter == TREASURE_FRAME_SKIP * NELEMS(treasureAnimation))
      treasureFrameCounter = 0;

    // Check for collisions with treasure
    for (uint8_t p = 0; p < PLAYERS; ++p) {
      if (((ENTITY*)(&player[p]))->enabled == true) {
        for (uint8_t i = 0; i < tcount; ++i) {
          if (BitArray_readBit(treasureCollected, i))
            continue;

          // The calculation below assumes each sprite is WORLD_METER wide
          if (overlap(((ENTITY*)(&player[p]))->x,
                      ((ENTITY*)(&player[p]))->y,
                      WORLD_METER,
                      WORLD_METER,
                      (uint16_t)treasureX(levelOffset, i) * (TILE_WIDTH << FP_SHIFT),
                      (uint16_t)treasureY(levelOffset, i) * (TILE_HEIGHT << FP_SHIFT),
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
