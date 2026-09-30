# Pakai di terminal MSYS2 UCRT64:  make        (build)
#                                   make clean  (hapus hasil build)
# File yang tidak berubah tidak dikompilasi ulang.
CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
SRC      = $(wildcard src/*.cpp)
OBJ      = $(SRC:src/%.cpp=build/%.o)

rag.exe: $(OBJ)
	$(CXX) $(OBJ) -o $@ -lsqlite3

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build rag.exe
