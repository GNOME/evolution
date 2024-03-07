![icon] Evolution
============

**Evolution** is the integrated mail, calendar and address book suite from
the Evolution Team.

See [the Evolution GNOME Wiki page][gnome-wiki] for more information.

If you are using Evolution, you may wish to subscribe to [the Evolution
users mailing list][mailing-list]. Visit there to subscribe or view archives
of the Evolution mailing list.

There is also a [#gnome-evolution] IRC channel on [Libera.Chat].

Help for Evolution is available in the user manual (select "Help" from
the menu after running the application), at the
[GNOME users help site][gnome-help], and in the --help strings (run
`evolution --help` at the command line).

The rest of this file is dedicated to building Evolution.

[icon]: https://raw.github.com/gnome-design-team/gnome-icons/master/apps/hicolor/48x48/apps/evolution.png "Evolution app icon"
[gnome-wiki]: https://gitlab.gnome.org/GNOME/evolution/-/wikis/home
[mailing-list]: https://lists.osuosl.org/mailman/listinfo/evolution-users
[#gnome-evolution]: ircs://irc.libera.chat:6697/gnome-evolution
[Libera.Chat]: https://libera.chat/
[gnome-help]: https://gnome.pages.gitlab.gnome.org/evolution/help/

DEPENDENCIES
------------

In order to build Evolution you need to have the full set of GNOME 3
(or greater) development libraries installed.

GNOME 3 or greater comes with most of the modern distributions, so
in most cases it should be enough to just install all the devel
packages from your distribution.

Please make sure you have the most recent versions of the libraries
installed, since bugs in the libraries can cause bugs in Evolution.

Additional dependencies, besides the stock GNOME libraries (the
dependencies should be compiled in the order they are listed here):

* [evolution-data-server of the same version as the Evolution is][eds]
* [libsoup 3.0 or later][libsoup]
* [WebKitGTK 2.34.0][webkitgtk]
* [Mozilla NSPR/NSS libraries][mozilla]
  These are needed if you want to compile Evolution with SSL and S/MIME
  support. Many distributions ship these as Mozilla development packages.

Other dependencies are claimed during the configure phase. If these are
optional, also a parameter for the CMake configure to not use that dependency
is shown.

[eds]: https://download.gnome.org/sources/evolution-data-server/
[libsoup]: https://download.gnome.org/sources/libsoup/
[webkitgtk]: https://webkitgtk.org/releases/
[mozilla]: https://www.mozilla.org/

CONFIGURING EVOLUTION
---------------------

First you have to decide whether you want to install Evolution (and
its dependencies) into the same prefix as the rest of your GNOME
install, or into a new prefix.

Installing everything into the same prefix as the rest of your GNOME
install will make it much easier to build and run programs, and easier
to switch between using packages and building it yourself, but it may
also make it harder to uninstall later.  Also, it increases the chance
that something goes wrong and your GNOME installation gets ruined.

If you want to install in a different prefix, you need to do the
following things:

* Set the environment variables to contain a colon-separated list
  of all the directories that will be involved in the build.
  The environment variables are `GSETTINGS_SCHEMA_DIR`,
  `LD_LIBRARY_PATH`, `PATH` and `PKG_CONFIG_PATH`.

  For example, if you have GNOME installed in `/usr` and you
  are installing Evolution and its dependencies in
  `/opt/evolution`, you want to do something like the following
  (assuming you are using Bash):

  ```bash
  export GSETTINGS_SCHEMA_DIR="/opt/evolution/share/glib-2.0/schemas"
  export LD_LIBRARY_PATH=/opt/evolution/lib:$LD_LIBRARY_PATH
  export PATH=/opt/evolution/bin:$PATH
  export PKG_CONFIG_PATH=/opt/evolution/lib/pkgconfig:$PKG_CONFIG_PATH
  ```

* Edit the D-Bus `session-local.conf` file (which is normally
  search for by D-Bus in `/etc/dbus-1/`) to include the
  location where you are installing Evolution.

  In the example given above (GNOME in `/usr`, Evolution and
  dependencies in `/opt/evolution`), your
  `session-local.conf` will have to look like this:

  ```xml
  <!DOCTYPE busconfig PUBLIC
        "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
        "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
  <busconfig>
        <!-- Search for .service files in /usr/local -->
        <servicedir>/opt/evolution/share/dbus-1/services</servicedir>
  </busconfig>
    ```

* Pass an appropriate `CMAKE_INSTALL_PREFIX` parameter to the configure
  scripts of Evolution and its dependencies, eg:

  ```bash
  cd ..../sources/evolution
  mkdir build
  cd build
  cmake -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=/opt/evolution \
        -DCMAKE_BUILD_TYPE=Release \
        ..
  ```

* Run `cmake --help` to get list of available generators (the -G argument)
          on your platform.

OPTIONAL FEATURES
-----------------

Some optional features can be enabled at compilation time by passing
appropriate flags to the CMake. These options are shown at the end
of the successful configure phase.

BUILDING EVOLUTION
------------------

After the Evolution is properly configured, run:

  ```bash
  make -j
  make -j install
  ```

to build it.

ONLINE BUILD MANUAL
-------------------

An [online build manual][Build Manual] is also available.

[Build Manual]: https://gitlab.gnome.org/GNOME/evolution/-/wikis/Building
