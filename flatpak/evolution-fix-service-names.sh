# see https://gitlab.gnome.org/GNOME/glib/issues/1737
# previous versions used milliseconds instead of seconds as the timeout argument
(`pkg-config --atleast-version 2.60.1 gio-2.0` || `pkg-config --atleast-version 2.61.0 gio-2.0`) && TIMEOUTMULT= || TIMEOUTMULT=000

sed -e "s|@SOURCES_SERVICE@|$(pkg-config --variable=sourcesdbusservicename evolution-data-server-1.2)|" \
    -e "s|@ADDRESSBOOK_SERVICE@|$(pkg-config --variable=addressbookdbusservicename evolution-data-server-1.2)|" \
    -e "s|@CALENDAR_SERVICE@|$(pkg-config --variable=calendardbusservicename evolution-data-server-1.2)|" \
    -e "s|@TIMEOUTMULT@|${TIMEOUTMULT}|"
