/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <NotZed@ximian.com>
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
 */


#ifndef _CAMEL_NEWS_ADDRESS_H
#define _CAMEL_NEWS_ADDRESS_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-address.h>

#define CAMEL_NEWS_ADDRESS(obj)         CAMEL_CHECK_CAST (obj, camel_news_address_get_type (), CamelNewsAddress)
#define CAMEL_NEWS_ADDRESS_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_news_address_get_type (), CamelNewsAddressClass)
#define CAMEL_IS_NEWS_ADDRESS(obj)      CAMEL_CHECK_TYPE (obj, camel_news_address_get_type ())

typedef struct _CamelNewsAddressClass CamelNewsAddressClass;

struct _CamelNewsAddress {
	CamelAddress parent;

	struct _CamelNewsAddressPrivate *priv;
};

struct _CamelNewsAddressClass {
	CamelAddressClass parent_class;
};

CamelType		camel_news_address_get_type	(void);
CamelNewsAddress      *camel_news_address_new	(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_NEWS_ADDRESS_H */
