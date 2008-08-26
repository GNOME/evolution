#ifndef _DBIND_H_
#define _DBIND_H_


#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>


struct _DBindContext {
    DBusConnection *cnx;
};
typedef struct _DBindContext DBindContext;

DBindContext *dbind_create_context         (DBusBusType type, DBusError *opt_error);
void          dbind_context_free           (DBindContext *ctx);
dbus_bool_t   dbind_context_method_call    (DBindContext *ctx,
                                            const char *bus_name,
                                            const char *path,
                                            const char *interface,
                                            const char *method,
                                            DBusError *opt_error,
                                            const char *arg_types,
                                            ...);

/* dbus connection variants */
dbus_bool_t   dbind_connection_method_call    (DBusConnection *cnx,
                                               const char *bus_name,
                                               const char *path,
                                               const char *interface,
                                               const char *method,
                                               DBusError *opt_error,
                                               const char *arg_types,
                                               ...);
dbus_bool_t   dbind_connection_method_call_va (DBusConnection *cnx,
                                               const char *bus_name,
                                               const char *path,
                                               const char *interface,
                                               const char *method,
                                               DBusError *opt_error,
                                               const char *arg_types,
                                               va_list     args);


#endif /* _DBIND_H_ */
