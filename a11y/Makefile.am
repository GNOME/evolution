SUBDIRS = . calendar widgets addressbook

# for debug
#A11Y_CFLAGS += -pedantic -ansi -DACC_DEBUG -Werror

privlib_LTLIBRARIES = libevolution-a11y.la

INCLUDES =                                              \
        -I$(top_srcdir)/a11y                            \
        $(A11Y_CFLAGS)

libevolution_a11y_la_SOURCES = 		        \
        ea-factory.h                            \
        ea-cell-table.h                         \
        ea-cell-table.c

