
DESTDIR ?=
PREFIX ?= /usr/local
INSTALL ?= install
CC ?= gcc

all:	tinymd2html

tinymd2html:	tinymd2html.c Makefile
	$(CC) -fsanitize=address,undefined -g -Wall -Wextra -o tinymd2html tinymd2html.c

install:	all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/man/man1
	${INSTALL} -m 755 tinymd2html ${DESTDIR}${PREFIX}/bin
	${INSTALL} -m 644 man/tinymd2html.1 ${DESTDIR}${PREFIX}/man/man1

uninstall:
	/bin/rm -f ${DESTDIR}${PREFIX}/bin/tinymd2html
	/bin/rm -f ${DESTDIR}${PREFIX}/man/tinymd2html.1

clean:
	rm -f tinymd2html


