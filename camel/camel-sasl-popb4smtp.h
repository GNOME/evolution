/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifndef CAMEL_SASL_POPB4SMTP_H
#define CAMEL_SASL_POPB4SMTP_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-sasl.h>

#define CAMEL_SASL_POPB4SMTP_TYPE     (camel_sasl_popb4smtp_get_type ())
#define CAMEL_SASL_POPB4SMTP(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SASL_POPB4SMTP_TYPE, CamelSaslPOPB4SMTP))
#define CAMEL_SASL_POPB4SMTP_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SASL_POPB4SMTP_TYPE, CamelSaslPOPB4SMTPClass))
#define CAMEL_IS_SASL_POPB4SMTP(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SASL_POPB4SMTP_TYPE))

typedef struct _CamelSaslPOPB4SMTP {
	CamelSasl parent_object;

} CamelSaslPOPB4SMTP;


typedef struct _CamelSaslPOPB4SMTPClass {
	CamelSaslClass parent_class;
	
} CamelSaslPOPB4SMTPClass;


/* Standard Camel function */
CamelType camel_sasl_popb4smtp_get_type (void);

extern CamelServiceAuthType camel_sasl_popb4smtp_authtype;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SASL_POPB4SMTP_H */
