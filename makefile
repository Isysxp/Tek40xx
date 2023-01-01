CC=gcc
LIBS = `sdl2-config --libs` -lm -Wl,--allow-multiple-definition
CFLAGS =  -DHAVE_LIBSDL -DSDL_MAIN_AVAILABLE `sdl2-config --cflags` -Ofast -g -Wall
SRC = tek_main.c tek_display.c tek_telnet.c tek_drawline.c font-5x7.c
OBJ = tek_main.o tek_display.o tek_telnet.o tek_drawline.o font-5x7.o
all: tek40xx

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

tek40xx: $(OBJ)
	 $(CC) -o $@ $^ $(LIBS)

clean:	
	rm *.o
	rm tek40xx

