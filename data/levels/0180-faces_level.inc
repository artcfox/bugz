  THEME_SPACE, // uint8_t theme
  LE(300), // uint16_t timeBonus
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  PLAYER_INPUT, PLAYER_INPUT, // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER, // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_UP|IFLAG_SPRITE_FLIP_X, 0, 0, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT, // uint8_t monsterFlags[6]
  LE(WORLD_METER * 12), LE(WORLD_METER * 2), LE(WORLD_METER * 3), LE(WORLD_METER * 6), LE(WORLD_METER * 5), LE(WORLD_METER * 5), // int16_t monsterMaxDX[6]
  20,25, 0x22,8, 0x22,16, 2,11, 20,27, 20,27, // int16_t monsterImpulse[6]
  AI_FLY_VERTICAL, AI_FLY_CIRCLE_CCW, AI_FLY_CIRCLE_CW, AI_FLY_HORIZONTAL_UNDULATE, AI_FLY_HORIZONTAL_UNDULATE, AI_FLY_HORIZONTAL_UNDULATE, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE_FLYING, NULL_UPDATE, NULL_UPDATE, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING,  // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  ALT_SPIDER_RENDER, MOTH_RENDER, MOTH_RENDER, MOTH_RENDER, FRUITFLY_RENDER, FRUITFLY_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]
