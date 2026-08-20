CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread -g
LDFLAGS = -lpthread

# Uncomment these two lines for UPnP auto port-forwarding.
# Requires: sudo apt install libminiupnpc-dev
# CFLAGS += -DUSE_UPNP
# LDFLAGS += -lminiupnpc

SRC_DIR = src
BIN_DIR = bin
INC_DIR = include
HEADERS = $(wildcard $(INC_DIR)/*.h)

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR)/server: $(SRC_DIR)/server.o $(SRC_DIR)/network.o $(SRC_DIR)/ring_buffer.o $(SRC_DIR)/stun.o $(SRC_DIR)/config.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/client: $(SRC_DIR)/client.o $(SRC_DIR)/network.o $(SRC_DIR)/ring_buffer.o $(SRC_DIR)/stun.o $(SRC_DIR)/config.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o $(BIN_DIR)/*

.PHONY: all clean
