/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: 
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CAMEL_NEWS_ADDRESS_H
#define _CAMEL_NEWS_ADDRESS_H

#include <camel/camel-address.h>

#define CAMEL_NEWS_ADDRESS(obj)         CAMEL_CHECK_CAST (obj, camel_news_address_get_type (), CamelNewsAddress)
#define CAMEL_NEWS_ADDRESS_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_news_address_get_type (), CamelNewsAddressClass)
#define IS_CAMEL_NEWS_ADDRESS(obj)      CAMEL_CHECK_TYPE (obj, camel_news_address_get_type ())

typedef struct _CamelNewsAddressClass CamelNewsAddressClass;

struct _CamelNewsAddress {
	CamelAddress parent;

	struct _CamelNewsAddressPrivate *priv;
};

struct _CamelNewsAddressClass {
	CamelAddressClass parent_class;
};

guint		camel_news_address_get_type	(void);
CamelNewsAddress      *camel_news_address_new	(void);

#endif /* ! _CAMEL_NEWS_ADDRESS_H */
