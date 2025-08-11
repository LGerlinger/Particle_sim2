CXX=g++
CXXFLAGS=-I. -Iinclude -O3 -flto
#CPPFLAGS=-std=c++11
SFML_LIB=-lsfml-graphics -lsfml-window -lsfml-system

WARNING := -Wextra# no -Wall

SOURCES := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SOURCES))
DEPS := $(patsubst src/%.cpp,build/%.d,$(SOURCES))

.PHONY: all clean

all : build_dir particle_sim2

build_dir:
	mkdir -p build


-include $(DEPS)

build/%.o: src/%.cpp include/%.hpp
	$(CXX) $(WARNING) -MMD -MP -c -o $@ $< $(CXXFLAGS)

build/main.o: src/main.cpp
	$(CXX) $(WARNING) -MMD -MP -c -o $@ $< $(CXXFLAGS)

particle_sim2: $(OBJ)
	$(CXX) $(WARNING) -o $@ $^ $(SFML_LIB) $(CXXFLAGS)

clean:
	rm build/*.o build/*.d

again: clean all

sure: all
	clear;
	./particle_sim2

clean_save :
	rm saves/Map/* saves/PSparameters/* saves/Positions/*