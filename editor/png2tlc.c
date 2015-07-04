/*

  png2tlc.c

  Converts a directory of PNG files (whose height in pixels matches
  the number of LEDs on the RGB-POV hardware) into a single output
  file that contains all of those images packed in an optimized format
  that the RGB-POV hardware can read off a microSD card.

  Copyright 2011-2012 Matthew T. Pandina. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY MATTHEW T. PANDINA "AS IS" AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHEW T. PANDINA OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// open, stat, lseek
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// errno
#include <errno.h>

// mmap
#ifdef __MINGW32__
#include <windows.h>
#else // __MINGW32__
#include <sys/mman.h>
#endif // __MINGW32__

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif // __APPLE__

// memcpy
#include <string.h>

// scandir
#include <dirent.h>

// PNG
#include <png.h>
#include <math.h>


// The function scandir() is not implemented in MinGW32, so opendir()
// and readdir() are used instead. A Red-Black Tree is used to ensure
// that the files are processed in alphabetical order
#include "dirent_node.h"

#define BitArray_numBits(bits) (((bits) >> 3) + 1 * (((bits) & 7) ? 1 : 0))
#define BitArray_setBit(array, index) ((array)[(index) >> 3] |= (1 << ((index) & 7)))
#define BitArray_clearBit(array, index) ((array)[(index) >> 3] &= ((1 << ((index) & 7)) ^ 0xFF))
#define BitArray_readBit(array, index) ((bool)((array)[(index) >> 3] & (1 << ((index) & 7))))

// Which byte offset each field begins on
#define SLICE_TLC_SIGNATURE 0
#define SLICE_HEADER_VERSION 8
#define SLICE_TOTAL_BLOCK_COUNT 9
#define SLICE_SIZE_INFO 13
#define SLICE_ENTRIES_START_BLOCK 14
#define SLICE_MAX_ENTRY_COUNT 15
#define SLICE_ENTRY_COUNT 19
#define SLICE_NEXT_FREE_DATA_BLOCK 23

#define SLICE_DATA_START_BLOCK 0
#define SLICE_DATA_TOTAL_BYTES 4
#define SLICE_BYTES_PER_COLUMN 8
#define SLICE_IMAGE_NAME 10

#define TLC_SIGNATURE { 0x89, 'T', 'L', 'C', 0x0d, 0x0a, 0x1a, 0x0a }
#define END_SIGNATURE { 0x89, 'E', 'N', 'D', 0x0d, 0x0a, 0x1a, 0x0a }
#define ENTRIES_START_BLOCK 1
#define BLOCK_LEN 9 // 2^9 = 512
#define ENTRY_LEN 6 // 2^6 = 64

#define FILENAME_LEN 256
#define DEFAULT_OUTPUT_FILENAME "RGB-POV.TLC"
char outputFileName[FILENAME_LEN] = {0};

#define DEFAULT_INPUT_DIRECTORY "RGB-POV"
char inputDirectory[FILENAME_LEN] = {0};

size_t outputFileSize = 64 * 1024 * 1024; // default 64 MiB
size_t maxEntries = 65536; // default max number of entries

// ----- TLC5940 Stuff
uint8_t TLC5940_PWM_BITS = 12;
#define TLC5940_MULTIPLEX_N 3
#define gsData_t uint16_t
#define channel_t uint16_t
#define channel3_t uint16_t

gsData_t gsDataSize;
channel_t numChannels;

uint8_t breadboardMode = 0;

// maps a linear 8-bit value to a 12-bit gamma corrected value
uint16_t TLC5940_GammaCorrect[256];

void TLC5940_SetGS(uint8_t gsData[][TLC5940_MULTIPLEX_N], uint8_t row,
                   channel_t channel, uint16_t value) {
  channel = numChannels - 1 - channel;
  channel3_t i = (channel3_t)channel * 3 / 2;

  switch (channel % 2) {
  case 0:
    gsData[i][row] = (value >> 4);
    i++;
    gsData[i][row] = (gsData[i][row] & 0x0F) | (uint8_t)(value << 4);
    break;
  default: // case 1:
    gsData[i][row] = (gsData[i][row] & 0xF0) | (value >> 8);
    i++;
    gsData[i][row] = (uint8_t)value;
    break;
  }
}

void TLC5940_SetAllGS(uint8_t gsData[][TLC5940_MULTIPLEX_N], uint8_t row,
                      uint16_t value) {
  uint8_t tmp1 = (value >> 4);
  uint8_t tmp2 = (uint8_t)(value << 4) | (tmp1 >> 4);
  gsData_t i = 0;
  do {
    gsData[i++][row] = tmp1;              // bits: 11 10 09 08 07 06 05 04
    gsData[i++][row] = tmp2;              // bits: 03 02 01 00 11 10 09 08
    gsData[i++][row] = (uint8_t)value;    // bits: 07 06 05 04 03 02 01 00
  } while (i < gsDataSize);
}

void TLC5940_InitGamma(uint8_t TLC5940_PWM_BITS) {
  // Build gamma correction table
  for (uint16_t i = 0; i < 256; i++)
    TLC5940_GammaCorrect[i] = (uint16_t)(pow((double)i / 255.0, 2.5) *
                                         ((double)(1 << TLC5940_PWM_BITS) - 1) +
                                         0.5);
}

// ----- End TLC5940 Stuff

int createEmptyFile(char *name, off_t size) {
  // Create the file
  int fd = open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  // Seek to the end of the file
  off_t offset = lseek(fd, size - 1, SEEK_SET);
  if (offset < 0) {
    perror("lseek");
    goto err_lseek;
  } else if (offset != size - 1) {
    fprintf(stderr, "lseek: offset returned is not offset requested\n");
    goto err_lseek;
  }

  /* Write a single byte */ {
    uint8_t buf = 0;
    if (write(fd, &buf, sizeof(buf)) < 0) {
      perror("write");
      goto err_write;
    }
  }

  // If successful, return the file descriptor
  return fd;

  // If there are any errors, close the file
 err_lseek:
 err_write:
  if (close(fd) < 0)
    perror("close");

  return -1;
}

uint8_t getTlcSignature(uint8_t *mm, uint8_t *signature, size_t length) {
  if (length == 8)
    memcpy(signature, &mm[0], 8);
  else
    return -1;
  return 0;
}

void setTlcSignature(uint8_t *mm) {
  uint8_t tlcStartSignature[] = TLC_SIGNATURE;
  memcpy(&mm[SLICE_TLC_SIGNATURE], tlcStartSignature, sizeof(tlcStartSignature));
}

uint8_t getHeaderVersion(uint8_t *mm) {
  return mm[SLICE_HEADER_VERSION];
}

void setHeaderVersion(uint8_t *mm, uint8_t value) {
  mm[SLICE_HEADER_VERSION] = value;
}

uint32_t getTotalBlockCount(uint8_t *mm) {
  return mm[SLICE_TOTAL_BLOCK_COUNT] +
    (mm[SLICE_TOTAL_BLOCK_COUNT + 1] << 8) +
    (mm[SLICE_TOTAL_BLOCK_COUNT + 2] << 16) +
    (mm[SLICE_TOTAL_BLOCK_COUNT + 3] << 24);
}

void setTotalBlockCount(uint8_t *mm, uint32_t value) {
  //  memcpy(&mm[SLICE_TOTAL_BLOCK_COUNT], &value, sizeof(value);
  size_t offset = SLICE_TOTAL_BLOCK_COUNT;
  for (uint8_t i = 0; i < 32; i += 8)
    mm[offset++] = (uint8_t)(value >> i);
}

uint8_t getBlockLen(uint8_t *mm) {
  return ((mm[SLICE_SIZE_INFO] & 0xF0) >> 4);
}

uint16_t getBytesPerBlock(uint8_t *mm) {
  return (1 << getBlockLen(mm));
}

void setBlockLen(uint8_t *mm, uint8_t value) {
  mm[SLICE_SIZE_INFO] = (mm[SLICE_SIZE_INFO] & 0x0F) | (value << 4);
}

uint8_t getEntryLen(uint8_t *mm) {
  return (mm[SLICE_SIZE_INFO] & 0x0F);
}

uint16_t getBytesPerEntry(uint8_t *mm) {
  return (1 << getEntryLen(mm));
}

void setEntryLen(uint8_t *mm, uint8_t value) {
  mm[SLICE_SIZE_INFO] = (mm[SLICE_SIZE_INFO] & 0xF0) | (value & 0x0F);
}

uint8_t getEntriesStartBlock(uint8_t *mm) {
  return mm[SLICE_ENTRIES_START_BLOCK];
}

void setEntriesStartBlock(uint8_t *mm, uint8_t value) {
  mm[SLICE_ENTRIES_START_BLOCK] = value;
}

uint32_t getMaxEntryCount(uint8_t *mm) {
  return mm[SLICE_MAX_ENTRY_COUNT] +
    (mm[SLICE_MAX_ENTRY_COUNT + 1] << 8) +
    (mm[SLICE_MAX_ENTRY_COUNT + 2] << 16) +
    (mm[SLICE_MAX_ENTRY_COUNT + 3] << 24);
}

void setMaxEntryCount(uint8_t *mm, uint32_t value) {
  size_t offset = SLICE_MAX_ENTRY_COUNT;
  for (uint8_t i = 0; i < 32; i += 8)
    mm[offset++] = (uint8_t)(value >> i);
}

uint32_t getEntryCount(uint8_t *mm) {
  return mm[SLICE_ENTRY_COUNT] +
    (mm[SLICE_ENTRY_COUNT + 1] << 8) +
    (mm[SLICE_ENTRY_COUNT + 2] << 16) +
    (mm[SLICE_ENTRY_COUNT + 3] << 24);
}

void setEntryCount(uint8_t *mm, uint32_t value) {
  size_t offset = SLICE_ENTRY_COUNT;
  for (uint8_t i = 0; i < 32; i += 8)
    mm[offset++] = (uint8_t)(value >> i);
}

uint32_t getNextFreeDataBlock(uint8_t *mm) {
  return mm[SLICE_NEXT_FREE_DATA_BLOCK] +
    (mm[SLICE_NEXT_FREE_DATA_BLOCK + 1] << 8) +
    (mm[SLICE_NEXT_FREE_DATA_BLOCK + 2] << 16) +
    (mm[SLICE_NEXT_FREE_DATA_BLOCK + 3] << 24);
}

void setNextFreeDataBlock(uint8_t *mm, uint32_t value) {
  size_t offset = SLICE_NEXT_FREE_DATA_BLOCK;
  for (uint8_t i = 0; i < 32; i += 8)
    mm[offset++] = (uint8_t)(value >> i);
}

int addEntry(uint8_t *mm, uint32_t dataStartBlock,
             uint32_t dataTotalBytes, uint16_t bytesPerColumn,
             char *imageName) {
  size_t base_offset = getEntriesStartBlock(mm) * getBytesPerBlock(mm) +
    getEntryCount(mm) * getBytesPerEntry(mm);

  if (getEntryCount(mm) >= getMaxEntryCount(mm)) {
    fprintf(stderr, "Unable to add: '%s'\n", imageName);
    return -1;
  }

  /* // alternative check that rounds MAX_ENTRIES up to the nearest full block */
  /* if (base_offset >= (getEntriesStartBlock(mm) * getBytesPerBlock(mm) + */
  /*                    getMaxEntryCount(mm) * getBytesPerEntry(mm))) { */
  /*   fprintf(stderr, "Not enough room to add: '%s'\n", imageName); */
  /*   return -1; */
  /* } */

  // DATA_START_BLOCK
  size_t offset = SLICE_DATA_START_BLOCK;
  for (uint8_t i = 0; i < 32; i += 8)
    mm[base_offset + offset++] = (uint8_t)(dataStartBlock >> i);

  // DATA_TOTAL_BYTES
  offset = SLICE_DATA_TOTAL_BYTES;
  for (uint8_t i = 0; i < 32; i += 8)
    mm[base_offset + offset++] = (uint8_t)(dataTotalBytes >> i);

  // BYTES_PER_COLUMN
  offset = SLICE_BYTES_PER_COLUMN;
  for (uint8_t i = 0; i < 16; i += 8)
    mm[base_offset + offset++] = (uint8_t)(bytesPerColumn >> i);

  // IMAGE_NAME
  uint16_t imageNameLength = getBytesPerEntry(mm) - SLICE_IMAGE_NAME;
  memset(&mm[base_offset + SLICE_IMAGE_NAME], 0, imageNameLength);
  strncpy((char *)&mm[base_offset + SLICE_IMAGE_NAME], imageName,
          imageNameLength);

  // update ENTRY_COUNT
  setEntryCount(mm, getEntryCount(mm) + 1);

  // update NEXT_FREE_DATA_BLOCK
  setNextFreeDataBlock(mm, dataStartBlock +
                       (dataTotalBytes / (1 << BLOCK_LEN)) +
                       ((dataTotalBytes % (1 << BLOCK_LEN)) ? 1 : 0));

  return 0;
}

int initializeMappedFile(uint8_t *mfile, uint32_t numFullBlocks) {
  uint8_t tlcEndSignature[] = END_SIGNATURE;

  // TLC_SIGNATURE
  setTlcSignature(mfile);

  // END_SIGNATURE
  memcpy(&mfile[(off_t)(numFullBlocks - 1) * (1 << BLOCK_LEN)],
         tlcEndSignature, sizeof(tlcEndSignature));

  // HEADER_VERSION (stored in binary coded decimal: high.low)
  setHeaderVersion(mfile, 0x02);

  // TOTAL_BLOCK_COUNT
  setTotalBlockCount(mfile, numFullBlocks);

  // BLOCK_LEN
  setBlockLen(mfile, BLOCK_LEN);

  // ENTRY_LEN
  setEntryLen(mfile, ENTRY_LEN);

  // ENTRIES_START_BLOCK
  setEntriesStartBlock(mfile, ENTRIES_START_BLOCK);

  // MAX_ENTRY_COUNT
  setMaxEntryCount(mfile, maxEntries);

  // ENTRY_COUNT
  setEntryCount(mfile, 0);

  // NEXT_FREE_DATA_BLOCK
  uint32_t nextFreeDataBlock = ENTRIES_START_BLOCK +
    (maxEntries * (1 << ENTRY_LEN) /
     (1 << BLOCK_LEN) +
     ((maxEntries * (1 << ENTRY_LEN) %
       (1 << BLOCK_LEN)) ? 1 : 0));

  setNextFreeDataBlock(mfile, nextFreeDataBlock);
  return 0;
}

struct PIXEL_DATA;
typedef struct PIXEL_DATA PIXEL_DATA;
struct PIXEL_DATA {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

bool isSky(const PIXEL_DATA* p)
{
  return ((p->r == 255) &&
          (p->g == 255) &&
          (p->b == 218));
}
bool isSolid(const PIXEL_DATA* p)
{
  return ((p->r == 0) &&
          (p->g == 0) &&
          (p->b == 0));
}
bool isPlayer0(const PIXEL_DATA* p)
{
  return ((p->r == 0) &&
          (p->g == 218) &&
          (p->b == 218));
}
bool isPlayer1(const PIXEL_DATA* p)
{
  return ((p->r == 255) &&
          (p->g == 109) &&
          (p->b == 145));
}
bool isMonster0(const PIXEL_DATA* p)
{
  return ((p->r == 255) &&
          (p->g == 0) &&
          (p->b == 0));
}
bool isMonster1(const PIXEL_DATA* p)
{
  return ((p->r == 255) &&
          (p->g == 109) &&
          (p->b == 0));
}
bool isMonster2(const PIXEL_DATA* p)
{
  return ((p->r == 255) &&
          (p->g == 255) &&
          (p->b == 0));
}
bool isMonster3(const PIXEL_DATA* p)
{
  return ((p->r == 0) &&
          (p->g == 255) &&
          (p->b == 0));
}
bool isMonster4(const PIXEL_DATA* p)
{
  return ((p->r == 0) &&
          (p->g == 0) &&
          (p->b == 145));
}
bool isMonster5(const PIXEL_DATA* p)
{
  return ((p->r == 145) &&
          (p->g == 72) &&
          (p->b == 0));
}
bool isTreasure(const PIXEL_DATA* p)
{
  return ((p->r == 218) &&
          (p->g == 182) &&
          (p->b == 0));
}

int addPNG(uint8_t *mfile, char *pngfile) {
  // decode PNG file
  FILE *fp = fopen(pngfile, "rb");
  if (!fp) {
    fprintf(stderr, "Unable to open '%s'\n", pngfile);
    return -1;
  }

  unsigned char header[8]; // NUM_SIG_BYTES = 8
  fread(header, 1, sizeof(header), fp);
  if (png_sig_cmp(header, 0, sizeof(header))) {
    fprintf(stderr, "'%s' is not a png file\n", pngfile);
    fclose(fp);
    return -1;
  }

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                               (png_voidp)NULL, NULL, NULL);
  if (!png_ptr) {
    fprintf(stderr, "png_create_read_struct() failed\n");
    fclose(fp);
    return -1;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    fprintf(stderr, "png_create_info_struct() failed\n");
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    fclose(fp);
    return -1;
  }

  png_infop end_info = png_create_info_struct(png_ptr);
  if (!end_info) {
    fprintf(stderr, "png_create_info_struct() failed\n");
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    fclose(fp);
    return -1;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    fprintf(stderr, "longjmp() called\n");
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fp);
    return -1;
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, sizeof(header));
  png_read_info(png_ptr, info_ptr);

  png_uint_32 width, height;
  int bit_depth, color_type, interlace_method, compression_method,
    filter_method;
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               &interlace_method, &compression_method, &filter_method);

  /* do any transformations necessary here */
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png_ptr);

  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  png_color_16 black_bg = {0, 0, 0, 0, 0};
  png_set_background(png_ptr, &black_bg, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1);

  png_read_update_info(png_ptr, info_ptr);
  /* end transformations */

  png_uint_32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);
  png_uint_32 pixel_size = rowbytes / width;

  if (height > PNG_UINT_32_MAX / png_sizeof(png_byte))
    png_error(png_ptr, "Image is too tall to process in memory");
  if (width > PNG_UINT_32_MAX / pixel_size)
    png_error(png_ptr, "Image is too wide to process in memory");

  png_bytep data = (png_bytep)png_malloc(png_ptr, height * rowbytes);
  png_bytepp row_pointers = (png_bytepp)png_malloc(png_ptr, height * png_sizeof(png_bytep));

  for (png_uint_32 i = 0; i < height; ++i)
    row_pointers[i] = &data[i * rowbytes];

  png_set_rows(png_ptr, info_ptr, row_pointers);
  png_read_image(png_ptr, row_pointers);

  // data now contains the byte data in the format RGB

  // hooray for C99 variable length arrays!
  /* uint8_t gsData[gsDataSize][TLC5940_MULTIPLEX_N]; */

  int errorVal = 0;

  if (width != 30 || height < 28) {
    fprintf(stderr, "Error: Image size should be 30x28, but if taller, only the top 28 rows will be used\n");
    errorVal = -1;
    goto cleanup;
  }

  uint8_t map[BitArray_numBits(30 * 28)] = {0};
  size_t mapOffset = 0;
  uint8_t playerInitialX[2] = {0};
  uint8_t playerInitialY[2] = {0};
  uint8_t monsterInitialX[6] = {0};
  uint8_t monsterInitialY[6] = {0};
  uint8_t treasureX[32] = {0};
  uint8_t treasureY[32] = {0};
  uint8_t treasureOffset = 0;

  // Scan through the PNG, and build the base map
  for (uint8_t h = 0; h < 28; h++) {
    for (uint8_t w = 0; w < width; w++) {
      PIXEL_DATA pixel = {
        *(data + h * width * 3 + w * 3 + 0),
        *(data + h * width * 3 + w * 3 + 1),
        *(data + h * width * 3 + w * 3 + 2),
      };
      if (isSolid(&pixel))
        BitArray_setBit(map, mapOffset);
      else
        BitArray_clearBit(map, mapOffset);
      mapOffset++;

      if (isPlayer0(&pixel)) {
        playerInitialX[0] = w;
        playerInitialY[0] = h;
      } else if (isPlayer1(&pixel)) {
        playerInitialX[1] = w;
        playerInitialY[1] = h;
      } else if (isMonster0(&pixel)) {
        monsterInitialX[0] = w;
        monsterInitialY[0] = h;        
      } else if (isMonster1(&pixel)) {
        monsterInitialX[1] = w;
        monsterInitialY[1] = h;        
      } else if (isMonster2(&pixel)) {
        monsterInitialX[2] = w;
        monsterInitialY[2] = h;        
      } else if (isMonster3(&pixel)) {
        monsterInitialX[3] = w;
        monsterInitialY[3] = h;        
      } else if (isMonster4(&pixel)) {
        monsterInitialX[4] = w;
        monsterInitialY[4] = h;        
      } else if (isMonster5(&pixel)) {
        monsterInitialX[5] = w;
        monsterInitialY[5] = h;        
      } else if (isTreasure(&pixel) && treasureOffset < 32) {
        treasureX[treasureOffset] = w;
        treasureY[treasureOffset] = h;
        treasureOffset++;
      }
      //printf("(%d,%d,%d), ", pixel.r, pixel.g, pixel.b);
    }
  }
  
  for (uint8_t i = 0; i < sizeof(map); ++i)
    printf("%d,", map[i]);
  printf(" // map data from file: %s\n", pngfile);

  for (uint8_t i = 0; i < sizeof(playerInitialX); ++i)
    printf("%d,", playerInitialX[i]);
  printf(" // playerInitialX[%d]\n", (int)sizeof(playerInitialX));
  for (uint8_t i = 0; i < sizeof(playerInitialY); ++i)
    printf("%d,", playerInitialY[i]);
  printf(" // playerInitialY[%d]\n", (int)sizeof(playerInitialY));

  for (uint8_t i = 0; i < sizeof(monsterInitialX); ++i)
    printf("%d,", monsterInitialX[i]);
  printf(" // monsterInitialX[%d]\n", (int)sizeof(monsterInitialX));
  for (uint8_t i = 0; i < sizeof(monsterInitialY); ++i)
    printf("%d,", monsterInitialY[i]);
  printf(" // monsterInitialY[%d]\n", (int)sizeof(monsterInitialY));

  printf("%d, // treasureCount\n", treasureOffset);

  for (uint8_t i = 0; i < sizeof(treasureX); ++i)
    printf("%d,", treasureX[i]);
  printf(" // treasureX[%d]\n", (int)sizeof(treasureX));
  for (uint8_t i = 0; i < sizeof(treasureY); ++i)
    printf("%d,", treasureY[i]);
  printf(" // treasureY[%d]\n", (int)sizeof(treasureY));


 cleanup:
  png_free(png_ptr, row_pointers);
  png_free(png_ptr, data);

  png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
  fclose(fp);

  return errorVal;
}

int addDirectory(uint8_t *mfile, char *directory) {
  // Change into the directory specified as an argument
  if (chdir(directory) < 0) {
    perror("chdir");
    return -1;
  }

  DIR *dp = opendir(".");
  if (dp == NULL) {
    perror("opendir");
    return -1;
  }

  // Initialize the Red-Black Tree used for sorting the filenames
  dirent_node_t myNil;
  rbtree_node_t *myNilRef = (rbtree_node_t *)&myNil;
  rbtree_t tree;
  rbtree_init(&tree, myNilRef, sizeof(dirent_node_t), dirent_node_compare);

  struct stat sb;
  struct dirent *entry;
  while ((entry = readdir(dp)))
    if (stat(entry->d_name, &sb) < 0)
      perror("stat");
    else if ((sb.st_mode & S_IFMT) == S_IFREG)
      rbtree_setinsert(&tree, dirent_node_new(entry));

  closedir(dp);

  // Iterate through the sorted filenames
  for (rbtree_node_t *itr = rbtree_minimum(&tree);
       itr != myNilRef; itr = rbtree_successor(&tree, itr)) {
    printf("Adding '%s'...\n", ((dirent_node_t *)itr)->entry.d_name);
    addPNG(mfile, ((dirent_node_t *)itr)->entry.d_name);
  }

  // Cleanup the Red-Black Tree
  rbtree_postorderwalk(&tree, dirent_node_delete, 0);
  rbtree_destroy(&tree);

  /*
  // Scan the directory in alphabetical order
  struct dirent **namelist;
  int numEntries = scandir(".", &namelist, 0, alphasort);
  if (numEntries < 0) {
  perror("scandir");
  return -1;
  }

  // Loop over all the entries
  for (int i = 0; i < numEntries; i++) {
  struct stat sb;
  // Only interested in regular files
  if (stat(namelist[i]->d_name, &sb) < 0)
  perror("stat"); // print error and skip file
  else {
  if ((sb.st_mode & S_IFMT) == S_IFREG) {
  //printf("Adding '%s'...\n", namelist[i]->d_name);
  addPNG(mfile, namelist[i]->d_name);
  }
  }
  free(namelist[i]);
  }
  free(namelist);
  */

  return 0;
}

void printUsage(void) {
  fprintf(stderr, "\n"
          "NAME:\n"
          "\tpng2tlc - convert a directory of PNG files into a single TLC\n"
	  "\t          file that contains every image in that directory\n"
          "\n"
          "SYNOPSIS:\n"
          "\tpng2tlc [-h] [OPTIONS] [-d inputDirectory] [-o outputFileName]\n"
          "\n"
          "DESCRIPTION:\n"
          "\tThis program is designed to pack a directory of PNG files\n"
          "\t(with or without an alpha channel) into a format that is\n"
	  "\toptimized for sending directly to the TLC5940 chips on an\n"
	  "\tRGB-POV.\n"
          "\n"
          "OPTIONS:\n"
          "  -h\n"
          "\tDisplay this help and exit.\n"
          "\n"
          "  -b\n"
          "\tBreadboard mode. Instead of the LEDs being arranged\n"
          "\tsequentially, they are arranged in two halves with all of the\n"
          "\teven numbered outputs first,\n\tfollowed by all of the odd\n"
          "\tnumbered outputs. This makes it easier to fit them next to\n"
          "\teach other on a breadboard.\n"
          "\n"
          "\tDefault: %s\n"
          "\n"
          "  -p TLC5940_PWM_BITS\n"
          "\tNumber of PWM bits used to define a single PWM cycle.\n"
          "\n"
          "\tDefault: %d\n"
          "\n"
          "  -s outputFileSize\n"
          "\tSize (in bytes) of the output file to create or update.\n"
          "\n"
#ifdef __MINGW32__
          "\tDefault: %ul\n"
#else // __MINGW32__
	  "\tDefault: %zu\n"
#endif // __MINGW32__
          "\n"
          "  -e maxEntries\n"
          "\tThe maximum number of entries that can be stored in the output\n"
	  "\tfile.\n"
          "\n"
#ifdef __MINGW32__
          "\tDefault: %ul\n"
#else // __MINGW32__
	  "\tDefault: %zu\n"
#endif // __MINGW32__
          "\n"
          "  -d inputDirectory\n"
          "\tThe input directory that contains the PNG files to convert.\n"
          "\n"
          "\tDefault: %s\n"
          "\n"
          "  -o outputFileName\n"
          "\tThe full path to the output file name as it would exist on the\n"
	  "\tmicroSD card.\n"
          "\n"
          "\tDefault: %s\n"
          "\n"
          ,breadboardMode ? "TRUE" : "FALSE", TLC5940_PWM_BITS,
	  outputFileSize, maxEntries, inputDirectory, outputFileName);
}

int parseArguments(int *argc, char ***argv) {
  uint8_t error, showHelp;
  error = showHelp = 0;
  int opt;

  // Double-clicking on an executable on Linux or OS X does not set
  // the current working directory to the directory of the executable,
  // so if we create a file using a relative path, expecting it to be
  // relative to the executable, it will not work. This works around
  // the issue.
  if (*argc == 1) {
#if defined  __linux__
    readlink("/proc/self/exe", outputFileName, sizeof(outputFileName));
#elif defined __APPLE__
    uint32_t bufsize = sizeof(outputFileName);
    _NSGetExecutablePath(outputFileName, &bufsize);
#elif defined __MINGW32__
    GetModuleFileName(NULL, outputFileName, sizeof(outputFileName));
#else
    /* strncpy(outputFileName, DEFAULT_OUTPUT_FILENAME, */
    /* 	    sizeof(outputFileName) - 1); */
    /* strncpy(inputDirectory, DEFAULT_INPUT_DIRECTORY, */
    /* 	    sizeof(inputDirectory) - 1); */
    goto skip_dblclk;
#endif

    printf("outputFileName = %s\n", outputFileName);

    memcpy(inputDirectory, outputFileName, sizeof(inputDirectory));

#ifdef __MINGW32__
    char *pOut = strrchr(outputFileName, '\\');
    char *pIn = strrchr(inputDirectory, '\\');
#else
    char *pOut = strrchr(outputFileName, '/');
    char *pIn = strrchr(inputDirectory, '/');
#endif
    if (pOut++ == NULL) {
      fprintf(stderr, "Error determining path to executable\n");
      return -1;
    }
    if (pIn++ == NULL) {
      fprintf(stderr, "Error determining path to executable\n");
      return -1;
    }

    strncpy(pOut, DEFAULT_OUTPUT_FILENAME,
	    sizeof(outputFileName) - 1 - (pOut - outputFileName));
    strncpy(pIn, DEFAULT_INPUT_DIRECTORY,
	    sizeof(inputDirectory) - 1 - (pIn - inputDirectory));
  } else {
    goto skip_dblclk; // avoid warning when compiling on non-Linux/Mac/MinGW32
  skip_dblclk:
    strncpy(outputFileName, DEFAULT_OUTPUT_FILENAME,
	    sizeof(outputFileName) - 1);
    strncpy(inputDirectory, DEFAULT_INPUT_DIRECTORY,
	    sizeof(inputDirectory) - 1);
  }

  while (!error && (opt = getopt(*argc, *argv, "hbp:s:e:o:d:")) != -1) {
    switch (opt) {
    case 'h': // display help
      showHelp = 1;
      break;
    case 'b': // breadboard mode
      breadboardMode = 1;
      break;
    case 'p': // TLC5940_PWM_BITS
      {
        errno = 0;
        char *endptr = 0;
        long int val = strtol(optarg, &endptr, 10);
        if (errno != 0) {
          perror("strtol");
          error = 1;
          break;
        }
        if (val < 8 || val > 12 || !(optarg != '\0' && *endptr == '\0')) {
          fprintf(stderr, "Error: TLC5940_PWM_BITS invalid\n");
          error = 1;
          break;
        }
        TLC5940_PWM_BITS = (uint8_t)val;
        printf("Forcing TLC4950_PWM_BITS to be: %d\n", TLC5940_PWM_BITS);
      }
      break;
    case 's': // override default outputFileSize
    case 'e': // override default maxEntries
      {
        errno = 0;
        char *endptr = 0;
        long int val = strtol(optarg, &endptr, 10);
        if (errno != 0) {
          perror("strtol");
          error = 1;
          break;
        }
        if (val < 0 || !(optarg != '\0' && *endptr == '\0')) {
          fprintf(stderr, "Error: %s invalid\n",
                  (opt == 's') ? "outputFileSize" : "maxEntries");
          error = 1;
          break;
        }
        if (opt == 's') {
          outputFileSize = val;
#ifdef __MINGW32__
          printf("Forcing outputFileSize to be: %ul\n", outputFileSize);
#else // __MINGW32__
          printf("Forcing outputFileSize to be: %zu\n", outputFileSize);
#endif // __MINGW32__
        } else { // opt == 'e'
          maxEntries = val;
#ifdef __MINGW32__
          printf("Forcing maxEntries to be: %ul\n", maxEntries);
#else // __MINGW32__
          printf("Forcing maxEntries to be: %zu\n", maxEntries);
#endif // __MINGW32__
        }
      }
      break;
    case 'o': // override default outputFileName
      if (strlen(optarg) < sizeof(outputFileName)) {
        strncpy(outputFileName, optarg, sizeof(outputFileName) - 1);
        outputFileName[sizeof(outputFileName) - 1] = '\0';
      } else {
        fprintf(stderr, "Error: outputFileName too long\n");
        error = 1;
      }
      break;
    case 'd': // override default inputDirctory
      if (strlen(optarg) < sizeof(inputDirectory)) {
        strncpy(inputDirectory, optarg, sizeof(inputDirectory) - 1);
        inputDirectory[sizeof(inputDirectory) - 1] = '\0';
      } else {
        fprintf(stderr, "Error: inputDirectory too long\n");
        error = 1;
      }
      break;
    default:
      error = 1;
      break;
    }
  }

  if (showHelp) {
    printUsage();
    return 1;
  }

  // Check for extraneous arguments
  if (optind < *argc) {
    fprintf(stderr, "Error: too many arguments\n");
    error = 1;
  }

  return (error) ? -1 : 0;
}

int main(int argc, char *argv[]) {
  int retVal = 0;

  printf("For usage information run: ./png2tlc -h\n");

  // Parse command line arguments
  retVal = parseArguments(&argc, &argv);
  if (retVal != 0)
    return retVal;

  int fd;
  uint8_t *mfile;

  // Does the file already exist?
  struct stat sb;
  int st = stat(outputFileName, &sb);
  if (st == 0 && (sb.st_mode & S_IFMT) == S_IFREG && sb.st_size != 0) {
    printf("File exists, opening...\n");
    // Open existing file
    fd = open(outputFileName, O_RDWR);
    if (fd < 0) {
      perror("open");
      return -1;
    }
  } else if (st == -1 && errno == ENOENT) { // File does not exist
    printf("File does not exist, creating...\n");
    // Create an empty file
    fd = createEmptyFile(outputFileName, outputFileSize);
    if (fd < 0) {
      fprintf(stderr, "Unable to create file\n");
      return -1;
    }

#ifdef __MINGW32__
    // This shouldn't be necessary, but it will crash otherwise
    close(fd);
    fd = open(outputFileName, O_RDWR);
    if (fd < 0) {
      perror("open");
      return -1;
    }
#endif

    // Update the file size of the newly created file
    st = stat(outputFileName, &sb);
    if (st < 0) {
      perror("stat");
      retVal = -1;
      goto err_stat;
    }
  } else {
    fprintf(stderr, "File exists, but of unexpected type or invalid size\n");
    return -1;
  }

  // map the file into memory
  printf("Mapping the file into memory...\n");

#ifdef __MINGW32__
  HANDLE hFile = (HANDLE)_get_osfhandle(fd);
  HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE,
					  0, 0, NULL);
  if (hFileMapping == NULL) {
    perror("CreateFileMapping");
    retVal = -1;
    goto err_mmap;
  }

  mfile = (PBYTE)MapViewOfFile(hFileMapping, FILE_MAP_WRITE,
			       0, 0, sb.st_size);
  if (mfile == NULL) {
    perror("MapViewOfFile");
    retVal = -1;
    goto err_mmap;
  }

#else // __MINGW32__
  mfile = (uint8_t*)mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
			 fd, 0);
  if (mfile == MAP_FAILED) {
    perror("mmap");
    retVal = -1;
    goto err_mmap;
  }
#endif // __MINGW32__

  outputFileSize = sb.st_size;

  // Now we can write to mfile[] as if it were an array

  // Start initializing the data structures inside the file
  if (initializeMappedFile(mfile, sb.st_size / (1 << BLOCK_LEN)) < 0) {
    retVal = -1;
    goto err_init;
  }

#ifdef __MINGW32__
  CreateDirectory(inputDirectory, NULL);
#else // __MINGW32__
  /* Create 'PNGs' directory if it doesn't exist */ {
    struct stat sb;
    int st = stat(inputDirectory, &sb);
    // If something else already exists with the same name as inputDirectory
    // try to create it anyway so the proper error gets printed by mkdir.
    if (st == -1 && errno == ENOENT && mkdir(inputDirectory, S_IRWXU) < 0) {
      perror("mkdir");
      retVal = -1;
      goto err_mkdir;
    } else if ((st == 0 && (sb.st_mode & S_IFMT) != S_IFDIR) &&
	       (mkdir(inputDirectory, S_IRWXU) < 0)) {
      perror("mkdir");
      fprintf(stderr, "Error: Unable to create inputDirectory\n");
      retVal = -1;
      goto err_mkdir;
    }
  }
#endif // __MINGW32__

  // Build the TLC5940 gamma correction table
  TLC5940_InitGamma(TLC5940_PWM_BITS); // TLC5940_PWM_BITS

  // Add all PNG files from the directory
  addDirectory(mfile, inputDirectory);

 err_init:
  goto err_mkdir; // remove unused label warning when compiling on MINGW32
 err_mkdir:
  // synchronize and unmap the memory mapped file
#ifdef __MINGW32__
  /*  if (FlushViewOfFile(mfile, sb.st_size) == 0) {
      perror("FlushViewOfFile");
      retVal = -1;
      }*/

  if (UnmapViewOfFile(mfile) == 0) {
    perror("UnmapViewOfFile");
    retVal = -1;
    goto err_munmap;
  }

  CloseHandle(hFileMapping);
#else // __MINGW32__
  if (msync(mfile, sb.st_size, MS_SYNC) < 0) {
    perror("msync");
    retVal = -1;
    // If msync fails, still munmap and close the file
  }

  if (munmap(mfile, sb.st_size) < 0) {
    perror("munmap");
    retVal = -1;
    goto err_munmap;
  }
#endif // __MINGW32__

 err_mmap:
 err_munmap:
 err_stat:
  // close the file
  if (close(fd) < 0) {
    perror("close");
    retVal = -1;
  }

  return retVal;
}
