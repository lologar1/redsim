SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
TEST_DIR := tests

CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O2
INCLUDES := -I$(INC_DIR)

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(TEST_DIR)/%,$(TEST_SRCS))

all: $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_BINS)

$(TEST_DIR)/%: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJS) -o $@

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TEST_BINS)

.PHONY: all clean test
