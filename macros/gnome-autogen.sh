#!/bin/sh

export GNOME_COMMON_MACROS_DIR=`gnome-config --datadir`/aclocal/gnome-macros
export ACLOCAL_FLAGS="-I $GNOME_COMMON_MACROS_DIR $ACLOCAL_FLAGS"
. $GNOME_COMMON_MACROS_DIR/autogen.sh

