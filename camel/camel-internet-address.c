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

#include "camel-mime-utils.h"
#include "camel-internet-address.h"

static int    internet_decode		(CamelAddress *, const char *raw);
static char * internet_encode		(CamelAddress *);
static void   internet_remove		(CamelAddress *, int index);

static void camel_internet_address_class_init (CamelInternetAddressClass *klass);
static void camel_internet_address_init       (CamelInternetAddress *obj);

static CamelAddressClass *camel_internet_address_parent;

struct _address {
	char *name;
	char *address;
};

static void
camel_internet_address_class_init (CamelInternetAddressClass *klass)
{
	CamelAddressClass *address = (CamelAddressClass *) klass;

	camel_internet_address_parent = CAMEL_ADDRESS_CLASS (camel_type_get_global_classfuncs (camel_address_get_type ()));

	address->decode = internet_decode;
	address->encode = internet_encode;
	address->remove = internet_remove;
}

static void
camel_internet_address_init (CamelInternetAddress *obj)
{
}

CamelType
camel_internet_address_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_address_get_type (), "CamelInternetAddress",
					    sizeof (CamelInternetAddress),
					    sizeof (CamelInternetAddressClass),
					    (CamelObjectClassInitFunc) camel_internet_address_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_internet_address_init,
					    NULL);
	}
	
	return type;
}

static int
internet_decode	(CamelAddress *a, const char *raw)
{
	struct _header_address *ha, *n;

	/* Should probably use its own decoder or something */
	ha = header_address_decode(raw);
	if (ha) {
		n = ha;
		while (n) {
			if (n->type == HEADER_ADDRESS_NAME) {
				camel_internet_address_add((CamelInternetAddress *)a, n->name, n->v.addr);
			} else if (n->type == HEADER_ADDRESS_GROUP) {
				struct _header_address *g = n->v.members;
				while (g) {
					if (g->type == HEADER_ADDRESS_NAME)
						camel_internet_address_add((CamelInternetAddress *)a, g->name, g->v.addr);
					/* otherwise, its an error, infact */
					g = g->next;
				}
			}
			n = n->next;
		}
		header_address_list_clear(&ha);
	}
	return 0;
}

static char * internet_encode		(CamelAddress *a)
{
	int i;
	GString *out;
	char *ret;

	if (a->addresses->len == 0)
		return NULL;

	out = g_string_new("");

	for (i=0;i<a->addresses->len;i++) {
		struct _address *addr = g_ptr_array_index( a->addresses, i );
		char *name = header_encode_string(addr->name);

		if (i!=0)
			g_string_append(out, ", ");

		if (name) {
			if (*name)
				g_string_sprintfa(out, "%s <%s>", name, addr->address);
			g_free(name);
		} else
			g_string_sprintfa(out, "%s", addr->address);
	}
	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

static void   internet_remove		(CamelAddress *a, int index)
{
	struct _address *addr;

	if (index <0 || index >= a->addresses->len)
		return;

	addr = g_ptr_array_index( a->addresses, index);
	g_free(addr->name);
	g_free(addr->address);
	g_free(addr);
	g_ptr_array_remove_index( a->addresses, index);
}

/**
 * camel_internet_address_new:
 *
 * Create a new CamelInternetAddress object.
 * 
 * Return value: A new CamelInternetAddress object.
 **/
CamelInternetAddress *
camel_internet_address_new (void)
{
	CamelInternetAddress *new = CAMEL_INTERNET_ADDRESS ( camel_object_new (camel_internet_address_get_type ()));
	return new;
}

/**
 * camel_internet_address_add:
 * @a: 
 * @name: 
 * @address: 
 * 
 * Add a new internet address to the address object.
 * 
 * Return value: Index of added entry.
 **/
int
camel_internet_address_add	(CamelInternetAddress *a, const char *name, const char *address)
{
	struct _address *new;
	int index;

	g_return_val_if_fail(IS_CAMEL_INTERNET_ADDRESS(a), -1);

	new = g_malloc(sizeof(*new));
	new->name = g_strdup(name);
	new->address = g_strdup(address);
	index = ((CamelAddress *)a)->addresses->len;
	g_ptr_array_add(((CamelAddress *)a)->addresses, new);

	return index;
}

/**
 * camel_internet_address_get:
 * @a: 
 * @index: 
 * @namep: Holder for the returned name, or NULL, if not required.
 * @addressp: Holder for the returned address, or NULL, if not required.
 * 
 * Get the address at @index.
 * 
 * Return value: TRUE if such an address exists, or FALSE otherwise.
 **/
gboolean
camel_internet_address_get	(const CamelInternetAddress *a, int index, const char **namep, const char **addressp)
{
	struct _address *addr;

	g_return_val_if_fail(IS_CAMEL_INTERNET_ADDRESS(a), -1);
	g_return_val_if_fail(index >= 0, -1);

	if (index >= ((CamelAddress *)a)->addresses->len)
		return FALSE;

	addr = g_ptr_array_index( ((CamelAddress *)a)->addresses, index);
	if (namep)
		*namep = addr->name;
	if (addressp)
		*addressp = addr->address;
	return TRUE;
}

/**
 * camel_internet_address_find_name:
 * @a: 
 * @name: 
 * @addressp: Holder for address part, or NULL, if not required.
 * 
 * Find address by real name.
 * 
 * Return value: The index of the address matching the name, or -1
 * if no match was found.
 **/
int
camel_internet_address_find_name(CamelInternetAddress *a, const char *name, const char **addressp)
{
	struct _address *addr;
	int i, len;

	g_return_val_if_fail(IS_CAMEL_INTERNET_ADDRESS(a), -1);

	len = ((CamelAddress *)a)->addresses->len;
	for (i=0;i<len;i++) {
		addr = g_ptr_array_index( ((CamelAddress *)a)->addresses, i );
		if (!strcmp(addr->name, name)) {
			if (addressp)
				*addressp = addr->address;
			return i;
		}
	}
	return -1;
}

/**
 * camel_internet_address_find_address:
 * @a: 
 * @address: 
 * @namep: Return for the matching name, or NULL, if not required.
 * 
 * Find an address by address.
 * 
 * Return value: The index of the address, or -1 if not found.
 **/
int
camel_internet_address_find_address(CamelInternetAddress *a, const char *address, const char **namep)
{
	struct _address *addr;
	int i, len;

	g_return_val_if_fail(IS_CAMEL_INTERNET_ADDRESS(a), -1);

	len = ((CamelAddress *)a)->addresses->len;
	for (i=0;i<len;i++) {
		addr = g_ptr_array_index( ((CamelAddress *)a)->addresses, i );
		if (!strcmp(addr->address, address)) {
			if (namep)
				*namep = addr->name;
			return i;
		}
	}
	return -1;
}
