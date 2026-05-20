CC ?= gcc
PKG_CONFIG ?= pkg-config

TARGET ?= exactcc
ROOT := ..
SRC := src/exact_chinese_chess_portmaster.c $(ROOT)/xiangqi_engine.c

CFLAGS += -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags sdl2)
SDL_LIBS := $(filter-out -Wl%,$(shell $(PKG_CONFIG) --libs sdl2))
LDLIBS += $(SDL_LIBS) -lm

.PHONY: all clean package-layout piece-assets

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

package-layout: $(TARGET)
	mkdir -p port/exact_chinesechess/exact_chinesechess/licenses
	rm -rf port/exact_chinesechess/exact_chinesechess/bin
	rm -f port/exact_chinesechess/exact_chinesechess/exact_chinesechess.aarch64 \
	      port/exact_chinesechess/exact_chinesechess/exact_chinesechess.armhf \
	      port/exact_chinesechess/exact_chinesechess/exactcc.aarch64 \
	      port/exact_chinesechess/exact_chinesechess/exactcc.armhf
	cp $(TARGET) port/exact_chinesechess/exact_chinesechess/$(TARGET).$${DEVICE_ARCH:-aarch64}
	cp $(ROOT)/licenses/COPYING.GPL3 port/exact_chinesechess/exact_chinesechess/licenses/
	cp $(ROOT)/licenses/COPYING.GPL3 port/exact_chinesechess/exact_chinesechess/licenses/COPYING.XMULI.GPL3
	cp COPYING.AUGUS.MIT port/exact_chinesechess/exact_chinesechess/licenses/
	cp THIRD_PARTY_NOTICES.md port/exact_chinesechess/exact_chinesechess/licenses/
	if [ -x "$(ROOT)/bin/$${DEVICE_ARCH:-aarch64}/pikafish" ]; then \
		mkdir -p port/exact_chinesechess/exact_chinesechess/bin/$${DEVICE_ARCH:-aarch64}; \
		cp "$(ROOT)/bin/$${DEVICE_ARCH:-aarch64}/pikafish" "$(ROOT)/bin/$${DEVICE_ARCH:-aarch64}/pikafish.nnue" port/exact_chinesechess/exact_chinesechess/bin/$${DEVICE_ARCH:-aarch64}/; \
		cp "$(ROOT)/Pikafish/Copying.txt" port/exact_chinesechess/exact_chinesechess/licenses/COPYING.PIKAFISH.GPL3; \
		cp "$(ROOT)/Pikafish/AUTHORS" port/exact_chinesechess/exact_chinesechess/licenses/PIKAFISH_AUTHORS; \
	fi

piece-assets:
	python3 scripts/generate_piece_assets.py

clean:
	rm -f $(TARGET)
