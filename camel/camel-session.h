/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.h : Abstract class for an email session */

/*
 *
 * Author :
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef CAMEL_SESSION_H
#define CAMEL_SESSION_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-object.h>
#include <camel/camel-provider.h>

#define CAMEL_SESSION_TYPE     (camel_session_get_type ())
#define CAMEL_SESSION(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SESSION_TYPE, CamelSession))
#define CAMEL_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SESSION_TYPE, CamelSessionClass))
#define CAMEL_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SESSION_TYPE))


typedef enum {
	CAMEL_AUTHENTICATOR_ASK, CAMEL_AUTHENTICATOR_TELL
} CamelAuthCallbackMode;

typedef char *(*CamelAuthCallback) (CamelAuthCallbackMode mode,
				    char *data, gboolean secret,
				    CamelService *service, char *item,
				    CamelException *ex);
typedef gboolean (*CamelTimeoutCallback) (gpointer data);
typedef guint (*CamelTimeoutRegisterCallback) (guint32 interval,
					       CamelTimeoutCallback cb,
					       gpointer camel_data);
typedef gboolean (*CamelTimeoutRemoveCallback) (guint id);

struct _CamelSession
{
	CamelObject parent_object;

	char *storage_path;
	CamelAuthCallback authenticator;
	CamelTimeoutRegisterCallback registrar;
	CamelTimeoutRemoveCallback remover;

	GHashTable *providers, *modules;
};

typedef struct {
	CamelObjectClass parent_class;

} CamelSessionClass;


/* public methods */

/* Standard Camel function */
CamelType camel_session_get_type (void);


CamelSession *  camel_session_new (const char *storage_path,
				   CamelAuthCallback authenticator,
				   CamelTimeoutRegisterCallback registrar,
				   CamelTimeoutRemoveCallback remover);

void            camel_session_register_provider       (CamelSession *session,
						       CamelProvider *provider);
GList *         camel_session_list_providers          (CamelSession *session,
						       gboolean load);

CamelService *  camel_session_get_service             (CamelSession *session,
						       const char *url_string,
						       CamelProviderType type,
						       CamelException *ex);
CamelService *  camel_session_get_service_connected   (CamelSession *session, 
						       const char *url_string,
						       CamelProviderType type, 
						       CamelException *ex);

#define camel_session_get_store(session, url_string, ex) \
	((CamelStore *) camel_session_get_service_connected (session, url_string, CAMEL_PROVIDER_STORE, ex))
#define camel_session_get_transport(session, url_string, ex) \
	((CamelTransport *) camel_session_get_service_connected (session, url_string, CAMEL_PROVIDER_TRANSPORT, ex))

char *          camel_session_get_storage_path (CamelSession *session,
						CamelService *service,
						CamelException *ex);

char *          camel_session_query_authenticator (CamelSession *session,
						   CamelAuthCallbackMode mode,
						   char *prompt,
						   gboolean secret,
						   CamelService *service,
						   char *item,
						   CamelException *ex);

guint           camel_session_register_timeout (CamelSession *session,
						guint32 interval,
						CamelTimeoutCallback callback,
						gpointer user_data);

gboolean        camel_session_remove_timeout (CamelSession *session,
					      guint handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SESSION_H */
