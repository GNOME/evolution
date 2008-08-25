/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */



#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

int e_dbus_register_handler (const char *object_path, DBusObjectPathMessageFunction reg, DBusObjectPathUnregisterFunction unreg);

void e_dbus_connection_close (void);

int e_dbus_setup_handlers (void);
