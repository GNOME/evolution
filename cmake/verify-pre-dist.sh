#!/bin/bash

git diff --no-patch --exit-code HEAD

if [ ! $? -eq 0 ]; then
	echo "" 1>&2
	echo "***********************************************************************" 1>&2
	echo "  There are uncommitted changes which will not be part of the tarball  " 1>&2
	echo "***********************************************************************" 1>&2
	echo "" 1>&2

	if [ "$SKIP_COMMIT_TEST" = "1" ]; then
		echo "(Failed commit test skipped due to SKIP_COMMIT_TEST=1 being set.)" 1>&2
		exit 0
	else
		echo "(This test can be skipped when SKIP_COMMIT_TEST=1 is set.)" 1>&2
	fi
	exit 1
fi
