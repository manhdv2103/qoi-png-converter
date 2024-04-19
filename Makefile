# Taken from: https://github.com/TheNetAdmin/Makefile-Templates/tree/master/SmallProject

# tool macros
CC := gcc
CFLAGS := -std=c99 -pedantic -Wall -Wextra -O3
DBGFLAGS := -g
COBJFLAGS := $(CFLAGS) -c
MAKEFLAGS += --no-print-directory

# path macros
BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := .
DBG_PATH := debug

# compile macros
TARGET_NAME := qpc
TARGET := $(BIN_PATH)/$(TARGET_NAME)
TARGET_DEBUG := $(DBG_PATH)/$(TARGET_NAME)

# src files & obj files
SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c)))
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
OBJ_DEBUG := $(addprefix $(DBG_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
DEPENDS := $(addprefix $(OBJ_PATH)/, $(addsuffix .d, $(notdir $(basename $(SRC)))))

# clean files list
DISTCLEAN_LIST := $(OBJ) \
                  $(OBJ_DEBUG)
CLEAN_LIST := $(TARGET) \
		  $(TARGET_DEBUG) \
		  $(DISTCLEAN_LIST)

# default rule
default: makedir all

-include $(DEPENDS)

# non-phony targets
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c Makefile
	$(CC) $(COBJFLAGS) -MMD -MP -o $@ $<

$(TARGET_DEBUG): $(OBJ_DEBUG)
	$(CC) $(CFLAGS) $(DBGFLAGS) -o $@ $(OBJ_DEBUG)

$(DBG_PATH)/%.o: $(SRC_PATH)/%.c Makefile
	$(CC) $(COBJFLAGS) $(DBGFLAGS) -MMD -MP -o $@ $<

# phony rules
.PHONY: run
run:
	@$(TARGET)

.PHONY: watch
watch:
	@find . -name "*.c" -o -name "*.h" -o -name Makefile | entr -c bash -c "$(MAKE) --silent && $(MAKE) run"

.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH)

.PHONY: all
all: $(TARGET)

.PHONY: debug
debug: $(TARGET_DEBUG)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(DISTCLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)
