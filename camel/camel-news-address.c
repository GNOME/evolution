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

#include "camel-news-address.h"


static void camel_news_address_class_init (CamelNewsAddressClass *klass);

static CamelAddressClass *camel_news_address_parent;

static void
camel_news_address_class_init (CamelNewsAddressClass *klass)
{
	camel_news_address_parent = CAMEL_ADDRESS_CLASS (camel_type_get_global_classfuncs (camel_address_get_type ()));
}


CamelType
camel_news_address_get_type (void)
{
	static guint type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_address_get_type (), "CamelNewsAddress",
					    sizeof (CamelNewsAddress),
					    sizeof (CamelNewsAddressClass),
					    (CamelObjectClassInitFunc) camel_news_address_class_init,
					    NULL,
					    NULL,
					    NULL);
	}
	
	return type;
}

/**
 * camel_news_address_new:
 *
 * Create a new CamelNewsAddress object.
 * 
 * Return value: A new CamelNewsAddress widget.
 **/
CamelNewsAddress *
camel_news_address_new (void)
{
	CamelNewsAddress *new = CAMEL_NEWS_ADDRESS ( camel_object_new (camel_news_address_get_type ()));
	return new;
}
