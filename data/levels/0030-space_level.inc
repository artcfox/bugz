  THEME_SPACE, // uint8_t theme;
  LE(200), // uint16_t timeBonus
  IFLAG_SPRITE_FLIP_X, IFLAG_SPRITE_FLIP_X, // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT, // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER, // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_DOWN|IFLAG_SPRITE_FLIP_X, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT, IFLAG_LEFT,  IFLAG_LEFT, // uint8_t monsterFlags[6]
  LE(WORLD_METER * 5), LE(WORLD_METER * 1), LE(WORLD_METER * 3), LE(WORLD_METER * 2), LE(WORLD_METER * 1), LE(WORLD_METER * 1), // int16_t monsterMaxDX[6]
  16, 23, LE(WORLD_JUMP >> 1), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t monsterImpulse[6]
  AI_FLY_VERTICAL_UNDULATE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, AI_WALK_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE_FLYING, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  BEE_RENDER, CRICKET_RENDER, LADYBUG_RENDER, GRASSHOPPER_RENDER, ANT_RENDER, ANT_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]