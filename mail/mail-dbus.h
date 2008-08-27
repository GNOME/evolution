/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 * */

#include "dbind.h"
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

/* methods to be used when implementing the server process,
   and to call-back into the client */

int             mail_dbus_init(void);

char           *e_dbus_get_folder_hash (const char *store_url, const char *folder_name);
DBindContext   *e_dbus_peek_context (void);
DBusConnection *e_dbus_connection_get (void);
int             e_dbus_setup_handlers (void);
char           *e_dbus_get_store_hash (const char *store_url);
int             e_dbus_register_handler (const char *object_path, DBusObjectPathMessageFunction reg, DBusObjectPathUnregisterFunction unreg);
void            e_dbus_connection_close (void);

/* urgh - external init fn's we use */
extern void mail_session_remote_impl_init (void);
extern void camel_session_remote_impl_init (void);
extern void camel_object_remote_impl_init (void);
extern void camel_store_remote_impl_init (void);
