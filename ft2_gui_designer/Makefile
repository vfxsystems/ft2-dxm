CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I../src
LIBS = -lSDL2 -lGL -lGLU -lm

TARGET = ft2_gui_designer
SOURCES = main.c palette.c canvas.c widgets.c font.c assets.c stb_image.c \
	$(wildcard ../src/gfxdata/*.c) \
	../src/shared/ft2_ui_assets.c \
	../src/shared/ft2_ui_widget_registry.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install:
	sudo apt-get update
	sudo apt-get install libsdl2-dev

# Dependencies
main.o: main.c widgets.h canvas.h palette.h font.h assets.h
palette.o: palette.c palette.h
canvas.o: canvas.c canvas.h palette.h
widgets.o: widgets.c widgets.h palette.h assets.h
font.o: font.c font.h
assets.o: assets.c assets.h palette.h
stb_image.o: stb_image.c

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.SUFFIXES: .c .o
