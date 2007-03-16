/*
 * evolution-ipod-sync.h
 *
 * (C)2004 Justin Wake <jwake@iinet.net.au>
 *
 * Licensed under the GNU GPL v2. See COPYING.
 *
 */

#include "config.h"
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <libhal.h>
#include <signal.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(String) gettext (String)
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
#else
# define _(String)
# define N_(String) (String)
#endif

#ifdef EIS_DEBUG
# define dbg(fmt,arg...) fprintf(stderr, "%s/%d: " fmt,__FILE__,__LINE__,##arg)
#else
# define dbg(fmt,arg...) do { } while(0)
#endif

#define warn(fmt,arg...) g_warning("%s/%d: " fmt,__FILE__,__LINE__,##arg)


gboolean check_hal ();

char *find_ipod_mount_point (LibHalContext *ctx);
gboolean ipod_check_status (gboolean silent);
char *ipod_get_mount ();

