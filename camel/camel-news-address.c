/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors:
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
