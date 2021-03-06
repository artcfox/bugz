  THEME_SPACE, // uint8_t theme
  LE(300), // uint16_t timeBonus
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  PLAYER_INPUT, PLAYER_INPUT, // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER, // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_LEFT, IFLAG_RIGHT, IFLAG_DOWN, IFLAG_RIGHT, IFLAG_RIGHT, IFLAG_DOWN|IFLAG_SPRITE_FLIP_X, // uint8_t monsterFlags[6]
  LE(WORLD_METER * 3), LE(WORLD_METER * 1), LE(WORLD_METER * 5), LE(WORLD_METER * 6), LE(WORLD_METER * 5), LE(WORLD_METER * 12), // int16_t monsterMaxDX[6]
  LE(WORLD_JUMP), LE(WORLD_JUMP >> 1), 1,25, 11,27, 11,16, 1,15, // int16_t monsterImpulse[6]
  AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED_OR_LEDGE, AI_FLY_VERTICAL_UNDULATE, AI_FLY_HORIZONTAL_ERRATIC, AI_FLY_HORIZONTAL_UNDULATE, AI_FLY_VERTICAL, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING, ENTITY_UPDATE_FLYING,  // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  LADYBUG_RENDER, CRICKET_RENDER, FRUITFLY_RENDER, BUTTERFLY_RENDER, MOTH_RENDER, ALT_SPIDER_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]
