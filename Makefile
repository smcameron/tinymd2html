
tinymd2html:	tinymd2html.c Makefile
	$(CC) -fsanitize=address,undefined -g -Wall -Wextra -o tinymd2html tinymd2html.c

clean:
	rm -f tinymd2html


