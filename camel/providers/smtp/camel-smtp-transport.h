/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-smtp-transport.h : class for an smtp transfer */

/* 
 * Authors:
 *   Jeffrey Stedfast <fejj@stampede.org>
 *
 * Copyright (C) 2000 Ximian, Inc. (www.ximian.com)
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

#ifndef CAMEL_SMTP_TRANSPORT_H
#define CAMEL_SMTP_TRANSPORT_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "camel-transport.h"
#include "camel-tcp-stream.h"

#define CAMEL_SMTP_TRANSPORT_TYPE     (camel_smtp_transport_get_type ())
#define CAMEL_SMTP_TRANSPORT(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SMTP_TRANSPORT_TYPE, CamelSmtpTransport))
#define CAMEL_SMTP_TRANSPORT_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SMTP_TRANSPORT_TYPE, CamelSmtpTransportClass))
#define CAMEL_IS_SMTP_TRANSPORT(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SMTP_TRANSPORT_TYPE))

#define CAMEL_SMTP_TRANSPORT_IS_ESMTP               (1 << 0)
#define CAMEL_SMTP_TRANSPORT_8BITMIME               (1 << 1)
#define CAMEL_SMTP_TRANSPORT_ENHANCEDSTATUSCODES    (1 << 2)
#define CAMEL_SMTP_TRANSPORT_STARTTLS               (1 << 3)

#define CAMEL_SMTP_TRANSPORT_AUTH_EQUAL             (1 << 4)  /* set if we are using authtypes from a broken AUTH= */

typedef struct {
	CamelTransport parent_object;
	
	CamelStream *istream, *ostream;
	
	guint32 flags;
	
	gboolean connected;
	struct sockaddr *localaddr;
	socklen_t localaddrlen;
	
	GHashTable *authtypes;
} CamelSmtpTransport;

typedef struct {
	CamelTransportClass parent_class;

} CamelSmtpTransportClass;

/* Standard Camel function */
CamelType camel_smtp_transport_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SMTP_TRANSPORT_H */
