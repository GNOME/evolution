/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <string.h>
#include <time.h>

#include "camel-sasl-popb4smtp.h"
#include "camel-service.h"
#include "camel-session.h"

CamelServiceAuthType camel_sasl_popb4smtp_authtype = {
	N_("POP before SMTP"),

	N_("This option will authorise a POP connection before attempting SMTP"),

	"POPB4SMTP",
	FALSE,
};

/* last time the pop was accessed (through the auth method anyway), *time_t */
static GHashTable *poplast;

/* use 1 hour as our pop timeout */
#define POPB4SMTP_TIMEOUT (60*60)

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define POPB4SMTP_LOCK(l) pthread_mutex_lock(&l)
#define POPB4SMTP_UNLOCK(l) pthread_mutex_unlock(&l)

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslPOPB4SMTP */
#define CSP_CLASS(so) CAMEL_SASL_POPB4SMTP_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *popb4smtp_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);

static void
camel_sasl_popb4smtp_class_init (CamelSaslPOPB4SMTPClass *camel_sasl_popb4smtp_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_popb4smtp_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = popb4smtp_challenge;

	poplast = g_hash_table_new(g_str_hash, g_str_equal);
}

CamelType
camel_sasl_popb4smtp_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_sasl_get_type (),
					    "CamelSaslPOPB4SMTP",
					    sizeof (CamelSaslPOPB4SMTP),
					    sizeof (CamelSaslPOPB4SMTPClass),
					    (CamelObjectClassInitFunc) camel_sasl_popb4smtp_class_init,
					    NULL,
					    NULL,
					    NULL);
	}
	
	return type;
}

static GByteArray *
popb4smtp_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	char *popuri;
	CamelSession *session = sasl->service->session;
	CamelStore *store;
	time_t now, *timep;

	sasl->authenticated = FALSE;

	popuri = camel_session_get_password (session, sasl->service, NULL, _("POP Source URI"), "popb4smtp_uri", 0, ex);

	if (popuri == NULL) {
		camel_exception_setv(ex, 1, _("POP Before SMTP auth using an unknown transport"));
		return NULL;
	}

	if (strncasecmp(popuri, "pop:", 4) != 0) {
		camel_exception_setv(ex, 1, _("POP Before SMTP auth using a non-pop source"));
		return NULL;
	}

	/* check if we've done it before recently in this session */
	now = time(0);

	/* need to lock around the whole thing until finished with timep */

	POPB4SMTP_LOCK(lock);
	timep = g_hash_table_lookup(poplast, popuri);
	if (timep) {
		if ((*timep + POPB4SMTP_TIMEOUT) > now) {
			sasl->authenticated = TRUE;
			POPB4SMTP_UNLOCK(lock);
			g_free(popuri);
			return NULL;
		}
	} else {
		timep = g_malloc0(sizeof(*timep));
		g_hash_table_insert(poplast, g_strdup(popuri), timep);
	}

	/* connect to pop session */
	store = camel_session_get_store(session, popuri, ex);
	if (store) {
		sasl->authenticated = TRUE;
		camel_object_unref((CamelObject *)store);
		*timep = now;
	} else {
		sasl->authenticated = FALSE;
		*timep = 0;
	}

	POPB4SMTP_UNLOCK(lock);

	g_free(popuri);
	
	return NULL;
}
