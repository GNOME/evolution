#include "config.h"
#include <stdio.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include "dbind.h"
#include "dbind-any.h"
#include <glib.h>
#include <stdarg.h>

#define d(x) 

/*
 * FIXME: compare types - to ensure they match &
 *        do dynamic padding of structures etc.
 */

DBindContext *
dbind_create_context (DBusBusType type, DBusError *opt_error)
{
    DBindContext *ctx = NULL;
    DBusConnection *cnx;
    DBusError *err, real_err;
    
    if (opt_error)
        err = opt_error;
    else {
        dbus_error_init (&real_err);
        err = &real_err;
    }
   
    cnx = dbus_bus_get_private (type, err);
    if (!cnx)
        goto out;

    ctx = g_new0 (DBindContext, 1);
    ctx->cnx = cnx;
    d(printf("DBIND DBUS %p\n", cnx));

out:
    if (err == &real_err)
        dbus_error_free (err);

    return ctx;
}

void
dbind_context_free (DBindContext *ctx)
{
    if (!ctx)
        return;
    dbus_connection_unref (ctx->cnx);
    g_free (ctx);
}

dbus_bool_t
dbind_context_method_call (DBindContext *ctx,
                           const char *bus_name,
                           const char *path,
                           const char *interface,
                           const char *method,
                           DBusError *opt_error,
                           const char *arg_types,
                           ...)
{
    dbus_bool_t success;
    va_list args;

    va_start (args, arg_types);

    success = dbind_connection_method_call_va
        (ctx->cnx, bus_name, path, interface, method, opt_error, arg_types, args);

    va_end (args);

    return success;
}

dbus_bool_t
dbind_connection_method_call (DBusConnection *cnx,
                              const char *bus_name,
                              const char *path,
                              const char *interface,
                              const char *method,
                              DBusError *opt_error,
                              const char *arg_types,
                              ...)
{
    dbus_bool_t success;
    va_list args;

    va_start (args, arg_types);

    success = dbind_connection_method_call_va
        (cnx, bus_name, path, interface, method, opt_error, arg_types, args);

    va_end (args);

    return success;
}

dbus_bool_t
dbind_context_emit_signal (DBindContext *ctx,
			   const char *bus_name,
			   const char *path,
			   const char *interface,
			   const char *method,
			   DBusError *opt_error,
			   const char *arg_types,
			   ...)
{
    dbus_bool_t success;
    va_list args;

    va_start (args, arg_types);

    success = dbind_connection_emit_signal_va
        (ctx->cnx, bus_name, path, interface, method, opt_error, arg_types, args);

    va_end (args);

    return success;
}

dbus_bool_t
dbind_connection_emit_signal (DBusConnection *cnx,
                              const char *bus_name,
                              const char *path,
                              const char *interface,
                              const char *method,
                              DBusError *opt_error,
                              const char *arg_types,
                              ...)
{
    dbus_bool_t success;
    va_list args;

    va_start (args, arg_types);

    success = dbind_connection_emit_signal_va
        (cnx, bus_name, path, interface, method, opt_error, arg_types, args);

    va_end (args);

    return success;
}

#ifdef CRAZY
/* urgh */
static GMutex *mutex = NULL;
static GCond *cond = NULL;
static gboolean waiting = FALSE;

/*
 * So - it is worse than this ...
 * If a reply comes in on the mainloop ...
 * We don't wake other threads up (!?)
 * or - what happens ? do we even get a 'set_reply' called ?
 */
   
static void set_reply (DBusPendingCall *pending, void *user_data)
{
  void **replyptr = (void **)user_data;

  *replyptr = dbus_pending_call_steal_reply (pending);
  fprintf (stderr, "L: %p set_reply - %p, serial %d\n", g_thread_self(), replyptr,
	   dbus_message_get_serial (*replyptr));
  g_cond_broadcast (cond);
}

static DBusMessage *
send_and_allow_reentry (DBusConnection *bus, DBusMessage *message, int timeout, DBusError *error)
{
  DBusPendingCall *pending;
  DBusMessage *reply = NULL;
  gboolean terminate = FALSE;

  /* urgh */
  if (!mutex) {
    mutex = g_mutex_new ();
    cond = g_cond_new();
  }
  if (!dbus_connection_send_with_reply (bus, message, &pending, timeout))
  {
    return NULL;
  }
  dbus_pending_call_set_notify (pending, set_reply, (void *)&reply, NULL);
  g_mutex_lock (mutex);
  fprintf (stderr, "L: %p send - %p (%s) [%d] serial %d reply serial %d\n",
	   g_thread_self(), &reply, dbus_message_get_member (message),
	   waiting, dbus_message_get_serial (message),
	   dbus_message_get_reply_serial (message));

  /* DBus's connection_read_write_dispatch is deadly broken wrt. replies ... */
  while (!reply && !terminate) {
	gboolean do_poll = !waiting;
	if (do_poll) {
		waiting = TRUE;
		g_mutex_unlock (mutex);
		/* FIXME: should we use a glib mainloop instead to process
		   this stuff ? */
		terminate = !dbus_connection_read_write_dispatch (bus, timeout);
		g_mutex_lock (mutex);
		g_cond_broadcast (cond);
		waiting = FALSE;
	} else
		g_cond_wait (cond, mutex);
  }
  g_mutex_unlock (mutex);

  return reply;
}
#endif

static dbus_bool_t
dbind_connection_exec_va (DBusConnection *cnx,
			  int         message_type, 
			  const char *bus_name,
			  const char *path,
			  const char *interface,
			  const char *method,
			  DBusError *opt_error,
			  const char *arg_types,
			  va_list     args)
{
    dbus_bool_t success = FALSE;
    DBusMessage *msg = NULL, *reply = NULL;
    DBusError *err, real_err;
    char *p;
    char *dest;

    d(printf("DBIND: %s: %s: %s: %d\n", bus_name, path, method, dbus_connection_get_is_connected(cnx)));
    if (opt_error)
        err = opt_error;
    else {
        dbus_error_init (&real_err);
        err = &real_err;
    }

//    msg = dbus_message_new_method_call (bus_name, path, interface, method);
    msg = dbus_message_new (message_type);
    dbus_message_set_destination (msg, bus_name);
    dbus_message_set_path (msg, path);
    dbus_message_set_interface (msg, interface);
    dbus_message_set_member (msg, method);
    if (!msg)
        goto out;
    dbus_message_set_auto_start (msg, TRUE);

    /* marshal */
    p = (char *)arg_types;
    {
        DBusMessageIter iter;
        
        dbus_message_iter_init_append (msg, &iter);
        /* special case base-types since we need to walk the stack worse-luck */
        for (;*p != '\0' && *p != '=';) {
            int intarg;
            void *ptrarg;
            double doublearg;
            dbus_int64_t int64arg;
            void *arg = NULL;

            switch (*p) {
            case DBUS_TYPE_BYTE:
            case DBUS_TYPE_BOOLEAN:
            case DBUS_TYPE_INT16:
            case DBUS_TYPE_UINT16:
            case DBUS_TYPE_INT32:
            case DBUS_TYPE_UINT32:
                intarg = va_arg (args, int);
                arg = &intarg;
                break;
            case DBUS_TYPE_INT64:
            case DBUS_TYPE_UINT64:
                int64arg = va_arg (args, dbus_int64_t);
                arg = &int64arg;
                break;
            case DBUS_TYPE_DOUBLE:
                doublearg = va_arg (args, double);
                arg = &doublearg;
                break;
            /* ptr types */
            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
            case DBUS_TYPE_SIGNATURE:
            case DBUS_TYPE_ARRAY:
            case DBUS_TYPE_DICT_ENTRY:
                ptrarg = va_arg (args, void *);
                arg = &ptrarg;
                break;
            case DBUS_STRUCT_BEGIN_CHAR:
                ptrarg = va_arg (args, void *);
                arg = ptrarg;
                break;

            case DBUS_TYPE_VARIANT:
                fprintf (stderr, "No variant support yet - very toolkit specific\n");
                ptrarg = va_arg (args, void *);
                arg = &ptrarg;
                break;
            default:
                fprintf (stderr, "Unknown / invalid arg type %c\n", *p);
                break;
            }
            if (arg != NULL)
                dbind_any_marshal (&iter, &p, &arg);
            }
    }

    dest = dbus_message_get_destination(msg);
    if (!dest)
        goto out;

    if (message_type == DBUS_MESSAGE_TYPE_SIGNAL) { /* one-way */
	success = dbus_connection_send (cnx, msg, NULL);
	goto out;
    } else {
	reply = dbus_connection_send_with_reply_and_block (cnx, msg, -1, err);
#ifdef CRAZY
        /* FIXME: We should clean evo.'s APIs up to not require re-enterancy later */
	reply = send_and_allow_reentry (cnx, msg, -1, err);
#endif
    }
    if (!reply)
        goto out;

    if (dbus_message_get_type (reply) == DBUS_MESSAGE_TYPE_ERROR)
    {
      char *name = dbus_message_get_error_name (reply);
      dbus_set_error (err, name, g_strdup (""));
      goto out;
    }
    /* demarshal */
    if (p[0] == '=' && p[1] == '>')
    {
        DBusMessageIter iter;
        p += 2;
        dbus_message_iter_init (reply, &iter);
        for (;*p != '\0';) {
            void *arg = va_arg (args, void *);
            dbind_any_demarshal (&iter, &p, &arg);
        }
    }

    success = TRUE;
out:
    if (msg)
        dbus_message_unref (msg);

    if (reply)
        dbus_message_unref (reply);

    if (err == &real_err)
        dbus_error_free (err);

    return success;
}

/* urgh - axe this mess in favour of a single public message_type method */
dbus_bool_t
dbind_connection_method_call_va (DBusConnection *cnx,
                                 const char *bus_name,
                                 const char *path,
                                 const char *interface,
                                 const char *method,
                                 DBusError *opt_error,
                                 const char *arg_types,
                                 va_list     args)
{
  return dbind_connection_exec_va (cnx, DBUS_MESSAGE_TYPE_METHOD_CALL,
				   bus_name, path, interface, method,
				   opt_error, arg_types, args);
}

dbus_bool_t
dbind_connection_emit_signal_va (DBusConnection *cnx,
                                 const char *bus_name,
                                 const char *path,
                                 const char *interface,
                                 const char *method,
                                 DBusError *opt_error,
                                 const char *arg_types,
                                 va_list     args)
{
  return dbind_connection_exec_va (cnx, DBUS_MESSAGE_TYPE_SIGNAL,
				   bus_name, path, interface, method,
				   opt_error, arg_types, args);
}
