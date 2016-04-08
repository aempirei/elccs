CC = gcc
CCFLAGS = -Wall -W -w
CFLAGS = -Wall -W -I. -pedantic -std=gnu99
LIBFLAGS =
PROGRAMS = austere_terminal
INSTALL_PATH = /usr/local/bin
INDENTFLAGS = -i4 -br -ce -nprs -nbfda -npcs -ncs -sob -brf -nut -bap -bad -npsl -l140

.PHONY: all clean tidy install

all: $(PROGRAMS)

austere_terminal: austere_terminal.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LIBFLAGS)

install: $(PROGRAMS)
	install -m755 $(PROGRAMS) $(INSTALL_PATH)

clean:
	rm -f *.o *~ $(PROGRAMS)

tidy:
	indent $(INDENTFLAGS) *.c
