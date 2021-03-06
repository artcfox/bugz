  THEME_SPACE, // uint8_t theme
  LE(150), // uint16_t timeBonus
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  PLAYER_INPUT, PLAYER_INPUT, // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER, // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_LEFT, IFLAG_LEFT, IFLAG_RIGHT, IFLAG_UP, IFLAG_UP, IFLAG_UP, // uint8_t monsterFlags[6]
  LE(WORLD_METER * 1), LE(WORLD_METER * 1), LE(WORLD_METER * 3), LE(WORLD_METER * 13), LE(WORLD_METER * 9), LE(WORLD_METER * 13), // int16_t monsterMaxDX[6]
  LE(WORLD_JUMP), LE(WORLD_JUMP >> 1), LE(WORLD_JUMP), 20,26, 20,26, 20,26, // int16_t monsterImpulse[6]
  AI_WALK_UNTIL_BLOCKED, AI_HOP_UNTIL_BLOCKED_OR_LEDGE, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_FLY_VERTICAL_UNDULATE, AI_FLY_VERTICAL_UNDULATE, AI_FLY_VERTICAL_UNDULATE, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  ANT_RENDER, CRICKET_RENDER, LADYBUG_RENDER, FRUITFLY_RENDER, FRUITFLY_RENDER, FRUITFLY_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]
