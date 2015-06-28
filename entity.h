#ifndef __ENTITY_H__
#define __ENTITY_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
  One way of implementing everthing is to extend the idea that I'm
  already using, which is to have an array of PLAYER structs for each
  player, and I could have a fixed size array of MONSTER structs whose
  size is the maximum number of monsters that I can have on the screen
  at any time, and i could have a BOSS struct for when there is a boss
  fight. Each one would have an enabled flag, so they could be turned
  on or off as needed.
 */

struct ENTITY;
typedef struct ENTITY ENTITY;

struct ENTITY {
  void (*input)(ENTITY*);
  void (*update)(ENTITY*);
  void (*render)(ENTITY*);
  uint8_t tag;
  uint16_t x;
  uint16_t y;
  int16_t dx;
  int16_t dy;
  int16_t ddx;
  int16_t ddy;
  int16_t maxdx;
  unsigned int enabled : 1;
  unsigned int falling : 1;
  unsigned int jumping : 1;
  unsigned int left : 1;
  unsigned int right : 1;
  unsigned int jump : 1;
} __attribute__ ((packed));

// Default functions that do nothing
void null_input(ENTITY* e);
void null_update(ENTITY* e);
void null_render(ENTITY* e);

void entity_init(ENTITY* e, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx);

struct BUTTON_INFO;
typedef struct BUTTON_INFO BUTTON_INFO;

struct BUTTON_INFO {
  uint16_t held;
  uint16_t pressed;
  uint16_t released;
  uint16_t prev;
};

struct PLAYER;
typedef struct PLAYER PLAYER;

struct PLAYER { ENTITY entity;
  BUTTON_INFO buttons;

  bool jumpReleased; // state variable used for allowing jump to be pressed early, and for implementing variable jumping

  uint16_t framesFalling; // used for allowing late jumps immediately after falling

  // Debugging purposes
  /* bool clamped; */

  /* u8 w; */
  /* u8 h; */
} __attribute__ ((packed));

void player_init(PLAYER* p, void (*input)(ENTITY*), void (*update)(ENTITY*), void (*render)(ENTITY*), uint8_t tag, uint16_t x, uint16_t y, int16_t maxdx);

#endif // __ENTITY_H__
