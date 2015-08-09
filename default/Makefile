###############################################################################
# Makefile for the Generating Tiles and Maps with gconvert tutorial
###############################################################################

## General Flags
PROJECT = Physics
GAME= physics
MCU = atmega644
TARGET = $(GAME).elf
CC = avr-gcc
INFO=../gameinfo.properties
UZEBIN_DIR=../../../bin

## Kernel settings
KERNEL_DIR = ../../../kernel
KERNEL_OPTIONS  = -DVIDEO_MODE=3 -DINTRO_LOGO=0 -DSCROLLING=0 -DSOUND_MIXER=1
KERNEL_OPTIONS += -DMAX_SPRITES=8 -DRAM_TILES_COUNT=32 -DSCREEN_TILES_V=28
# -DSOUND_CHANNEL_4_ENABLE=0
# -DSCREEN_TILES_V=28
# -DSCREEN_TILES_V=26 -DFIRST_RENDER_LINE=28
#-DSOUND_CHANNEL_4_ENABLE=0 

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units. (-O3 is necessary to support 6 simultaneous monsters)
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99 -DF_CPU=28636360UL -O2 -fsigned-char -ffunction-sections
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d
CFLAGS += $(KERNEL_OPTIONS)


## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS += -Wl,-Map=$(GAME).map 
LDFLAGS += -Wl,-gc-sections

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Objects that must be built in order to link
OBJECTS = uzeboxVideoEngineCore.o uzeboxCore.o uzeboxSoundEngine.o uzeboxSoundEngineCore.o uzeboxVideoEngine.o $(GAME).o entity.o stackmon.o

## Objects explicitly added by the user
LINKONLYOBJECTS =

## Include Directories
INCLUDES = -I"$(KERNEL_DIR)"

## Build
all: $(TARGET) $(GAME).hex $(GAME).eep $(GAME).lss $(GAME).uze

## Compile Kernel files
uzeboxVideoEngineCore.o: $(KERNEL_DIR)/uzeboxVideoEngineCore.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

uzeboxSoundEngineCore.o: $(KERNEL_DIR)/uzeboxSoundEngineCore.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

uzeboxCore.o: $(KERNEL_DIR)/uzeboxCore.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

uzeboxSoundEngine.o: $(KERNEL_DIR)/uzeboxSoundEngine.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

uzeboxVideoEngine.o: $(KERNEL_DIR)/uzeboxVideoEngine.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

## Compile game sources
$(GAME).o: ../physics.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

entity.o: ../entity.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

stackmon.o: ../stackmon.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@
	avr-size -A --format=avr --mcu=$(DEVICE) $^

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

%.uze: $(TARGET)
	-$(UZEBIN_DIR)/packrom $(GAME).hex $@ $(INFO)

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) $(GAME).eep $(GAME).elf $(GAME).hex $(GAME).lss $(GAME).map $(GAME).o $(GAME).uze dep/*


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)
