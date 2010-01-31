ifeq ($(DEVICE),pre)
	MARCH_TUNE	=	-march=armv7-a -mtune=cortex-a8
	SUFFIX		=	-armv7
else
ifeq ($(DEVICE),pixi)
	MARCH_TUNE	=	-march=armv6j -mtune=arm1136j-s
	SUFFIX		=	-armv6
else
ifeq ($(DEVICE),emu)
	SUFFIX		=	-i686
endif
endif
endif

VERSION = 0.0.1
CC = gcc
INCLUDES	=	-I. \
					-I/usr/local/include/glib-2.0 \
					-I/usr/local/lib/glib-2.0/include \
					-I/usr/local/include/lunaservice \
					-I/usr/local/include/mjson
					
CFLAGS = -Os -g $(MARCH_TUNE) -DVERSION=\"$(VERSION)\"
LDFLAGS = -Wl,-rpath,/usr/local/lib -L/usr/local/lib
LIBS = $(LDFLAGS) -lglib-2.0 -llunaservice -lpng

PROGRAM_BASE	=	org.webosinternals.thmapi

PROGRAM			= 	$(PROGRAM_BASE)$(SUFFIX)

OBJECTS			= 	theme_manager.o

.PHONY			: 	clean-objects clean


all: $(PROGRAM)

fresh: clean all

$(PROGRAM): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(ARCHIVES) -o $(PROGRAM) $(INCLUDES) $(LIBS)

$(OBJECTS): %.o: %.c
	$(CC) $(CFLAGS) -c $<  -o $@ -I. $(INCLUDES) $(LIBS)
	
clean-objects:
	rm -rf $(OBJECTS)
	
clean: clean-objects
	rm -rf $(PROGRAM_BASE)*
