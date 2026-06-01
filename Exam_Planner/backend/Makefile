CC = gcc
CFLAGS = -Wall -Wextra -g -I./src
LDFLAGS = -lcurl -lsqlite3 -lcjson -lmicrohttpd -lpthread

SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = $(BUILD_DIR)/exam_planner

.PHONY: all clean run test

all: $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) --serve

clean:
	rm -rf $(BUILD_DIR)

test: $(TARGET)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_runner.c src/db_init.c src/json_parser.c -o $(BUILD_DIR)/test_runner $(LDFLAGS)
	./$(BUILD_DIR)/test_runner
