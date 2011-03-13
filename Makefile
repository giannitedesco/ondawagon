.PHONY: all clean install uninstall
TARGET: all

CONFIG_MAK := Config.mak
$(CONFIG_MAK):
	./configure

-include $(CONFIG_MAK)

SUFFIX :=
CROSS_COMPILE :=
CC := $(CROSS_COMPILE)gcc
RM := rm -f
TOUCH := touch
RMDIR := rm -rf
CFLAGS := -g -pipe -Os -Wall -Wsign-compare -Wcast-align -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -finline-functions -Wmissing-format-attribute $(LIBUSB_CFLAGS)

ALL_BIN := onda
ONDA_OBJ := onda.o
ALL_OBJ := $(ONDA_OBJ)

install:

uninstall:

onda: $(ONDA_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(ONDA_OBJ) $(LIBUSB_LIBS)

ifeq ($(filter clean, $(MAKECMDGOALS)),clean)
CLEAN_DEP := clean
else
CLEAN_DEP :=
endif

%.o %.d: %.c $(CLEAN_DEP) $(CONFIG_MAK)
	@echo " [C] $(patsubst .%.d, %.c, $@)"
	@$(CC) $(CFLAGS) -MMD -MF $(patsubst %.o, .%.d, $@) \
		-MT $(patsubst .%.d, %.o, $@) \
		-c -o $(patsubst .%.d, %.o, $@) $<

all: $(ALL_BIN)
clean:
	$(RM) Config.mak $(ALL_BIN) $(ALL_OBJ)

ifneq ($(MAKECMDGOALS),clean)
-include .*.d
endif
