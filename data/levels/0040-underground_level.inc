  THEME_DIRT, // uint8_t theme;
  LE(300), // uint16_t timeBonus
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT, // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER, // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_SPRITE_FLIP_X|IFLAG_LEFT, IFLAG_SPRITE_FLIP_X|IFLAG_RIGHT, IFLAG_SPRITE_FLIP_X|IFLAG_RIGHT, IFLAG_LEFT, IFLAG_DOWN, IFLAG_LEFT, // uint8_t monsterFlags[6]
  LE(WORLD_METER * 4), LE(WORLD_METER * 2), LE(WORLD_METER * 3), LE(WORLD_METER * 6), LE(WORLD_METER * 8), LE(WORLD_METER * 4), // int16_t monsterMaxDX[6]
  LE(WORLD_JUMP), LE(WORLD_JUMP >> 1), LE(WORLD_JUMP), 1, 16, 13, 23, LE(WORLD_JUMP), // int16_t monsterImpulse[6]
  AI_WALK_UNTIL_BLOCKED, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_FLY_HORIZONTAL_UNDULATE, AI_FLY_VERTICAL, AI_HOP_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  ANT_RENDER, CRICKET_RENDER, ANT_RENDER, FRUITFLY_RENDER, SPIDER_RENDER, GRASSHOPPER_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]