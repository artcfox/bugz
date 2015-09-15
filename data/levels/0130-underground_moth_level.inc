  THEME_DIRT, // uint8_t theme
  LE(400), // uint16_t timeBonus
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT, // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER, // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, 0, IFLAG_LEFT, IFLAG_SPRITE_FLIP_X, 0, // uint8_t monsterFlags[6]
  LE(WORLD_METER * 1), LE(WORLD_METER * 2), LE(WORLD_METER * 3), LE(WORLD_METER * 5), LE(WORLD_METER * 3), LE(WORLD_METER * 2), // int16_t monsterMaxDX[6]
  0x22,0, 0x31,8, 0x22,64, LE(WORLD_JUMP), 0x22,0, 0x22,16, // int16_t monsterImpulse[6]
  AI_FLY_CIRCLE_CCW, AI_FLY_CIRCLE_CW, AI_FLY_CIRCLE_CCW, AI_HOP_UNTIL_BLOCKED, AI_FLY_CIRCLE_CCW, AI_FLY_CIRCLE_CW, // INPUT_FUNCTIONS monsterInputFuncs[6]
  NULL_UPDATE, NULL_UPDATE, NULL_UPDATE, ENTITY_UPDATE, NULL_UPDATE, NULL_UPDATE,  // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  MOTH_RENDER, MOTH_RENDER, MOTH_RENDER, GRASSHOPPER_RENDER, MOTH_RENDER, MOTH_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]