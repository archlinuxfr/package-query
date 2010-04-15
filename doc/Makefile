all: package-query.8.gz

package-query.8.gz: package-query.8
	gzip -c package-query.8 > package-query.8.gz

install: package-query.8.gz
	install -m 644 package-query.8.gz $(DESTDIR)/usr/share/man/man8/package-query.8.gz
clean:
	rm package-query.8.gz


