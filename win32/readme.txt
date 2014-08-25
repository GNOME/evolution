************************************************
*** Caution - this work is still in progress ***
************************************************

These are steps how to setup local envinronment to build evolution
and its dependencies from sources.

a) http://sourceforge.net/projects/mingw/files/Installer/mingw-get-setup.exe/download
	Destination folder: C:\MinGW

	Install packages:
	- Basic setup:
	  - mingw-developer-toolkit
	  - msys-base

	- All Packages
	  - msys-patch
	  - msys-perl
	  - msys-unzip
	  - msys-wget

b) http://sourceforge.net/projects/mingwbuilds/files/mingw-builds-install/mingw-builds-install.exe/download
	Version: 4.8.1
	Architecture: x32
	Threads: posix
	Exception: dwarf
	Build revision: 5

	Destination folder: C:\MinGW64

c) Replace MinGW version of gcc & co. with the MinGW64 version (mainly for webkitgtk)
	- delete all but 'msys' directories in C:\MinGW\
	- move all directories from C:\MinGW64\mingw32\ to C:MinGW\

d) http://www.python.org/ftp/python/2.7.6/python-2.7.6.msi
	Destination folder: C:\Python27

e) Add to PATH: C:\Python27;C:\MinGW\bin

f) go to evolution checkout, into evolution\win32\ subfolder and execute:
   	$ source setup-env
   which setups build environment. This can take some addition
   parameters, see the top of setup-env for more information.

g) build evolution from sources with:
	$ make evolution
   or with some additional software:
	$ make addons

h) Make sure dbus-daemon is running before evolution is run, which can
   be done with $PREFIX\deps\bin\dbus-launch.exe

i) Run 'evolution'
