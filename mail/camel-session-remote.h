/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include "camel-store-remote.h"

typedef struct {
	char *object_id;
} CamelSessionRemote;

void camel_session_remote_construct (CamelSessionRemote *session,
					const char *storage_path);

char *camel_session_remote_get_password (CamelSessionRemote *session,
				CamelStoreRemote *service,
				const char *domain,
				const char *prompt,
				const char *item,
				guint32 flags);

char *camel_session_remote_get_storage_path (CamelSessionRemote *session, 
					CamelStoreRemote *service);

void camel_session_remote_forget_password (CamelSessionRemote *session, 
					CamelStoreRemote *service,
					const char *domain,
					const char *item);


CamelStoreRemote *camel_session_remote_get_service (CamelSessionRemote *session, 
						const char *url_string,
			   			CamelProviderType type);

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


