MODE ?= debug
SRC_DIR := src
OBJ_BASE_DIR := objs
OBJ_DIR := $(OBJ_BASE_DIR)/$(MODE)
BIN_DIR := bin
ASSETS_DIR := assets/cooked
EXE     := $(BIN_DIR)/index.html

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

SRCS := $(shell find src -name "*.cpp")
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

ASSET_SRCS := $(shell if [ -d "$(ASSETS_DIR)" ]; then find $(ASSETS_DIR) -type f; fi)
ASSET_DESTS := $(patsubst $(ASSETS_DIR)/%,$(BIN_DIR)/%,$(ASSET_SRCS))

.PHONY: all clean run assets

all: $(EXE) assets

$(EXE): $(OBJS) shell.html
	@echo [LINK] $@
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

assets: $(ASSET_DESTS)

$(BIN_DIR)/%: $(ASSETS_DIR)/%
	@echo [COPY] $*
	@mkdir -p $(@D)
	@cp $< $@


$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@echo [CXX] $<
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	@echo [CLEAN] Removing artifacts...
	@rm -rf $(OBJ_BASE_DIR) $(BIN_DIR)

run: all
	@echo [RUN]
	@python -m http.server 8000 --directory $(BIN_DIR)

-include $(DEPS)
