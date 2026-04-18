# Simple Makefile
# root
#  |
#  +--- BIN_DIR
#  |
#  +--- *INC_DIR
#  |
#  +--- *LIB_DIR
#  |
#  +--- OBJ_DIR
#  |
#  +--- *SRC_DIR
#
#  * Mandatory
#

PRJ_DIR := $(shell basename "$(CURDIR)")

BIN_DIR := .
INC_DIR := inc
LIB_DIR := lib
OBJ_DIR := obj
SRC_DIR := src

EXE := $(BIN_DIR)/$(PRJ_DIR)
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

CC       := gcc                                     # C Compiler
CPPFLAGS := -I. -I$(SRC_DIR) -I$(INC_DIR) -MMD -MP	# C Pre-Processor flags
CFLAGS   := -Wall -Wextra -O0 -std=gnu2x -g3	    # C compiler flags
LDFLAGS  := -L$(LIB_DIR)				            # C linker flags
LDLIBS   := -lm							            # Libs need

all: $(EXE)

vars:
	@echo " "
	@echo "PRJ_DIR = $(PRJ_DIR)"
	@echo "BIN_DIR = $(BIN_DIR)"
	@echo "INC_DIR = $(INC_DIR)"
	@echo "LIB_DIR = $(LIB_DIR)"
	@echo "OBJ_DIR = $(OBJ_DIR)"
	@echo "SRC_DIR = $(SRC_DIR)"
	@echo " "
	@echo "EXE = $(EXE)"
	@echo "SRC = $(SRC)"
	@echo "OBJ = $(OBJ)"
	@echo " "
	@echo "CC = $(CC)"
	@echo "CPPFLAGS = $(CPPFLAGS)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "LDLIBS = $(LDLIBS)"
	@echo " "

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: all clean

clean:
	-@$(RM) -rv $(EXE) $(OBJ_DIR)

-include $(OBJ:.o=.d)

