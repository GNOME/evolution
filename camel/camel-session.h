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


typedef gboolean (*CamelTimeoutCallback) (gpointer data);
typedef enum {
	CAMEL_SESSION_ALERT_INFO,
	CAMEL_SESSION_ALERT_WARNING,
	CAMEL_SESSION_ALERT_ERROR
} CamelSessionAlertType;

struct _CamelSession
{
	CamelObject parent_object;
	struct _CamelSessionPrivate *priv;

	char *storage_path;
	GHashTable *providers, *modules;
};

typedef struct {
	CamelObjectClass parent_class;

	void            (*register_provider) (CamelSession *session,
					      CamelProvider *provider);
	GList *         (*list_providers)    (CamelSession *session,
					      gboolean load);
	CamelProvider * (*get_provider)      (CamelSession *session,
					      const char *url_string,
					      CamelException *ex);

	CamelService *  (*get_service)       (CamelSession *session,
					      const char *url_string,
					      CamelProviderType type,
					      CamelException *ex);
	char *          (*get_storage_path)  (CamelSession *session,
					      CamelService *service,
					      CamelException *ex);

	char *          (*get_password)      (CamelSession *session,
					      const char *prompt,
					      gboolean secret,
					      CamelService *service,
					      const char *item,
					      CamelException *ex);
	void            (*forget_password)   (CamelSession *session,
					      CamelService *service,
					      const char *item,
					      CamelException *ex);
	gboolean        (*alert_user)        (CamelSession *session,
					      CamelSessionAlertType type,
					      const char *prompt,
					      gboolean cancel);

	guint           (*register_timeout)  (CamelSession *session,
					      guint32 interval,
					      CamelTimeoutCallback callback,
					      gpointer user_data);
	gboolean        (*remove_timeout)    (CamelSession *session,
					      guint handle);

} CamelSessionClass;


/* public methods */

/* Standard Camel function */
CamelType camel_session_get_type (void);


void            camel_session_construct             (CamelSession *session,
						     const char *storage_path);

void            camel_session_register_provider     (CamelSession *session,
						     CamelProvider *provider);
GList *         camel_session_list_providers        (CamelSession *session,
						     gboolean load);

CamelProvider * camel_session_get_provider          (CamelSession *session,
						     const char *url_string,
						     CamelException *ex);

CamelService *  camel_session_get_service           (CamelSession *session,
						     const char *url_string,
						     CamelProviderType type,
						     CamelException *ex);
CamelService *  camel_session_get_service_connected (CamelSession *session, 
						     const char *url_string,
						     CamelProviderType type, 
						     CamelException *ex);

#define camel_session_get_store(session, url_string, ex) \
	((CamelStore *) camel_session_get_service_connected (session, url_string, CAMEL_PROVIDER_STORE, ex))
#define camel_session_get_transport(session, url_string, ex) \
	((CamelTransport *) camel_session_get_service_connected (session, url_string, CAMEL_PROVIDER_TRANSPORT, ex))

char *          camel_session_get_storage_path      (CamelSession *session,
						     CamelService *service,
						     CamelException *ex);

char *          camel_session_get_password          (CamelSession *session,
						     const char *prompt,
						     gboolean secret,
						     CamelService *service,
						     const char *item,
						     CamelException *ex);
void            camel_session_forget_password       (CamelSession *session,
						     CamelService *service,
						     const char *item,
						     CamelException *ex);
gboolean        camel_session_alert_user            (CamelSession *session,
						     CamelSessionAlertType type,
						     const char *prompt,
						     gboolean cancel);

guint           camel_session_register_timeout      (CamelSession *session,
						     guint32 interval,
						     CamelTimeoutCallback callback,
						     gpointer user_data);

gboolean        camel_session_remove_timeout        (CamelSession *session,
						     guint handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SESSION_H */
