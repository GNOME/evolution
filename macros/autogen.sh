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

test -f /opt/lib/libgtk.a \
  || test -f /opt/lib/libgtk.so \
  || test -f /opt/gnome/lib/libgtk.a \
  || test -f /opt/gnome/lib/libgtk.so \
  || test -f /usr/lib/libgtk.a \
  || test -f /usr/lib/libgtk.so \
  || test -f /usr/local/lib/libgtk.a \
  || test -f /usr/local/lib/libgtk.so \
  || cat <<EOF
**Warning**: You must have Gtk installed to compile Gnome.  I cannot
find it installed in the usual places.  "configure" may do a better
job of finding out if you have it installed.  If Gtk is not installed,
visit ftp://ftp.gimp.org/pub/gtk/ (or get it out of CVS too).

EOF

if test -z "$*"; then
    echo "**Warning**: I am going to run "\`configure\'" with no arguments."
    echo "If you wish to pass any to it, please specify them on the"
    echo \`$0\'" command line."
    echo
fi

for j in `find $srcdir -name configure.in -print`
do 
    i=`dirname $j`
    if test -e $i/NO-AUTO-GEN; then
        echo skipping $i -- flagged as no auto-gen
    else
    	macrodirs=`sed -n -e 's,AM_ACLOCAL_INCLUDE(\(.*\)),\1,gp' < $j`
    	echo processing $i
    	## debug
    	test -n "$macrodirs" && echo \`aclocal\' will also look in \`$macrodirs\'
    	(cd $i; \
    	aclocalinclude=""; \
    	for k in $macrodirs; do \
    	    if test -d $k; then aclocalinclude="$aclocalinclude -I $k"; \
    	    else echo "**Warning**: No such directory \`$k'.  Ignored."; fi; \
    	done; \
    	libtoolize --copy --force; \
    	aclocal $aclocalinclude; \
    	autoheader; automake --add-missing --gnu; autoheader; autoconf)
    fi
done

echo running $srcdir/configure --enable-maintainer-mode "$@"
$srcdir/configure --enable-maintainer-mode "$@" \
&& echo Now type \`make\' to compile the $PKG_NAME
