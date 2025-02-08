#!/bin/bash

./_build/src/e-util/test-markdown
if [ "$?" != "0" ]; then
	exit 2
fi

./_build/src/e-util/test-web-view-jsc
if [ "$?" != "0" ]; then
	exit 2
fi
