#!/bin/sh

xgettext --default-domain=evolution --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f evolution.po \
   || ( rm -f ./evolution.pot \
    && mv evolution.po ./evolution.pot )
