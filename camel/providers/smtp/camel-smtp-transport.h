/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-smtp-transport.h : class for an smtp transfer */

/* 
 * Authors:
 *   Jeffrey Stedfast <fejj@stampede.org>
 *
 * Copyright (C) 2000 Helix Code, Inc. (www.helixcode.com)
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


#ifndef CAMEL_SMTP_TRANSPORT_H
#define CAMEL_SMTP_TRANSPORT_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/


#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "camel-transport.h"

#define CAMEL_SMTP_TRANSPORT_TYPE     (camel_smtp_transport_get_type ())
#define CAMEL_SMTP_TRANSPORT(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SMTP_TRANSPORT_TYPE, CamelSmtpTransport))
#define CAMEL_SMTP_TRANSPORT_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SMTP_TRANSPORT_TYPE, CamelSmtpTransportClass))
#define IS_CAMEL_SMTP_TRANSPORT(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SMTP_TRANSPORT_TYPE))


typedef struct {
	CamelTransport parent_object;

	CamelStream *istream, *ostream;

	gboolean smtp_is_esmtp;

	struct sockaddr_in localaddr;

	GList *esmtp_supported_authtypes;
	
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


