
SOURCES:=error.c  main.c  noise.c utils.c  world.c
HEADERS:=bool.h error.h  math.h  noise.h  utils.h  world.h

PACKAGES:=sdl glew

LIBS:=-lGL -lm -lSDL_image -lpthread
CFLAGS:=-std=gnu99 -Wall -march=native -ffast-math -g

CC:=gcc-4.6
#CC:=llvm-clang

main: $(SOURCES) $(HEADERS) Makefile
		$(CC) `pkg-config --libs --cflags $(PACKAGES)` $(CFLAGS) $(LIBS) $(SOURCES) -o main

run: main
		./main

debug: main
		gdb ./main --eval-command="run"
