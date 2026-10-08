CXX      = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Isrc -Isrc/cli
SRC      = $(wildcard src/*.cpp) $(wildcard src/cli/*.cpp) $(wildcard src/cli/kontrol/*.cpp) $(wildcard src/crud/*.cpp) $(wildcard src/cache/*/*.cpp) $(wildcard src/katalog/*.cpp) $(wildcard src/alur/*.cpp)
OBJ      = $(SRC:src/%.cpp=build/%.o)

rag.exe: $(OBJ)
	$(CXX) $(OBJ) -o $@ -lsqlite3

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build rag.exe