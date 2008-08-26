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
char * e_dbus_get_store_hash (const char *store_url);

char * e_dbus_get_folder_hash (const char *store_url, const char *folder_name);
DBusConnection * e_dbus_connection_get (void);
int mail_dbus_init(void);
