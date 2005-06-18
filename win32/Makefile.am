EXTRA_DIST =				\
	dummy.la			\
	libemiscwidgets.def		\
	libetable.def			\
	libetext.def			\
	libevolution-addressbook.def	\
	libevolution-calendar.def	\
	libevolution-mail.def		\
	libfilter.def

if OS_WIN32
BOOTSTRAP_LIBS =			\
	libemiscwidgets.la		\
	libetable.la			\
	libetext.la			\
	libevolution-addressbook.la	\
	libevolution-calendar.la	\
	libevolution-mail.la		\
	libfilter.la
endif

noinst_DATA = $(BOOTSTRAP_LIBS)

# Yes, it's a bit silly to repeat the same command sequence here
# multiple times, but on the other hand there are subtle differences:
# Part of the DLLs have -0 in the name, part don't. (Those linked with
# libtool's -module -avoid-version options don't get the -0 suffix.)

libemiscwidgets.la: dummy.la libemiscwidgets.def
	sed -e s!%DLL%!libemiscwidgets-0! -e s!%LIB%!libemiscwidgets! -e s!%PFX%!$(prefix)! <dummy.la >$@
	mkdir -p .libs
	dlltool --output-lib=.libs/libemiscwidgets.dll.a --dllname=libemiscwidgets-0.dll --input-def=libemiscwidgets.def

libetable.la: dummy.la libetable.def
	sed -e s!%DLL%!libetable! -e s!%LIB%!libetable! -e s!%PFX%!$(prefix)! <dummy.la >$@
	mkdir -p .libs
	dlltool --output-lib=.libs/libetable.dll.a --dllname=libetable.dll --input-def=libetable.def

libetext.la: dummy.la libetext.def
	sed -e s!%DLL%!libetext! -e s!%LIB%!libetext! -e s!%PFX%!$(prefix)! <dummy.la >$@
	mkdir -p .libs
	dlltool --output-lib=.libs/libetext.dll.a --dllname=libetext.dll --input-def=libetext.def

libevolution-addressbook.la: dummy.la libevolution-addressbook.def
	sed -e s!%DLL%!libevolution-addressbook! -e s!%LIB%!libevolution-addressbook! -e s!%PFX%!$(prefix)! <dummy.la >$@
	mkdir -p .libs
	dlltool --output-lib=.libs/libevolution-addressbook.dll.a --dllname=libevolution-addressbook.dll --input-def=libevolution-addressbook.def

libevolution-calendar.la: dummy.la libevolution-calendar.def
	sed -e s!%DLL%!libevolution-calendar! -e s!%LIB%!libevolution-calendar! -e s!%PFX%!$(prefix)! <dummy.la >$@
	mkdir -p .libs
	dlltool --output-lib=.libs/libevolution-calendar.dll.a --dllname=libevolution-calendar.dll --input-def=libevolution-calendar.def

libevolution-mail.la: dummy.la libevolution-mail.def
	sed -e s!%DLL%!libevolution-mail! -e s!%LIB%!libevolution-mail! -e s!%PFX%!$(prefix)! <dummy.la >$@
	mkdir -p .libs
	dlltool --output-lib=.libs/libevolution-mail.dll.a --dllname=libevolution-mail.dll --input-def=libevolution-mail.def

libfilter.la: dummy.la libfilter.def
	sed -e s!%DLL%!libfilter-0! -e s!%LIB%!libfilter! -e s!%PFX%!$(prefix)! <dummy.la >$@
	mkdir -p .libs
	dlltool --output-lib=.libs/libfilter.dll.a --dllname=libfilter-0.dll --input-def=libfilter.def

CLEANFILES = $(BOOTSTRAP_LIBS)
