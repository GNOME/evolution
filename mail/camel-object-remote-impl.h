/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */


#ifndef CAMEL_OBJECT_REMOTE_IMPL_H
#define CAMEL_OBJECT_REMOTE_IMPL_H

typedef enum {
	CAMEL_ROT_SESSION=0,
	CAMEL_ROT_STORE,
	CAMEL_ROT_FOLDER		
}CamelObjectRemoteImplType;

DBusHandlerResult
camel_object_signal_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data,
				    CamelObjectRemoteImplType type);
void
camel_object_remote_impl_init (void);

#endif
