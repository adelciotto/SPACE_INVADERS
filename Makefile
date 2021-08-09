# Windows, TODO!
ifeq ($(OS), Windows_NT)
else
	uname_s := $(shell uname -s)

	# OSX
	ifeq ($(uname_s), Darwin)
		# External library headers
		sdl_dev_inc := ./external/osx/Library/Frameworks/SDL2.framework/Headers
		sdlmix_dev_inc := ./external/osx/Library/Frameworks/SDL2_mixer.framework/Headers

		# Target executable
		target := space_invaders

		# Library flags
		lib_flags := -Wl,-rpath,@executable_path/./Library/Frameworks -F./external/osx/Library/Frameworks \
					 -framework OpenGL -framework SDL2 -framework SDL2_mixer

		# Distribution
		dist_name := space_invaders-osx
		dist_deps := ./external/osx/Library
	# Linux, TODO!
	else ifeq ($(uname_s, Linux))
	endif
endif

# Source files
src_dirs := code
srcs := $(shell find $(src_dirs) -name *.cpp -or -name *.c)

# Object files
objs := $(srcs:%=%.o)

# Depend files
deps := $(objs:.o=.d)

# Distribution related directories
data_dir := data
dist_dir := dist

# Compiler
cc := g++

# Include flags
incflags := -I$(sdl_dev_inc) -I$(sdlmix_dev_inc) -I$(src_dirs) -I$(src_dirs)/lib/glad/include

# Depend flags
depflags := -MMD -MP

# Compile flags
cxxflags := -std=c++11 -Wall -Wshadow -Wstrict-aliasing -Wstrict-overflow $(incflags) $(depflags) -DIMGUI_IMPL_OPENGL_LOADER_GLAD

# Debug build settings
dbg_dir := debug
dbg_target := $(dbg_dir)/$(target)
dbg_objs := $(addprefix $(dbg_dir)/obj/, $(objs))
dbg_cxxflags := -g -O0 -DDEBUG

# Release build settings
rel_dir := release
rel_target := $(rel_dir)/$(target)
rel_objs := $(addprefix $(rel_dir)/obj/, $(objs))
rel_cxxflags := -O3 -DNDEBUG

.PHONY: all clean debug release valgrind

# Default build
all: release

# Debug rules
debug: $(dbg_target)
	cp -r $(dist_deps) $(data_dir) $(dbg_dir)

$(dbg_target): $(dbg_objs)
	$(cc) $(dbg_objs) -o $@ $(lib_flags)

$(dbg_dir)/obj/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(cc) $(cxxflags) $(dbg_cxxflags) -c $< -o $@
$(dbg_dir)/obj/%.c.o: %.c
	mkdir -p $(dir $@)
	$(cc) $(cxxflags) $(dbg_cxxflags) -c $< -o $@

# Release rules
release: $(rel_target)
	cp -r $(dist_deps) $(data_dir) $(rel_dir)

$(rel_target): $(rel_objs)
	$(cc) $(rel_objs) -o $@ $(lib_flags)

$(rel_dir)/obj/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(cc) $(cxxflags) $(rel_cxxflags) -c $< -o $@
$(rel_dir)/obj/%.c.o: %.c
	mkdir -p $(dir $@)
	$(cc) $(cxxflags) $(rel_cxxflags) -c $< -o $@

# Other rules
clean:
	rm -rf $(rel_dir) $(dbg_dir)

# Include the .d makefiles.
-include $(deps)
