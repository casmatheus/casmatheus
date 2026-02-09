MODE ?= debug
SRC_DIR := src
OBJ_BASE_DIR := objs
OBJ_DIR := $(OBJ_BASE_DIR)/$(MODE)
WEBSITE_DIR := website

ifeq ($(MODE), release)
  OUTPUT_ROOT := $(WEBSITE_DIR)
  GAMES_DIR   := $(WEBSITE_DIR)/games
else
  OUTPUT_ROOT := bin
  GAMES_DIR   := bin/games
endif

ASSETS_SRC := assets/cooked
ASSETS_DEST := $(OUTPUT_ROOT)/Assets

WARNINGS := -Wno-gnu-zero-variadic-macro-arguments \
            -Wno-language-extension-token -Wno-nested-anon-types \
						-Wno-dollar-in-identifier-extension \
            -Wall -Wextra -pedantic

INCLUDES := -I. -Isrc -Ilibs -Ilibs/glm -Ilibs/stb
CXXFLAGS := -std=c++17 -fno-exceptions -fno-rtti $(INCLUDES) $(WARNINGS)

EXE_NAME  := $(BIN_DIR)/index.html
CXX       := emcc 

CXXFLAGS += --use-port=emdawnwebgpu
LDFLAGS  := --shell-file shell.html \
            -s WASM=1 \
          	-s ALLOW_MEMORY_GROWTH=1 \
          	-s NO_EXIT_RUNTIME=1 \
						--use-port=emdawnwebgpu \
						-lwebsocket.js \
						-s FETCH=1 \
						-s INITIAL_MEMORY=2097152 \
						-s STACK_SIZE=262144 \
						-s MALLOC=emmalloc \
						-s EXPORTED_FUNCTIONS="['_main']" \
						-s EXPORTED_RUNTIME_METHODS="['UTF8ToString', 'HEAPU8', 'HEAP32']"

ifeq ($(MODE), debug)
  CXXFLAGS += -g3 -DDEBUG
  LDFLAGS  += -g3 -s ASSERTIONS=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 
else ifeq ($(MODE), release)
  CXXFLAGS += -Oz
  LDFLAGS  += -Oz -flto -s ASSERTIONS=0 -s ENVIRONMENT=web -s FILESYSTEM=0 \
              -s MINIFY_HTML=1 \
              -s DISABLE_EXCEPTION_CATCHING=1
endif

ENGINE_SRCS := $(shell find src -mindepth 2 -name "*.cpp")
ENGINE_OBJS := $(ENGINE_SRCS:%.cpp=$(OBJ_DIR)/%.o)

GAME_SRCS := $(shell find src -maxdepth 1 -name "*.cpp")
GAMES := $(patsubst src/%.cpp,%,$(GAME_SRCS))
GAME_TARGETS := $(foreach g,$(GAMES),$(GAMES_DIR)/$g/index.html)

ASSET_SRCS := $(shell if [ -d "$(ASSETS_SRC_DIR)" ]; then find $(ASSETS_SRC_DIR) -type f; fi)
ASSET_DESTS := $(patsubst $(ASSETS_SRC_DIR)/%,$(ASSETS_DEST_DIR)/%,$(ASSET_SRCS))

.PHONY: all clean run assets

all: $(GAME_TARGETS) assets

assets: $(ASSET_DESTS)
$(ASSETS_DEST_DIR)/%: $(ASSETS_SRC_DIR)/%
	@echo [COPY] $<
	@mkdir -p $(@D)
	@cp $< $@

$(GAMES_DIR)/%/index.html: src/%.cpp $(ENGINE_OBJS) $(SHELL_FILE)
	@echo [LINK] $* "($(MODE))"
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) $< $(ENGINE_OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@echo [CXX] $<
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	@echo [CLEAN] Removing builds...
	@rm -rf $(OBJ_BASE_DIR)
	@rm -rf bin
	@rm -rf $(WEBSITE_DIR)/games
	@rm -rf $(OBJ_BASE_DIR) $(BIN_DIR)

run: all
ifeq ($(MODE), release)
	@python -m http.server 8000 --directory $(WEBSITE_DIR)
else
	@python -m http.server 8000 --directory $(OUTPUT_ROOT)
endif

-include $(ENGINE_OBJS:.o=.d)
