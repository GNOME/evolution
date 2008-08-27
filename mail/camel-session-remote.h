/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#ifndef CAMEL_SESSION_REMOTE_H
#define CAMEL_SESSION_REMOTE_H

#include "camel-store-remote.h"
#include "camel-object-remote.h"

typedef struct {
	char *object_id;
} CamelSessionRemote;


/*
#define camel_session_construct camel_session_remote_construct
#define camel_session_get_password camel_session_remote_get_password
#define camel_session_get_storage_path camel_session_remote_get_storage_path
#define camel_session_forget_password camel_session_remote_forget_password
#define camel_session_get_service camel_session_remote_get_service
#define camel_session_alert_user camel_session_remote_alert_user
#define camel_session_build_password_prompt camel_session_remote_build_password_prompt
*/
void camel_session_remote_construct (CamelSessionRemote *session,
					const char *storage_path);

char *camel_session_remote_get_password (CamelSessionRemote *session,
				CamelObjectRemote *service,
				const char *domain,
				const char *prompt,
				const char *item,
				guint32 flags,
				CamelException *ex);

char *camel_session_remote_get_storage_path (CamelSessionRemote *session, 
					CamelObjectRemote *service,
					CamelException *ex);

void camel_session_remote_forget_password (CamelSessionRemote *session, 
					CamelObjectRemote *service,
					const char *domain,
					const char *item,
					CamelException *ex);


CamelObjectRemote *camel_session_remote_get_service (CamelSessionRemote *session, 
						const char *url_string,
			   			CamelProviderType type,
						CamelException *ex);

CamelObjectRemote *camel_session_remote_get_service_connected (CamelSessionRemote *session, 
						const char *url_string,
			   			CamelProviderType type,
						CamelException *ex);

gboolean camel_session_remote_alert_user (CamelSessionRemote *session, 
					CamelSessionAlertType type,
					const char *prompt,
					gboolean cancel);

char *camel_session_remote_build_password_prompt (const char *type,
					     const char *user,
					     const char *host);

gboolean camel_session_remote_is_online (CamelSessionRemote *session);

void camel_session_remote_set_online  (CamelSessionRemote *session,
				gboolean online);

gboolean camel_session_remote_check_junk (CamelSessionRemote *session);


void camel_session_remote_set_check_junk (CamelSessionRemote *session,
				   	gboolean check_junk);

gboolean camel_session_remote_get_network_state (CamelSessionRemote *session);

void camel_session_remote_set_network_state  (CamelSessionRemote *session,
				     	gboolean network_state);


#endif
