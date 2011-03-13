.PHONY: all clean install uninstall
TARGET: all

SUFFIX :=
CROSS_COMPILE :=
CC := $(CROSS_COMPILE)gcc
RM := rm -f
TOUCH := touch
RMDIR := rm -rf
CFLAGS := -g -pipe -Os -Wall -Wsign-compare -Wcast-align -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -finline-functions -Wmissing-format-attribute

ALL_BIN := onda
ONDA_OBJ := onda.o
ALL_OBJ := $(ONDA_OBJ)

install:

uninstall:

onda: $(ONDA_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(ONDA_OBJ)

ifeq ($(filter clean, $(MAKECMDGOALS)),clean)
CLEAN_DEP := clean
else
CLEAN_DEP :=
endif

%.o %.d: %.c $(CLEAN_DEP)
	@echo " [C] $(patsubst .%.d, %.c, $@)"
	@$(CC) $(CFLAGS) -MMD -MF $(patsubst %.o, .%.d, $@) \
		-MT $(patsubst .%.d, %.o, $@) \
		-c -o $(patsubst .%.d, %.o, $@) $<

all: $(ALL_BIN)
clean:
	$(RM) $(ALL_BIN) $(ALL_OBJ)

ifneq ($(MAKECMDGOALS),clean)
-include .*.d
endif
