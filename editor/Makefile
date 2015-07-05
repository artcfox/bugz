#
#  Makefile
#
#  Copyright 2011-2012 Matthew T. Pandina. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  1. Redistributions of source code must retain the above copyright
#  notice, this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright
#  notice, this list of conditions and the following disclaimer in the
#  documentation and/or other materials provided with the distribution.
#
#  THIS SOFTWARE IS PROVIDED BY MATTHEW T. PANDINA "AS IS" AND ANY
#  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHEW T. PANDINA OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
#  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
#  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGE.

# Compile the code as C99
CC=gcc
CFLAGS=-Wall -std=gnu99 -g -c -Ilibpng

# Compile the code as C++
#CC=g++
#CFLAGS=-Wall -O3 -c

# Dynamically link the entire exectuable
#LDFLAGS=-lpng -lz -lm

# Statically link to the local libpng and zlib that was compiled from source
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
LDFLAGS=-DPNG_STATIC -DZLIB_STATIC libpng/libpng.a zlib/libz.a -lm
else
LDFLAGS=-DPNG_STATIC -DZLIB_STATIC -static libpng/libpng.a zlib/libz.a -lm
endif

# Statically link the entire executable
#LDFLAGS=-DPNG_STATIC -DZLIB_STATIC -static -lpng -lz -lm

SOURCES=png2inc.c rbtree/rbtree.c rbtree/rbtree+setinsert.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=png2inc

all: $(SOURCES) $(EXECUTABLE)

clean:
	rm -rf $(EXECUTABLE) $(OBJECTS)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@
