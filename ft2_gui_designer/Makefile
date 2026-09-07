CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I../src
LIBS = -lSDL2 -lGL -lGLU -lm

TARGET = ft2_gui_designer
TEST_TARGET = ft2_gui_bitmap_tests
SOURCES = main.c palette.c canvas.c widgets.c font.c assets.c stb_image.c \
	$(wildcard ../src/gfxdata/*.c) \
	../src/shared/ft2_ui_assets.c \
	../src/shared/ft2_ui_bitmap.c \
	../src/shared/ft2_ui_widget_registry.c
OBJECTS = $(SOURCES:.c=.o)
TEST_SOURCES = bitmap_pipeline_tests.c palette.c assets.c stb_image.c \
	$(wildcard ../src/gfxdata/*.c) \
	../src/shared/ft2_ui_assets.c \
	../src/shared/ft2_ui_bitmap.c
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) -o $(TEST_TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TARGET) $(TEST_TARGET)

test: $(TARGET) $(TEST_TARGET)
	./$(TARGET) --validate test_design.gui layouts/*.gui
	./$(TEST_TARGET)

install:
	sudo apt-get update
	sudo apt-get install libsdl2-dev

# Dependencies
main.o: main.c widgets.h canvas.h palette.h font.h assets.h
palette.o: palette.c palette.h
canvas.o: canvas.c canvas.h palette.h
widgets.o: widgets.c widgets.h palette.h assets.h ../src/shared/ft2_ui_bitmap.h
font.o: font.c font.h
assets.o: assets.c assets.h palette.h ../src/shared/ft2_ui_bitmap.h
../src/shared/ft2_ui_bitmap.o: ../src/shared/ft2_ui_bitmap.c ../src/shared/ft2_ui_bitmap.h
stb_image.o: stb_image.c
bitmap_pipeline_tests.o: bitmap_pipeline_tests.c assets.h ../src/shared/ft2_ui_bitmap.h

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.SUFFIXES: .c .o
