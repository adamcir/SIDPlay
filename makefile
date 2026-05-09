# SIDPlay Makefile

TARGET = sidplay
PRG = $(TARGET).prg
D64 = $(TARGET).d64

CC = cl65
CC1541 = cc1541

CFLAGS = -t c64 -O

SOURCES = main.c sidplay_call.s

SID_FILES := $(shell find . -type f \( -iname "*.sid" \))

.PHONY: all clean d64 run list

all: $(PRG)

$(PRG): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(PRG)

d64: $(PRG)
	rm -f $(D64)
	cc1541 -n "sidplay" -i "sidply" \
		-w $(PRG) \
		-w "./ol3.sid" -w "./ol2.sid" -w "./motr.sid" \
		$(D64)

list:
	@echo "SID files found:"
	@$(foreach sid,$(SID_FILES),echo "$(sid)";)

clean:
	rm -f $(PRG) $(D64) *.o *.map
