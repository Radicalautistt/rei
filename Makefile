rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

includePaths = -isystem third-party/
linkPaths = -L third-party/lz4/lib/
linkPaths += -L third-party/imgui/
linkPaths += -L third-party/simdjson/
links = -ldl -lm -lxcb -llz4 -lIMGUI -lSimdjson

flags = -std=c++17 -Wall -Wextra -Wpadded -Wconversion -Og -march=native -Wno-missing-field-initializers -g
sourceFiles = $(call rwildcard, src, *.cpp, *.hpp, *.h)
objectFiles = $(patsubst src/%.cpp, obj/%.o, $(sourceFiles))

playground: $(objectFiles)
	g++ -o $@ $^ $(linkPaths) $(links)

obj/%.o: src/%.cpp
	g++ $(flags) $(includePaths) -c -o $@ $<

run: playground
	./playground

clean:
	rm -r obj/
