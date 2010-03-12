CC=gcc -Wall #-g
all: package-query

package-query: aur.o alpm-query.o util.o package-query.o
	$(CC) *.o -o package-query -lalpm -lyajl -lcurl

install: package-query
	install -m 755 package-query $(DESTDIR)/usr/bin/package-query
clean:
	rm *.o package-query

