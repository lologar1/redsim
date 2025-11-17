SRC_DIR := src
INC_DIR := include
LIB_DIR := usflib2
OBJ_DIR := obj
TARGET := redsim

LIB_INC_DIR := $(LIB_DIR)/include
LIB_OBJ_DIR := $(LIB_DIR)/obj

CC := gcc
CFLAGS := -Wall -Wextra -Werror -O2 -g
LINKS := -lGL -lX11 -lpthread -lXrandr -ldl -lglfw -lm
INCLUDES := -I$(INC_DIR) -I$(LIB_INC_DIR)

SRCS := $(wildcard $(SRC_DIR)/*.c)

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
LIB_OBJS := $(wildcard $(LIB_OBJ_DIR)/*.o)
ALL_OBJS := $(OBJS) $(LIB_OBJS)

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LINKS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET)

.PHONY: all clean
