SOURCE_PATH=.
SOURCE_FILE=$(SOURCE_PATH)/change_resolution_ratio.c $(SOURCE_PATH)/main.c
LIB_FILE=-lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale
BIN_PATH=$(SOURCE_PATH)/bin

BIN_FILE=$(BIN_PATH)/test

INC_PATH=$(SOURCE_PATH)/ffmpeg/release/include
LIB_PATH=$(SOURCE_PATH)/ffmpeg/release/lib

CFLAG=-g -I$(INC_PATH)
LDFLAGS=-L$(LIB_PATH)



.PHONY:all clean

all:$(BIN_FILE)



$(BIN_FILE):$(SOURCE_FILE)
	mkdir -p $(BIN_PATH)
	$(CC) $(CFLAG) $(LDFLAGS) -o$(BIN_FILE) $^ $(LIB_FILE)
	
clean:
	rm -rf $(BIN_FILE)