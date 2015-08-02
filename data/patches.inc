/*
 *  Uzebox Default Patches
 *  Copyright (C) 2008  Alec Bourque
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
	Patches are made of a command stream made of 3 bytes per command:
		1=delta time
		2=command
		3=command parameter

	Patches must start with 1 byte describing the sound type:
		0=wave channel (i.e.; channel 0,1 and 2)
		1=noise channel (channel 3)
		2=PCM (channel 3)
		->For type=2 two more byte follows in order: sample adress low byte, sample adress hi byte

	It must end with <0,PATCH_END> and this command takes
	only two bytes (no parameter).
*/

//FX: jump
const char patch00[] PROGMEM ={
0,PC_WAVE,3,
0,PC_ENV_SPEED,-8,
0,PC_PITCH,80,
1,PC_NOTE_UP,4,
1,PC_NOTE_CUT,0,
0,PATCH_END
};

//FX: kill monster
const char patch01[] PROGMEM ={
0,PC_WAVE,4,
0,PC_ENV_SPEED,-8,
1,PC_PITCH,84,
1,PC_PITCH,80,
1,PC_PITCH,81,
1,PC_PITCH,80,
1,PC_NOTE_CUT,0,
0,PATCH_END
};

//FX: cookie
const char patch02[] PROGMEM ={
0,PC_WAVE,4,
0,PC_ENV_SPEED,-8,
1,PC_PITCH,95,
1,PC_NOTE_UP,2,
1,PC_NOTE_UP,2,
1,PC_NOTE_CUT,0,
0,PATCH_END
};

//FX: player death
const char patch03[] PROGMEM ={
0,PC_WAVE,4,
0,PC_ENV_SPEED,-16,
1,PC_PITCH,54,
1,PC_NOTE_DOWN,2,
1,PC_NOTE_DOWN,3,
1,PC_NOTE_CUT,0,
0,PATCH_END
};

/* //FX: unused */
/* const char patch04[] PROGMEM ={ */
/* 0,PC_WAVE,4, */
/* 0,PC_ENV_SPEED,0, */
/* 1,PC_PITCH,100, */
/* 1,PC_NOTE_CUT,0, */
/* 0,PATCH_END */
/* }; */
	
const struct PatchStruct patches[] PROGMEM = {
{0,NULL,patch00,0,0},
{0,NULL,patch01,0,0},
{0,NULL,patch02,0,0},
{0,NULL,patch03,0,0},
/* {0,NULL,patch04,0,0}, */
};
