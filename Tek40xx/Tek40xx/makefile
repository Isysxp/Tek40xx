CC=gcc
LIBS = `sdl2-config --libs` -lm
CFLAGS =  -DHAVE_LIBSDL -DSDL_MAIN_AVAILABLE `sdl2-config --cflags` -Ofast -Wall
SRC = tek_main.c tek_display.c tek_telnet.c tek_drawline.c font-5x8.c font-7x9.c font-12x16.c font-5x7.c
OBJ = tek_main.o tek_display.o tek_telnet.o tek_drawline.o font-5x8.o font-7x9.o font-12x16.o font-5x7.o
all: tek4010

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

tek4010: $(OBJ)
	 $(CC) -o $@ $^ $(LIBS)

clean:	
	rm *.o
	rm tek4010