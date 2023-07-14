CFLAGS := `sdl2-config --libs --cflags` -ggdb3 -O0 -Wall -lSDL2_image -lm -I../z80ex/include -DZ80EX_API_REVISION=1 \
-DZ80EX_VERSION_MAJOR=1 -DZ80EX_VERSION_MINOR=19 -lncurses

SRCS := dgemu.cpp ../z80ex/z80ex.c

OBJS := dgemu.o ../z80ex/z80ex.o

.c.o:
	gcc $(CFLAGS) -c $< -o $*.o

.cpp.o:
	g++ $(CFLAGS) -c $< -o $*.o

sim: $(OBJS) Makefile
	g++ -o sim $(OBJS) $(CFLAGS)