#!/bin/bash

git diff --no-patch --exit-code HEAD

if [ ! $? -eq 0 ]; then
	echo "" 1>&2
	echo "***********************************************************************" 1>&2
	echo "  There are uncommitted changes which will not be part of the tarball  " 1>&2
	echo "***********************************************************************" 1>&2
	echo "" 1>&2
fi
