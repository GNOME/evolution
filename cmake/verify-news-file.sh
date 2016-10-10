#!/bin/bash

# Verifies that the NEWS file's first line contains correct version and date.
# Requires arguments:
#   $1 ... the NEWS file name, preferably with full path
#   $2 ... expected version string, like "1.2.3"
#
# The date is expected in a form of YYYY-MM-DD of the current local time.
# The NEWS line is in form of "PROJECTNAME VERSION DATE".
#
# The test can be skipped entirely when SKIP_NEWS_FILE_TEST=1 is set.

FILENAME=$1
EXPVERSION=$2

if [ ! -f "$FILENAME" ]; then
	echo "File '$FILENAME' does not exist" 1>&2
	exit 1
fi

if [ -z "$EXPVERSION" ]; then
	echo "Expected version argument not given or empty, use format '1.2.3'" 1>&2
	exit 1
fi

NEWSLINE=`head --lines=1 "$FILENAME"`
EXPDATE=`date +%Y-%m-%d`

NEWSVERSION="${NEWSLINE#* }"
NEWSDATE="${NEWSVERSION#* }"
NEWSVERSION="${NEWSVERSION% *}"
SUCCESS=1

if [ "$NEWSVERSION" != "$EXPVERSION" ]; then
	echo "Read NEWS version '$NEWSVERSION' doesn't match expected version '$EXPVERSION'" 1>&2
	SUCCESS=0
fi

if [ "$NEWSDATE" != "$EXPDATE" ]; then
	echo "Read NEWS date '$NEWSDATE' doesn't match expected date '$EXPDATE'" 1>&2
	SUCCESS=0
fi

if [ "$SUCCESS" != "1" ]; then
	if [ "$SKIP_NEWS_FILE_TEST" = "1" ]; then
		echo "" 1>&2
		echo "****************************************************************" 1>&2
		echo "*  Failed NEWS file test ignored due to SKIP_NEWS_FILE_TEST=1  *" 1>&2
		echo "****************************************************************" 1>&2
		echo "" 1>&2
		exit 0
	else
		echo "(This test can be skipped when SKIP_NEWS_FILE_TEST=1 is set.)" 1>&2
	fi
	exit 1
fi
