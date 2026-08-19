CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread -g
LDFLAGS = libtailscale.a -lpthread -ldl -lm

# Folders
SRC_DIR = src
BIN_DIR = bin
INC_DIR = include

# Find all headers to ensure we recompile if they change
HEADERS = $(wildcard $(INC_DIR)/*.h)

# Main Targets
all: $(BIN_DIR)/server $(BIN_DIR)/client

# Individual Build Modules
server: $(BIN_DIR)/server
client: $(BIN_DIR)/client

# Link Server
$(BIN_DIR)/server: $(SRC_DIR)/server.o $(SRC_DIR)/network.o $(SRC_DIR)/ring_buffer.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Link Client
$(BIN_DIR)/client: $(SRC_DIR)/client.o $(SRC_DIR)/network.o $(SRC_DIR)/ring_buffer.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile Source Files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o $(BIN_DIR)/*
	@echo "Project cleaned."

.PHONY: all server client clean
