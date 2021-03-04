BUILD_DIR=build

#Compiler Parameters
CFLAGS = -Ofast -g -std=gnu11 -march=native -masm=att
LIB=-pthread -lm

DEFINES=
DEPENDS=

CONFIG_DIR=../src/defaultParams
SRC_DIR=../src
TEST_DIR=.

INC=-I$(CONFIG_DIR) -I$(SRC_DIR) -I$(TEST_DIR)

CONFIG_SRCS=convCodeParams.c
SRCS=convEncode.c convHelpers.c viterbiDecoder.c
TEST_SRCS=speedDecode.c

CONFIG_OBJS=$(patsubst %.c,$(BUILD_DIR)/config/%.o,$(CONFIG_SRCS))
OBJS=$(patsubst %.c,$(BUILD_DIR)/src/%.o,$(SRCS))
TEST_OBJS=$(patsubst %.c,$(BUILD_DIR)/test/%.o,$(TEST_SRCS))

#Production
all: speedDecode

speedDecode: $(CONFIG_OBJS) $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(INC) $(DEFINES) -o speedDecode $(CONFIG_OBJS) $(OBJS) $(TEST_OBJS) $(LIB)

$(BUILD_DIR)/config/%.o: $(CONFIG_DIR)/%.c $(HDRS_FULLPATH) | $(BUILD_DIR)/config/
	$(CC) $(CFLAGS) -c $(INC) $(DEFINES) -o $@ $<

$(BUILD_DIR)/src/%.o: $(SRC_DIR)/%.c $(HDRS_FULLPATH) | $(BUILD_DIR)/src/
	$(CC) $(CFLAGS) -c $(INC) $(DEFINES) -o $@ $<

$(BUILD_DIR)/test/%.o: $(TEST_DIR)/%.c $(HDRS_FULLPATH) | $(BUILD_DIR)/test/
	$(CC) $(CFLAGS) -c $(INC) $(DEFINES) -o $@ $<

$(BUILD_DIR)/:
	mkdir -p $@

$(BUILD_DIR)/config/: | $(BUILD_DIR)/
	mkdir -p $@

$(BUILD_DIR)/src/: | $(BUILD_DIR)/
	mkdir -p $@

$(BUILD_DIR)/test/: | $(BUILD_DIR)/
	mkdir -p $@

clean:
	rm -f speedDecode
	rm -rf build

.PHONY: clean
