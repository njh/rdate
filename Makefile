PACKAGE=rdate
VERSION=1.4
CFLAGS=-g -Wall -pipe
RCFLAGS=$(CFLAGS) -D_GNU_SOURCE
prefix=/usr
mandir=$(prefix)/share/man
bindir=$(prefix)/bin

all: rdate

rdate: rdate.c
	$(CC) -o rdate rdate.c $(RCFLAGS)

install: all
	install -d $(bindir)
	install -d $(mandir)/man1
	install -m 555 rdate $(bindir)
	install -m 444 rdate.1 $(mandir)/man1

dist:
	rm -rf $(PACKAGE)-$(VERSION)
	mkdir $(PACKAGE)-$(VERSION)
	cp rdate.spec COPYING rdate.c Makefile rdate.1 $(PACKAGE)-$(VERSION)
	tar -czSpf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)
	rm -rf $(PACKAGE)-$(VERSION)

clean:
	rm -f rdate
