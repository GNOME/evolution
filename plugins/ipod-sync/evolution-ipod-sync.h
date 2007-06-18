/*
 * evolution-ipod-sync.h
 *
 * (C)2004 Justin Wake <jwake@iinet.net.au>
 *
 * Licensed under the GNU GPL v2. See COPYING.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <libhal.h>
#include <signal.h>

gboolean check_hal (void);
gboolean ipod_check_status (gboolean silent);
gboolean try_umount (char *device);

char *find_ipod_mount_point (LibHalContext *ctx);
char *ipod_get_mount (void);


