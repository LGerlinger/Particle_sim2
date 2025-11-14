# Detect OS
UNAME := $(shell uname)
ifeq ($(OS),Windows_NT)
	IS_WINDOWS := 1
endif

# Set compiler and flags
CXX = g++
CXXFLAGS = -I. -Iinclude -O3 -flto
CPPFLAGS = -std=c++17
WARNING := -Wextra # -Wall

# Default: OpenGL is enabled
USE_OPENGL := 1

# Add USE_OPENGL to CPPFLAGS if needed
CPPFLAGS += -DUSE_OPENGL=$(USE_OPENGL)

# Set SFML and OpenGL libraries based on OS
# I am not sure which libraries have to be included to make openGL work on windows and I can't test it :/
ifeq ($(USE_OPENGL),1)
	ifeq ($(IS_WINDOWS),1)
		LIBS = -lsfml-graphics -lsfml-window -lsfml-system -lglew32 -lopengl32 -lWs2_32 -lole32 -lcomctl32 -lgdi32 -lcomdlg32 -luuid
	else
		LIBS = -lsfml-graphics -lsfml-window -lsfml-system -lGL -lGLEW
	endif
else
	LIBS = -lsfml-graphics -lsfml-window -lsfml-system
endif


SOURCES := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SOURCES))
DEPS := $(patsubst src/%.cpp,build/%.d,$(SOURCES))


all: build_dir particle_sim2

.PHONY: all clean

build_dir:
	mkdir -p build

again: clean all

sure: all
	clear;
	./particle_sim2

nopengl: USE_OPENGL=0
nopengl: all


-include $(DEPS)

build/%.o: src/%.cpp include/%.hpp
	$(CXX) $(WARNING) -MMD -MP -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

build/main.o: src/main.cpp
	$(CXX) $(WARNING) -MMD -MP -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

particle_sim2: $(OBJ)
	$(CXX) $(WARNING) -o $@ $^ $(CPPFLAGS) $(CXXFLAGS) $(LIBS)



clean:
	rm -rf build/*.o build/*.d

clean_save :
	rm -f saves/Map/* saves/PSparameters/* saves/Positions/*