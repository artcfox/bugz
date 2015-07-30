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


#define FILENAME_LEN 256
#define DEFAULT_OUTPUT_FILENAME "RGB-POV.TLC"
char outputFileName[FILENAME_LEN] = {0};

#define DEFAULT_INPUT_DIRECTORY "levels"
char inputDirectory[FILENAME_LEN] = {0};

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

// Given an x and a y in the range of 0-31, packs the coordinate into a byte array at logical index 'position'
void Pack5BitXY(uint8_t *xyData, uint8_t position, uint8_t x, uint8_t y)
{
  // Truncate x and y to the range 0-31, then pack them into 10 bits
  uint16_t value = ((x & 31) << 5) | (y & 31);
  uint16_t i = position * 5 / 4; // = position * 10 / 8, 10 bits packed into 8
  switch (position % 4) {
  case 0:
    xyData[i] = (value >> 2);                               // bits: 9 8 7 6 5 4 3 2
    ++i;
    xyData[i] = (xyData[i] & 0x3F) | (uint8_t)(value << 6); // bits: 1 0 x x x x x x
    break;
  case 1:
    xyData[i] = (xyData[i] & 0xC0) | (value >> 4);          // bits: x x 9 8 7 6 5 4
    ++i;
    xyData[i] = (xyData[i] & 0x0F) | (uint8_t)(value << 4); // bits: 3 2 1 0 x x x x
    break;
  case 2:
    xyData[i] = (xyData[i] & 0xF0) | (value >> 6);          // bits: x x x x 9 8 7 6
    ++i;
    xyData[i] = (xyData[i] & 0x03) | (uint8_t)(value << 2); // bits: 5 4 3 2 1 0 x x
    break;
  default: // case 3
    xyData[i] = (xyData[i] & 0xFC) | (value >> 8);          // bits: x x x x x x 9 8
    ++i;
    xyData[i] = (uint8_t)value;                             // bits: 7 6 5 4 3 2 1 0
    break;
  }
}

int addPNG(char *pngfile) {
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

  char includeFile[FILENAME_LEN] = {0};

#ifdef __MINGW32__
  sprintf(includeFile, "%s\\%s.inc", inputDirectory, pngfile);
#else
  sprintf(includeFile, "%s/%s.inc", inputDirectory, pngfile);
#endif

  FILE* fpInc = fopen(includeFile, "w");
  if (!fpInc) {
    fprintf(stderr, "Unable to open \"%s\" for writing.", includeFile);
    return -1;
  }

  fprintf(fpInc, "  ");
  for (uint8_t i = 0; i < sizeof(map); ++i)
    fprintf(fpInc, "%d,", map[i]);
  fprintf(fpInc, " // input file: %s\n", pngfile);

  fprintf(fpInc, "  ");
  for (uint8_t i = 0; i < sizeof(playerInitialX); ++i)
    fprintf(fpInc, "%d,", playerInitialX[i]);
  fprintf(fpInc, " // playerInitialX[%d]\n", (int)sizeof(playerInitialX));

  fprintf(fpInc, "  ");
  for (uint8_t i = 0; i < sizeof(playerInitialY); ++i)
    fprintf(fpInc, "%d,", playerInitialY[i]);
  fprintf(fpInc, " // playerInitialY[%d]\n", (int)sizeof(playerInitialY));


  /*
    So, I have two arrays of length 32, and each value is within the range of 0-31, so I only need 5 bits to store each element.

    Packed into bytes, the data looks like this:

    01234567
    --------
    01234012
    34012340
    12340123
    40123401
    23401234

    01234567
    89012345
    67890123
    45678901
    23456789

    So I should have a header: uint8_t players, uint8_t monsters, uint8_t treasures
   */


  fprintf(fpInc, "  ");
  for (uint8_t i = 0; i < sizeof(monsterInitialX); ++i)
    fprintf(fpInc, "%d,", monsterInitialX[i]);
  fprintf(fpInc, " // monsterInitialX[%d]\n", (int)sizeof(monsterInitialX));

  fprintf(fpInc, "  ");
  for (uint8_t i = 0; i < sizeof(monsterInitialY); ++i)
    fprintf(fpInc, "%d,", monsterInitialY[i]);
  fprintf(fpInc, " // monsterInitialY[%d]\n", (int)sizeof(monsterInitialY));




  fprintf(fpInc, "  ");
  fprintf(fpInc, "%d, // treasureCount\n", treasureOffset);

  fprintf(fpInc, "  ");
  for (uint8_t i = 0; i < sizeof(treasureX); ++i)
    fprintf(fpInc, "%d,", treasureX[i]);
  fprintf(fpInc, " // treasureX[%d]\n", (int)sizeof(treasureX));

  fprintf(fpInc, "  ");
  for (uint8_t i = 0; i < sizeof(treasureY); ++i)
    fprintf(fpInc, "%d,", treasureY[i]);
  fprintf(fpInc, " // treasureY[%d]\n", (int)sizeof(treasureY));

  fclose(fpInc);

 cleanup:
  png_free(png_ptr, row_pointers);
  png_free(png_ptr, data);

  png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
  fclose(fp);

  return errorVal;
}

int addDirectory(char *directory) {
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
    addPNG(((dirent_node_t *)itr)->entry.d_name);
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
          "\tpng2inc - convert a directory of PNG files into .inc files\n"
	  "\t          to #include in the definition of a PROGMEM array."
          "\n"
          "SYNOPSIS:\n"
          "\tpng2inc [-h] [OPTIONS] [-d inputDirectory]\n"
          "\n"
          "DESCRIPTION:\n"
          "\tThis program is designed to convert a directory of PNG files\n"
          "\t(with or without an alpha channel) into a format that is\n"
	  "\tsuitable for #including directly in the definition of a\n"
	  "\tPROGMEM array for the UZEBOX gaming system.\n"
          "\n"
          "OPTIONS:\n"
          "  -h\n"
          "\tDisplay this help and exit.\n"
          "\n"
          "  -d inputDirectory\n"
          "\tThe input directory that contains the PNG files to convert.\n"
          "\n"
          "\tDefault: %s\n"
          "\n"
          ,inputDirectory);
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

    //printf("outputFileName = %s\n", outputFileName);

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

  while (!error && (opt = getopt(*argc, *argv, "hd:")) != -1) {
    switch (opt) {
    case 'h': // display help
      showHelp = 1;
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

  printf("For usage information run: %s -h\n", argv[0]);

  // Parse command line arguments
  retVal = parseArguments(&argc, &argv);
  if (retVal != 0)
    return retVal;

  int fd = 0;

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

  printf("INPUT DIRECTORY \"%s\" \n", inputDirectory);

  // Add all PNG files from the directory
  addDirectory(inputDirectory);

 err_init:
  goto err_mkdir; // remove unused label warning when compiling on MINGW32
 err_mkdir:
 err_stat:
  // close the file
  if (close(fd) < 0) {
    perror("close");
    retVal = -1;
  }

  return retVal;
}
