# Generated automatically from Makefile.in by configure.
VERSION = 0.15a
DISTNAME = libical
SUBDIRS = src

prefix = /usr/local
PREFIX = ${prefix}
EXEC_PREFIX = ${prefix}
BIN_DIR = $(EXEC_PREFIX)/bin
LIB_DIR = $(EXEC_PREFIX)/lib
INCLUDE_DIR = $(PREFIX)/include   

INSTALL        = /usr/bin/install -c
INSTALL_DATA   = ${INSTALL} -m 644
INSTALL_PROGRAM= ${INSTALL} -m 755       


all:
	for dir in $(SUBDIRS) ; do \
	  (cd $$dir ; $(MAKE) $@) || exit 1; \
	done

depend:
	for dir in $(SUBDIRS) ; do \
	  (cd $$dir ; $(MAKE) $@) || exit 1; \
	done

clean:
	-/bin/rm -rf $(DISTNAME)* config.log config.cache config.status configure.scan
	-/bin/rm *~
	for dir in $(SUBDIRS) ; do \
	  (cd $$dir ; $(MAKE) $@) || exit 1; \
	done

dist: clean
	distname="$(DISTNAME)-$(VERSION)"; \
	distfiles=`find . -name "$$distname" -prune -o -name "design" -prune -o -name "rfc*" -prune -o -name "comp-cc" -prune -o -name "RCS" -prune -o -print | sed 's/^\.\///' ` ;\
	mkdir $$distname ; \
	for file in $$distfiles; do \
	  test -d $$file && mkdir $$distname/$$file; \
          test -f $$file &&   { ln ./$$file $$distname/$$file \
          || { echo copying $$file instead; cp -p $$file $$distname/$$file;};}; \
        done; \
	tar -czvf $$distname.tar.gz $$distname ; 


install: 
	@for i in $(LIB_DIR) $(INCLUDE_DIR) $(BIN_DIR);\
	do \
	  if [ ! -d $$i ] ; then \
	         mkdir $$i; \
	         echo "Making directory $$i"; \
	         chmod 755 $$i;\
	         else true; \
	  fi; \
	done;

	for dir in $(SUBDIRS) ; do \
	  (cd $$dir ; $(MAKE) $@) || exit 1; \
	done	

