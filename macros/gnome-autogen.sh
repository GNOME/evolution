#!/bin/sh

if test -n "$USE_GNOME_2_MACROS" ; then
  export GNOME_COMMON_MACROS_DIR=`gnome-config --datadir`/aclocal/gnome2-macros
else
  export GNOME_COMMON_MACROS_DIR=`gnome-config --datadir`/aclocal/gnome-macros
fi

export ACLOCAL_FLAGS="-I $GNOME_COMMON_MACROS_DIR $ACLOCAL_FLAGS"
. $GNOME_COMMON_MACROS_DIR/autogen.sh

