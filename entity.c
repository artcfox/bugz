#include "entity.h"

void null_input(ENTITY* e) { }
void null_update(ENTITY* e) { }
void null_render(ENTITY* e) { }

void entity_init(ENTITY* e, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx)
{
  e->input = input;
  e->update = update;
  e->render = render;
  e->tag = tag;
  e->x = x;
  e->y = y;
  e->maxdx = maxdx;
  e->dx = e->dy = e->ddx = e->ddy = e->falling = e->jumping = e->left = e->right = e->jump = 0;
}

void player_init(PLAYER* p, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx)
{
  entity_init((ENTITY*)p, input, update, render, tag, x, y, maxdx);
  memset(&p->buttons, 0, sizeof(p->buttons));
  p->jumpReleased = true;
  p->framesFalling = 0;
}

