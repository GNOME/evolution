/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-service.c : Abstract class for an email service */

/*
 *
 * Author :
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <errno.h>

#include <sys/poll.h>

#include "e-util/e-msgport.h"
#include "e-util/e-host-utils.h"

#include "camel-service.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "camel-operation.h"
#include "camel-private.h"
#include "camel-i18n.h"

#define d(x)
#define w(x)

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelService */
#define CSERV_CLASS(so) CAMEL_SERVICE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void construct (CamelService *service, CamelSession *session,
		       CamelProvider *provider, CamelURL *url,
		       CamelException *ex);
static gboolean service_connect(CamelService *service, CamelException *ex);
static gboolean service_disconnect(CamelService *service, gboolean clean,
				   CamelException *ex);
static void cancel_connect (CamelService *service);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static char *get_name (CamelService *service, gboolean brief);
static char *get_path (CamelService *service);

static int service_setv (CamelObject *object, CamelException *ex, CamelArgV *args);
static int service_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args);


static void
camel_service_class_init (CamelServiceClass *camel_service_class)
{
	CamelObjectClass *object_class = CAMEL_OBJECT_CLASS (camel_service_class);
	
	parent_class = camel_type_get_global_classfuncs (CAMEL_OBJECT_TYPE);
	
	/* virtual method overloading */
	object_class->setv = service_setv;
	object_class->getv = service_getv;
	
	/* virtual method definition */
	camel_service_class->construct = construct;
	camel_service_class->connect = service_connect;
	camel_service_class->disconnect = service_disconnect;
	camel_service_class->cancel_connect = cancel_connect;
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->get_name = get_name;
	camel_service_class->get_path = get_path;
}

static void
camel_service_init (void *o, void *k)
{
	CamelService *service = o;
	
	service->priv = g_malloc0(sizeof(*service->priv));
	service->priv->connect_lock = e_mutex_new(E_MUTEX_REC);
	service->priv->connect_op_lock = e_mutex_new(E_MUTEX_SIMPLE);
}

static void
camel_service_finalize (CamelObject *object)
{
	CamelService *service = CAMEL_SERVICE (object);

	if (service->status == CAMEL_SERVICE_CONNECTED) {
		CamelException ex;
		
		camel_exception_init (&ex);
		CSERV_CLASS (service)->disconnect (service, TRUE, &ex);
		if (camel_exception_is_set (&ex)) {
			w(g_warning ("camel_service_finalize: silent disconnect failure: %s",
				     camel_exception_get_description (&ex)));
		}
		camel_exception_clear (&ex);
	}
	
	if (service->url)
		camel_url_free (service->url);
	if (service->session)
		camel_object_unref (service->session);
	
	e_mutex_destroy (service->priv->connect_lock);
	e_mutex_destroy (service->priv->connect_op_lock);
	
	g_free (service->priv);
}



CamelType
camel_service_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type =
			camel_type_register (CAMEL_OBJECT_TYPE,
					     "CamelService",
					     sizeof (CamelService),
					     sizeof (CamelServiceClass),
					     (CamelObjectClassInitFunc) camel_service_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_service_init,
					     camel_service_finalize );
	}
	
	return type;
}


static int
service_setv (CamelObject *object, CamelException *ex, CamelArgV *args)
{
	CamelService *service = (CamelService *) object;
	CamelURL *url = service->url;
	gboolean reconnect = FALSE;
	guint32 tag;
	int i;
	
	for (i = 0; i < args->argc; i++) {
		tag = args->argv[i].tag;
		
		/* make sure this is an arg we're supposed to handle */
		if ((tag & CAMEL_ARG_TAG) <= CAMEL_SERVICE_ARG_FIRST ||
		    (tag & CAMEL_ARG_TAG) >= CAMEL_SERVICE_ARG_FIRST + 100)
			continue;
		
		if (tag == CAMEL_SERVICE_USERNAME) {
			/* set the username */
			if (strcmp (url->user, args->argv[i].ca_str) != 0) {
				camel_url_set_user (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_AUTH) {
			/* set the auth mechanism */
			if (strcmp (url->authmech, args->argv[i].ca_str) != 0) {
				camel_url_set_authmech (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_HOSTNAME) {
			/* set the hostname */
			if (strcmp (url->host, args->argv[i].ca_str) != 0) {
				camel_url_set_host (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_PORT) {
			/* set the port */
			if (url->port != args->argv[i].ca_int) {
				camel_url_set_port (url, args->argv[i].ca_int);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_PATH) {
			/* set the path */
			if (strcmp (url->path, args->argv[i].ca_str) != 0) {
				camel_url_set_path (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else {
			/* error? */
			continue;
		}
		
		/* let our parent know that we've handled this arg */
		camel_argv_ignore (args, i);
	}
	
	/* FIXME: what if we are in the process of connecting? */
	if (reconnect && service->status == CAMEL_SERVICE_CONNECTED) {
		/* reconnect the service using the new URL */
		if (camel_service_disconnect (service, TRUE, ex))
			camel_service_connect (service, ex);
	}
	
	return CAMEL_OBJECT_CLASS (parent_class)->setv (object, ex, args);
}

static int
service_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	CamelService *service = (CamelService *) object;
	CamelURL *url = service->url;
	guint32 tag;
	int i;
	
	for (i = 0; i < args->argc; i++) {
		tag = args->argv[i].tag;
		
		/* make sure this is an arg we're supposed to handle */
		if ((tag & CAMEL_ARG_TAG) <= CAMEL_SERVICE_ARG_FIRST ||
		    (tag & CAMEL_ARG_TAG) >= CAMEL_SERVICE_ARG_FIRST + 100)
			continue;
		
		switch (tag) {
		case CAMEL_SERVICE_USERNAME:
			/* get the username */
			*args->argv[i].ca_str = url->user;
			break;
		case CAMEL_SERVICE_AUTH:
			/* get the auth mechanism */
			*args->argv[i].ca_str = url->authmech;
			break;
		case CAMEL_SERVICE_HOSTNAME:
			/* get the hostname */
			*args->argv[i].ca_str = url->host;
			break;
		case CAMEL_SERVICE_PORT:
			/* get the port */
			*args->argv[i].ca_int = url->port;
			break;
		case CAMEL_SERVICE_PATH:
			/* get the path */
			*args->argv[i].ca_str = url->path;
			break;
		default:
			/* error? */
			break;
		}
	}
	
	return CAMEL_OBJECT_CLASS (parent_class)->getv (object, ex, args);
}

static void
construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex)
{
	char *err, *url_string;
	
	if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_USER) &&
	    (url->user == NULL || url->user[0] == '\0')) {
		err = _("URL '%s' needs a username component");
		goto fail;
	} else if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_HOST) &&
		   (url->host == NULL || url->host[0] == '\0')) {
		err = _("URL '%s' needs a host component");
		goto fail;
	} else if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_PATH) &&
		   (url->path == NULL || url->path[0] == '\0')) {
		err = _("URL '%s' needs a path component");
		goto fail;
	}
	
	service->provider = provider;
	service->url = camel_url_copy(url);
	service->session = session;
	camel_object_ref (session);
	
	service->status = CAMEL_SERVICE_DISCONNECTED;

	return;

fail:
	url_string = camel_url_to_string(url, CAMEL_URL_HIDE_PASSWORD);
	camel_exception_setv(ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID, err, url_string);
	g_free(url_string);
}

/**
 * camel_service_construct:
 * @service: the CamelService
 * @session: the session for the service
 * @provider: the service's provider
 * @url: the default URL for the service (may be NULL)
 * @ex: a CamelException
 *
 * Constructs a CamelService initialized with the given parameters.
 **/
void
camel_service_construct (CamelService *service, CamelSession *session,
			 CamelProvider *provider, CamelURL *url,
			 CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_SERVICE (service));
	g_return_if_fail (CAMEL_IS_SESSION (session));
	
	CSERV_CLASS (service)->construct (service, session, provider, url, ex);
}


static gboolean
service_connect (CamelService *service, CamelException *ex)
{
	/* Things like the CamelMboxStore can validly
	 * not define a connect function.
	 */
	 return TRUE;
}

/**
 * camel_service_connect:
 * @service: CamelService object
 * @ex: a CamelException
 *
 * Connect to the service using the parameters it was initialized
 * with.
 *
 * Return value: whether or not the connection succeeded
 **/

gboolean
camel_service_connect (CamelService *service, CamelException *ex)
{
	gboolean ret = FALSE;
	gboolean unreg = FALSE;
	
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);
	g_return_val_if_fail (service->session != NULL, FALSE);
	g_return_val_if_fail (service->url != NULL, FALSE);
	
	CAMEL_SERVICE_LOCK (service, connect_lock);
	
	if (service->status == CAMEL_SERVICE_CONNECTED) {
		CAMEL_SERVICE_UNLOCK (service, connect_lock);
		return TRUE;
	}

	/* Register a separate operation for connecting, so that
	 * the offline code can cancel it.
	 */
	CAMEL_SERVICE_LOCK (service, connect_op_lock);
	service->connect_op = camel_operation_registered ();
	if (!service->connect_op) {
		service->connect_op = camel_operation_new (NULL, NULL);
		camel_operation_register (service->connect_op);
		unreg = TRUE;
	}
	CAMEL_SERVICE_UNLOCK (service, connect_op_lock);

	service->status = CAMEL_SERVICE_CONNECTING;
	ret = CSERV_CLASS (service)->connect (service, ex);
	service->status = ret ? CAMEL_SERVICE_CONNECTED : CAMEL_SERVICE_DISCONNECTED;

	CAMEL_SERVICE_LOCK (service, connect_op_lock);
	if (service->connect_op) {
		if (unreg)
			camel_operation_unregister (service->connect_op);
		
		camel_operation_unref (service->connect_op);
		service->connect_op = NULL;
	}
	CAMEL_SERVICE_UNLOCK (service, connect_op_lock);

	CAMEL_SERVICE_UNLOCK (service, connect_lock);
	
	return ret;
}

static gboolean
service_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	/*service->connect_level--;*/

	/* We let people get away with not having a disconnect
	 * function -- CamelMboxStore, for example. 
	 */
	
	return TRUE;
}

/**
 * camel_service_disconnect:
 * @service: CamelService object
 * @clean: whether or not to try to disconnect cleanly.
 * @ex: a CamelException
 *
 * Disconnect from the service. If @clean is %FALSE, it should not
 * try to do any synchronizing or other cleanup of the connection.
 *
 * Return value: whether or not the disconnection succeeded without
 * errors. (Consult @ex if %FALSE.)
 **/
gboolean
camel_service_disconnect (CamelService *service, gboolean clean,
			  CamelException *ex)
{
	gboolean res = TRUE;
	int unreg = FALSE;

	CAMEL_SERVICE_LOCK (service, connect_lock);
	
	if (service->status != CAMEL_SERVICE_DISCONNECTED
	    && service->status != CAMEL_SERVICE_DISCONNECTING) {
		CAMEL_SERVICE_LOCK (service, connect_op_lock);
		service->connect_op = camel_operation_registered ();
		if (!service->connect_op) {
			service->connect_op = camel_operation_new (NULL, NULL);
			camel_operation_register (service->connect_op);
			unreg = TRUE;
		}
		CAMEL_SERVICE_UNLOCK (service, connect_op_lock);

		service->status = CAMEL_SERVICE_DISCONNECTING;
		res = CSERV_CLASS (service)->disconnect (service, clean, ex);
		service->status = CAMEL_SERVICE_DISCONNECTED;

		CAMEL_SERVICE_LOCK (service, connect_op_lock);
		if (unreg)
			camel_operation_unregister (service->connect_op);

		camel_operation_unref (service->connect_op);
		service->connect_op = NULL;
		CAMEL_SERVICE_UNLOCK (service, connect_op_lock);
	}
	
	CAMEL_SERVICE_UNLOCK (service, connect_lock);
	
	return res;
}

static void
cancel_connect (CamelService *service)
{
	camel_operation_cancel (service->connect_op);
}

/**
 * camel_service_cancel_connect:
 * @service: a service
 *
 * If @service is currently attempting to connect to or disconnect
 * from a server, this causes it to stop and fail. Otherwise it is a
 * no-op.
 **/
void
camel_service_cancel_connect (CamelService *service)
{
	CAMEL_SERVICE_LOCK (service, connect_op_lock);
	if (service->connect_op)
		CSERV_CLASS (service)->cancel_connect (service);
	CAMEL_SERVICE_UNLOCK (service, connect_op_lock);
}

/**
 * camel_service_get_url:
 * @service: a service
 *
 * Returns the URL representing a service. The returned URL must be
 * freed when it is no longer needed. For security reasons, this
 * routine does not return the password.
 *
 * Return value: the url name
 **/
char *
camel_service_get_url (CamelService *service)
{
	return camel_url_to_string (service->url, CAMEL_URL_HIDE_PASSWORD);
}


static char *
get_name (CamelService *service, gboolean brief)
{
	w(g_warning ("CamelService::get_name not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (service))));
	return g_strdup ("???");
}		

/**
 * camel_service_get_name:
 * @service: the service
 * @brief: whether or not to use a briefer form
 *
 * This gets the name of the service in a "friendly" (suitable for
 * humans) form. If @brief is %TRUE, this should be a brief description
 * such as for use in the folder tree. If @brief is %FALSE, it should
 * be a more complete and mostly unambiguous description.
 *
 * Return value: the description, which the caller must free.
 **/
char *
camel_service_get_name (CamelService *service, gboolean brief)
{
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);
	g_return_val_if_fail (service->url, NULL);
	
	return CSERV_CLASS (service)->get_name (service, brief);
}


static char *
get_path (CamelService *service)
{
	CamelProvider *prov = service->provider;
	CamelURL *url = service->url;
	GString *gpath;
	char *path;
	
	/* A sort of ad-hoc default implementation that works for our
	 * current set of services.
	 */
	
	gpath = g_string_new (service->provider->protocol);
	if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_USER)) {
		if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_HOST)) {
			g_string_append_printf (gpath, "/%s@%s",
						url->user ? url->user : "",
						url->host ? url->host : "");
			
			if (url->port)
				g_string_append_printf (gpath, ":%d", url->port);
		} else {
			g_string_append_printf (gpath, "/%s%s", url->user ? url->user : "",
						CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_USER) ? "" : "@");
		}
	} else if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_HOST)) {
		g_string_append_printf (gpath, "/%s%s",
					CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_HOST) ? "" : "@",
					url->host ? url->host : "");
		
		if (url->port)
			g_string_append_printf (gpath, ":%d", url->port);
	}
	
	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_PATH))
		g_string_append_printf (gpath, "%s%s", *url->path == '/' ? "" : "/", url->path);
	
	path = gpath->str;
	g_string_free (gpath, FALSE);
	
	return path;
}		

/**
 * camel_service_get_path:
 * @service: the service
 *
 * This gets a valid UNIX relative path describing the service, which
 * is guaranteed to be different from the path returned for any
 * different service. This path MUST start with the name of the
 * provider, followed by a "/", but after that, it is up to the
 * provider.
 *
 * Return value: the path, which the caller must free.
 **/
char *
camel_service_get_path (CamelService *service)
{
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);
	g_return_val_if_fail (service->url, NULL);
	
	return CSERV_CLASS (service)->get_path (service);
}


/**
 * camel_service_get_session:
 * @service: a service
 *
 * Returns the CamelSession associated with the service.
 *
 * Return value: the session
 **/
CamelSession *
camel_service_get_session (CamelService *service)
{
	return service->session;
}

/**
 * camel_service_get_provider:
 * @service: a service
 *
 * Returns the CamelProvider associated with the service.
 *
 * Return value: the provider
 **/
CamelProvider *
camel_service_get_provider (CamelService *service)
{
	return service->provider;
}

static GList *
query_auth_types (CamelService *service, CamelException *ex)
{
	return NULL;
}

/**
 * camel_service_query_auth_types:
 * @service: a CamelService
 * @ex: a CamelException
 *
 * This is used by the mail source wizard to get the list of
 * authentication types supported by the protocol, and information
 * about them.
 *
 * Return value: a list of CamelServiceAuthType records. The caller
 * must free the list with g_list_free() when it is done with it.
 **/
GList *
camel_service_query_auth_types (CamelService *service, CamelException *ex)
{
	GList *ret;
	
	/* note that we get the connect lock here, which means the callee
	   must not call the connect functions itself */
	CAMEL_SERVICE_LOCK (service, connect_lock);
	ret = CSERV_CLASS (service)->query_auth_types (service, ex);
	CAMEL_SERVICE_UNLOCK (service, connect_lock);
	
	return ret;
}

/* ********************************************************************** */
struct _addrinfo_msg {
	EMsg msg;
	unsigned int cancelled:1;

	/* for host lookup */
	const char *name;
	const char *service;
	int result;
	const struct addrinfo *hints;
	struct addrinfo **res;

	/* for host lookup emulation */
#ifdef NEED_ADDRINFO
	struct hostent hostbuf;
	int hostbuflen;
	char *hostbufmem;
#endif

	/* for name lookup */
	const struct sockaddr *addr;
	socklen_t addrlen;
	char *host;
	int hostlen;
	char *serv;
	int servlen;
	int flags;
};

static void
cs_freeinfo(struct _addrinfo_msg *msg)
{
	g_free(msg->host);
	g_free(msg->serv);
#ifdef NEED_ADDRINFO
	g_free(msg->hostbufmem);
#endif
	g_free(msg);
}

/* returns -1 if cancelled */
static int
cs_waitinfo(void *(worker)(void *), struct _addrinfo_msg *msg, const char *error, CamelException *ex)
{
	EMsgPort *reply_port;
	pthread_t id;
	int err, cancel_fd, cancel = 0, fd;

	cancel_fd = camel_operation_cancel_fd(NULL);
	if (cancel_fd == -1) {
		worker(msg);
		return 0;
	}
	
	reply_port = msg->msg.reply_port = e_msgport_new();
	fd = e_msgport_fd(msg->msg.reply_port);
	if ((err = pthread_create(&id, NULL, worker, msg)) == 0) {
		struct pollfd polls[2];
		int status;

		polls[0].fd = fd;
		polls[0].events = POLLIN;
		polls[1].fd = cancel_fd;
		polls[1].events = POLLIN;

		d(printf("waiting for name return/cancellation in main process\n"));
		do {
			polls[0].revents = 0;
			polls[1].revents = 0;
			status = poll(polls, 2, -1);
		} while (status == -1 && errno == EINTR);

		if (status == -1 || (polls[1].revents & POLLIN)) {
			if (status == -1)
				camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, "%s: %s", error, g_strerror(errno));
			else
				camel_exception_setv(ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled"));
			
			/* We cancel so if the thread impl is decent it causes immediate exit.
			   We detach so we dont need to wait for it to exit if it isn't.
			   We check the reply port incase we had a reply in the mean time, which we free later */
			d(printf("Cancelling lookup thread and leaving it\n"));
			msg->cancelled = 1;
			pthread_detach(id);
			pthread_cancel(id);
			cancel = 1;
		} else {
			struct _addrinfo_msg *reply = (struct _addrinfo_msg *)e_msgport_get(reply_port);

			g_assert(reply == msg);
			d(printf("waiting for child to exit\n"));
			pthread_join(id, NULL);
			d(printf("child done\n"));
		}
	} else {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, "%s: %s: %s", error, _("cannot create thread"), g_strerror(err));
	}
	e_msgport_destroy(reply_port);

	return cancel;
}

#ifdef NEED_ADDRINFO
static void *
cs_getaddrinfo(void *data)
{
	struct _addrinfo_msg *msg = data;
	int herr;
	struct hostent h;
	struct addrinfo *res, *last = NULL;
	struct sockaddr_in *sin;
	in_port_t port = 0;
	int i;

	/* This is a pretty simplistic emulation of getaddrinfo */

	while ((msg->result = e_gethostbyname_r(msg->name, &h, msg->hostbufmem, msg->hostbuflen, &herr)) == ERANGE) {
		pthread_testcancel();
                msg->hostbuflen *= 2;
                msg->hostbufmem = g_realloc(msg->hostbufmem, msg->hostbuflen);
	}
	
	/* If we got cancelled, dont reply, just free it */
	if (msg->cancelled)
		goto cancel;

	/* FIXME: map error numbers across */
	if (msg->result != 0)
		goto reply;

	/* check hints matched */
	if (msg->hints && msg->hints->ai_family && msg->hints->ai_family != h.h_addrtype) {
		msg->result = EAI_FAMILY;
		goto reply;
	}

	/* we only support ipv4 for this interface, even if it could supply ipv6 */
	if (h.h_addrtype != AF_INET) {
		msg->result = EAI_FAMILY;
		goto reply;
	}

	/* check service mapping */
	if (msg->service) {
		const char *p = msg->service;

		while (*p) {
			if (*p < '0' || *p > '9')
				break;
			p++;
		}

		if (*p) {
			const char *socktype = NULL;
			struct servent *serv;

			if (msg->hints && msg->hints->ai_socktype) {
				if (msg->hints->ai_socktype == SOCK_STREAM)
					socktype = "tcp";
				else if (msg->hints->ai_socktype == SOCK_DGRAM)
					socktype = "udp";
			}

			serv = getservbyname(msg->service, socktype);
			if (serv == NULL) {
				msg->result = EAI_NONAME;
				goto reply;
			}
			port = serv->s_port;
		} else {
			port = htons(strtoul(msg->service, NULL, 10));
		}
	}

	for (i=0;h.h_addr_list[i];i++) {
		res = g_malloc0(sizeof(*res));
		if (msg->hints) {
			res->ai_flags = msg->hints->ai_flags;
			if (msg->hints->ai_flags & AI_CANONNAME)
				res->ai_canonname = g_strdup(h.h_name);
			res->ai_socktype = msg->hints->ai_socktype;
			res->ai_protocol = msg->hints->ai_protocol;
		} else {
			res->ai_flags = 0;
			res->ai_socktype = SOCK_STREAM;	/* fudge */
			res->ai_protocol = 0;	/* fudge */
		}
		res->ai_family = AF_INET;
		res->ai_addrlen = sizeof(*sin);
		res->ai_addr = g_malloc(sizeof(*sin));
		sin = (struct sockaddr_in *)res->ai_addr;
		sin->sin_family = AF_INET;
		sin->sin_port = port;
		memcpy(&sin->sin_addr, h.h_addr_list[i], sizeof(sin->sin_addr));

		if (last == NULL) {
			*msg->res = last = res;
		} else {
			last->ai_next = res;
			last = res;
		}
	}
reply:
	e_msgport_reply((EMsg *)msg);
	return NULL;
cancel:
	cs_freeinfo(msg);
	return NULL;
}
#else
static void *
cs_getaddrinfo(void *data)
{
	struct _addrinfo_msg *info = data;

	info->result = getaddrinfo(info->name, info->service, info->hints, info->res);
	
	if (info->cancelled) {
		g_free(info);
	} else {
		e_msgport_reply((EMsg *)info);
	}
	
	return NULL;
}
#endif /* NEED_ADDRINFO */

struct addrinfo *
camel_getaddrinfo(const char *name, const char *service, const struct addrinfo *hints, CamelException *ex)
{
	struct _addrinfo_msg *msg;
	struct addrinfo *res = NULL;
#ifndef ENABLE_IPv6
	struct addrinfo myhints;
#endif
	g_return_val_if_fail(name != NULL, NULL);
	
	if (camel_operation_cancel_check(NULL)) {
		camel_exception_set(ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled"));
		return NULL;
	}

	camel_operation_start_transient(NULL, _("Resolving: %s"), name);

	/* force ipv4 addresses only */
#ifndef ENABLE_IPv6
	if (hints == NULL)
		memset(&myhints, 0, sizeof(myhints));
	else
		memcpy (&myhints, hints, sizeof (myhints));
	
	myhints.ai_family = AF_INET;
	hints = &myhints;
#endif

	msg = g_malloc0(sizeof(*msg));
	msg->name = name;
	msg->service = service;
	msg->hints = hints;
	msg->res = &res;
#ifdef NEED_ADDRINFO
	msg->hostbuflen = 1024;
	msg->hostbufmem = g_malloc(msg->hostbuflen);
#endif	
	if (cs_waitinfo(cs_getaddrinfo, msg, _("Host lookup failed"), ex) == 0) {
		if (msg->result != 0)
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, _("Host lookup failed: %s: %s"),
					      name, gai_strerror (msg->result));
		
		cs_freeinfo(msg);
	} else
		res = NULL;
	
	camel_operation_end(NULL);

	return res;
}

void
camel_freeaddrinfo(struct addrinfo *host)
{
#ifdef NEED_ADDRINFO
	while (host) {
		struct addrinfo *next = host->ai_next;

		g_free(host->ai_canonname);
		g_free(host->ai_addr);
		g_free(host);
		host = next;
	}
#else
	freeaddrinfo(host);
#endif
}

#ifdef NEED_ADDRINFO
static void *
cs_getnameinfo(void *data)
{
	struct _addrinfo_msg *msg = data;
	int herr;
	struct hostent h;
	struct sockaddr_in *sin = (struct sockaddr_in *)msg->addr;

	/* FIXME: error code */
	if (msg->addr->sa_family != AF_INET) {
		msg->result = -1;
		return NULL;
	}

	/* FIXME: honour getnameinfo flags: do we care, not really */

	while ((msg->result = e_gethostbyaddr_r((const char *)&sin->sin_addr, sizeof(sin->sin_addr), AF_INET, &h,
						msg->hostbufmem, msg->hostbuflen, &herr)) == ERANGE) {
		pthread_testcancel ();
                msg->hostbuflen *= 2;
                msg->hostbufmem = g_realloc(msg->hostbufmem, msg->hostbuflen);
	}
	
	if (msg->cancelled)
		goto cancel;

	if (msg->host) {
		g_free(msg->host);
		if (msg->result == 0 && h.h_name && h.h_name[0]) {
			msg->host = g_strdup(h.h_name);
		} else {
			unsigned char *in = (unsigned char *)&sin->sin_addr;
			
			/* sin_addr is always network order which is big-endian */
			msg->host = g_strdup_printf("%u.%u.%u.%u", in[0], in[1], in[2], in[3]);
		}
	}

	/* we never actually use this anyway */
	if (msg->serv)
		sprintf(msg->serv, "%d", sin->sin_port);

	e_msgport_reply((EMsg *)msg);
	return NULL;
cancel:
	cs_freeinfo(msg);
	return NULL;
}
#else
static void *
cs_getnameinfo(void *data)
{
	struct _addrinfo_msg *msg = data;

	/* there doens't appear to be a return code which says host or serv buffers are too short, lengthen them */
	msg->result = getnameinfo(msg->addr, msg->addrlen, msg->host, msg->hostlen, msg->serv, msg->servlen, msg->flags);
	
	if (msg->cancelled)
		cs_freeinfo(msg);
	else
		e_msgport_reply((EMsg *)msg);

	return NULL;
}
#endif

int
camel_getnameinfo(const struct sockaddr *sa, socklen_t salen, char **host, char **serv, int flags, CamelException *ex)
{
	struct _addrinfo_msg *msg;
	int result;

	if (camel_operation_cancel_check(NULL)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled"));
		return -1;
	}

	camel_operation_start_transient(NULL, _("Resolving address"));

	msg = g_malloc0(sizeof(*msg));
	msg->addr = sa;
	msg->addrlen = salen;
	if (host) {
		msg->hostlen = NI_MAXHOST;
		msg->host = g_malloc(msg->hostlen);
		msg->host[0] = 0;
	}
	if (serv) {
		msg->servlen = NI_MAXSERV;
		msg->serv = g_malloc(msg->servlen);
		msg->serv[0] = 0;
	}
	msg->flags = flags;
#ifdef NEED_ADDRINFO
	msg->hostbuflen = 1024;
	msg->hostbufmem = g_malloc(msg->hostbuflen);
#endif
	cs_waitinfo(cs_getnameinfo, msg, _("Name lookup failed"), ex);

	if ((result = msg->result) != 0)
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, _("Name lookup failed: %s"),
				      gai_strerror (result));

	if (host)
		*host = g_strdup(msg->host);
	if (serv)
		*serv = g_strdup(msg->serv);

	g_free(msg->host);
	g_free(msg->serv);
	g_free(msg);

	camel_operation_end(NULL);

	return result;
}

