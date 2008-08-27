/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */


#ifndef CAMEL_OBJECT_REMOTE_IMPL_H
#define CAMEL_OBJECT_REMOTE_IMPL_H

DBusHandlerResult
camel_object_session_signal_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);

DBusHandlerResult
camel_object_store_signal_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);

DBusHandlerResult
camel_object_folder_signal_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data);

void
camel_object_remote_impl_init (void);

#endif
