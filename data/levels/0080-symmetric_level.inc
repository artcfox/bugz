  THEME_GRASS, // uint8_t theme;
  LE(300), // uint16_t timeBonus
  0, IFLAG_SPRITE_FLIP_X,  // uint8_t playerFlags[2]
  LE(WORLD_MAXDX), LE(WORLD_MAXDX), // int16_t playerMaxDX[2]
  LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t playerImpulse[2]
  PLAYER_INPUT, PLAYER_INPUT, // INPUT_FUNCTIONS playerInputFuncs[2]
  ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS playerUpdateFuncs[2]
  PLAYER_RENDER, PLAYER_RENDER, // RENDER_FUNCTIONS playerRenderFuncs[2]
  IFLAG_LEFT, IFLAG_RIGHT, IFLAG_LEFT, IFLAG_RIGHT, IFLAG_RIGHT, IFLAG_LEFT, // uint8_t monsterFlags[6]
  LE(WORLD_METER * 3), LE(WORLD_METER * 3), LE(WORLD_METER * 2), LE(WORLD_METER * 2), LE(WORLD_METER * 4), LE(WORLD_METER * 4), // int16_t monsterMaxDX[6]
  LE(WORLD_JUMP), LE(WORLD_JUMP), LE(WORLD_JUMP >> 1), LE(WORLD_JUMP >> 1), LE(WORLD_JUMP), LE(WORLD_JUMP), // int16_t monsterImpulse[6]
  AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_WALK_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED_OR_LEDGE, AI_HOP_UNTIL_BLOCKED, AI_HOP_UNTIL_BLOCKED, // INPUT_FUNCTIONS monsterInputFuncs[6]
  ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, ENTITY_UPDATE, // UPDATE_FUNCTIONS monsterUpdateFuncs[6]
  LADYBUG_RENDER, LADYBUG_RENDER, CRICKET_RENDER, CRICKET_RENDER, GRASSHOPPER_RENDER, GRASSHOPPER_RENDER, // RENDER_FUNCTIONS monsterRenderFuncs[6]