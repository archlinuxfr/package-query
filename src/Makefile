CC=gcc -Wall #-g
SRC = aur.c alpm-query.c util.c package-query.c color.c
OBJ = ${SRC:.c=.o}
LIBS = -lalpm -lyajl -lcurl

all: package-query

alpm-query.o: alpm-query.c util.h aur.h alpm-query.h
aur.o: aur.c aur.h util.h
color.o: color.c color.h util.h aur.h
package-query.o: package-query.c util.h aur.h alpm-query.h
util.o: util.c util.h aur.h alpm-query.h color.h

package-query: ${OBJ}
	$(CC) ${OBJ} -o package-query ${LIBS}

install: package-query
	install -m 755 package-query ${DESTDIR}/usr/bin/package-query
clean:
	rm *.o package-query

