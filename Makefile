# ---------------------------------------------------------------------------
# SimpleGame - macOS build
#
#   make          build (Debug)
#   make run      build + run
#   make release  optimized build
#   make clean    remove build output
#
# Uses only system frameworks (OpenGL, GLUT, CoreText) - nothing to install.
# ---------------------------------------------------------------------------

CXX      := clang++
SRC_DIR  := SimpleGame
BUILD    := build
TARGET   := $(BUILD)/SimpleGame

SOURCES  := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS  := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD)/%.o,$(SOURCES))
DEPS     := $(OBJECTS:.o=.d)

CXXFLAGS := -std=c++17 -I$(SRC_DIR) -Wall -Wno-deprecated-declarations -MMD -MP
LDFLAGS  := -framework OpenGL -framework GLUT \
            -framework CoreText -framework CoreGraphics -framework CoreFoundation

CONFIG   ?= debug
ifeq ($(CONFIG),release)
  CXXFLAGS += -O2 -DNDEBUG
else
  CXXFLAGS += -g -O0
endif

.PHONY: all run release shot clean

all: $(TARGET) $(BUILD)/Shaders

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD)/%.o: $(SRC_DIR)/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# The program loads shaders from ./Shaders, so mirror them next to the binary.
$(BUILD)/Shaders: $(wildcard $(SRC_DIR)/Shaders/*)
	@mkdir -p $(BUILD)
	@rm -rf $(BUILD)/Shaders
	@cp -R $(SRC_DIR)/Shaders $(BUILD)/Shaders
	@touch $(BUILD)/Shaders

$(BUILD):
	@mkdir -p $(BUILD)

run: all
	cd $(BUILD) && ./SimpleGame

# 개발용 캡처: 정해진 경로를 걸어다닌 뒤 프레임버퍼를 저장하고 종료한다.
# 화면 기록 권한 없이 렌더링 결과를 확인할 수 있다.
shot: all
	cd $(BUILD) && ./SimpleGame --shot shot.ppm
	python3 tools/ppm2png.py $(BUILD)/shot.ppm $(BUILD)/shot.png

release:
	@$(MAKE) CONFIG=release

clean:
	rm -rf $(BUILD)

-include $(DEPS)
