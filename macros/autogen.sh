#!/bin/sh
# Run this to generate all the initial makefiles, etc.

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have "\`autoconf\'" installed to compile Gnome."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    DIE=1
}

(libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have "\`libtool\'" installed to compile Gnome."
    echo "Get ftp://ftp.gnu.org/pub/gnu/libtool-1.2.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have "\`automake\'" installed to compile Gnome."
    echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
    NO_AUTOMAKE=yes
}


# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: Missing "\`aclocal\'".  The version of "\`automake\'
    echo "installed doesn't appear recent enough."
    echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
}

if test "$DIE" -eq 1; then
    exit 1
fi

if test -z "$*"; then
    echo "**Warning**: I am going to run "\`configure\'" with no arguments."
    echo "If you wish to pass any to it, please specify them on the"
    echo \`$0\'" command line."
    echo
fi

case $CC in
xlc )
    am_opt=--include-deps;;
esac

for j in `find $srcdir -name configure.in -print`
do 
    i=`dirname $j`
    if test -f $i/NO-AUTO-GEN; then
        echo skipping $i -- flagged as no auto-gen
    else
    	macrodirs=`sed -n -e 's,AM_ACLOCAL_INCLUDE(\(.*\)),\1,gp' < $j`
    	echo processing $i
    	## debug
    	test -n "$macrodirs" && echo \`aclocal\' will also look in \`$macrodirs\'
    	(cd $i; \
        aclocalinclude="$ACLOCAL_FLAGS"; \
    	for k in $macrodirs; do \
    	    if test -d $k; then aclocalinclude="$aclocalinclude -I $k"; \
    	    else echo "**Warning**: No such directory \`$k'.  Ignored."; fi; \
    	done; \
    	libtoolize --copy --force; \
    	aclocal $aclocalinclude; \
    	autoheader; automake --add-missing --gnu $am_opt; autoheader; autoconf)
    fi
done

conf_flags="--enable-maintainer-mode --enable-compile-warnings" #--enable-iso-c
if [ ! -z "$OBJ_DIR" ]; then
	mkdir -p "$OBJ_DIR"
	cd "$OBJ_DIR"
fi

if test x$NOCONFIGURE = x; then
echo running $srcdir/$OBJ_DIR/configure $conf_flags "$@"
if [ -z "$OBJ_DIR" ]; then
	$srcdir/configure $conf_flags "$@" \
	&& echo Now type \`make\' to compile $PKG_NAME
else
	$srcdir/../configure $conf_flags "$@" \
	&& echo Now type \`make\' to compile $PKG_NAME
fi
else
echo Skipping configure process.
fi
