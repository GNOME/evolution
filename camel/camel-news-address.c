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
static void camel_news_address_init       (CamelNewsAddress *obj);

static CamelAddressClass *camel_news_address_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_news_address_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelNewsAddress",
			sizeof (CamelNewsAddress),
			sizeof (CamelNewsAddressClass),
			(GtkClassInitFunc) camel_news_address_class_init,
			(GtkObjectInitFunc) camel_news_address_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_address_get_type (), &type_info);
	}
	
	return type;
}

static void
camel_news_address_class_init (CamelNewsAddressClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	camel_news_address_parent = gtk_type_class (camel_address_get_type ());

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_news_address_init (CamelNewsAddress *obj)
{
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
	CamelNewsAddress *new = CAMEL_NEWS_ADDRESS ( gtk_type_new (camel_news_address_get_type ()));
	return new;
}
