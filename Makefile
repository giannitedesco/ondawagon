.PHONY: all clean install uninstall
TARGET: all

CONFIG_MAK := Config.mak
$(CONFIG_MAK): ./configure
	./configure

-include $(CONFIG_MAK)

SUFFIX :=
CROSS_COMPILE :=
CC := $(CROSS_COMPILE)gcc
RM := rm -f
TOUCH := touch
RMDIR := rm -rf
CFLAGS := -g -pipe -Os -Wall -Wsign-compare -Wcast-align -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -finline-functions -Wmissing-format-attribute $(LIBUSB_CFLAGS)

ALL_BIN := ondawagon
ONDA_OBJ := devlist.o \
		ondawagon.o
ALL_OBJ := $(ONDA_OBJ)
ALL_DEP := $(patsubst %.o, .%.d, $(ALL_OBJ))

install:

uninstall:

ondawagon: $(ONDA_OBJ)
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
	$(RM) Config.mak $(ALL_BIN) $(ALL_OBJ) $(ALL_DEP)

ifneq ($(MAKECMDGOALS),clean)
-include .*.d
endif
