/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
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

#include "camel-address.h"


static void camel_address_class_init (CamelAddressClass *klass);
static void camel_address_init       (CamelAddress *obj);
static void camel_address_finalise   (GtkObject *obj);

static CamelObjectClass *camel_address_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_address_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelAddress",
			sizeof (CamelAddress),
			sizeof (CamelAddressClass),
			(GtkClassInitFunc) camel_address_class_init,
			(GtkObjectInitFunc) camel_address_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_object_get_type (), &type_info);
	}
	
	return type;
}

static void
camel_address_class_init (CamelAddressClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	camel_address_parent = gtk_type_class (camel_object_get_type ());

	object_class->finalize = camel_address_finalise;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_address_init (CamelAddress *obj)
{
	obj->addresses = g_ptr_array_new();
}

static void
camel_address_finalise (GtkObject *obj)
{
	camel_address_remove((CamelAddress *)obj, -1);

	((GtkObjectClass *)(camel_address_parent))->finalize((GtkObject *)obj);
}

/**
 * camel_address_new:
 *
 * Create a new CamelAddress object.
 * 
 * Return value: A new CamelAddress widget.
 **/
CamelAddress *
camel_address_new (void)
{
	CamelAddress *new = CAMEL_ADDRESS ( gtk_type_new (camel_address_get_type ()));
	return new;
}


/**
 * camel_address_decode:
 * @a: An address.
 * @raw: Raw address description.
 * 
 * Construct a new address from a raw address field.
 * 
 * Return value: Returns the number of addresses found,
 * or -1 if the addresses could not be parsed fully.
 **/
int
camel_address_decode	(CamelAddress *a, const char *raw)
{
	g_return_val_if_fail(IS_CAMEL_ADDRESS(a), -1);

	return ((CamelAddressClass *)((GtkObject *)a)->klass)->decode(a, raw);
}

/**
 * camel_address_encode:
 * @a: 
 * 
 * Encode an address in a format suitable for a raw header.
 * 
 * Return value: The encoded address.
 **/
char *
camel_address_encode	(CamelAddress *a)
{
	g_return_val_if_fail(IS_CAMEL_ADDRESS(a), NULL);

	return ((CamelAddressClass *)((GtkObject *)a)->klass)->encode(a);
}

/**
 * camel_address_remove:
 * @a: 
 * @index: The address to remove, use -1 to remove all address.
 * 
 * Remove an address by index, or all addresses.
 **/
void
camel_address_remove	(CamelAddress *a, int index)
{
	g_return_if_fail(IS_CAMEL_ADDRESS(a));

	if (index == -1) {
		for (index=a->addresses->len; index>-1; index--)
			((CamelAddressClass *)((GtkObject *)a)->klass)->remove(a, index);
	} else {
		((CamelAddressClass *)((GtkObject *)a)->klass)->remove(a, index);
	}
}
