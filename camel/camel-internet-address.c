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

#include <stdio.h>

#define d(x) 

static int    internet_decode		(CamelAddress *, const char *raw);
static char * internet_encode		(CamelAddress *);
static int    internet_unformat		(CamelAddress *, const char *raw);
static char * internet_format		(CamelAddress *);
static int    internet_cat		(CamelAddress *dest, const CamelAddress *source);
static void   internet_remove		(CamelAddress *, int index);

static void camel_internet_address_class_init (CamelInternetAddressClass *klass);
static void camel_internet_address_init       (CamelInternetAddress *obj);

static CamelAddressClass *camel_internet_address_parent;

struct _address {
	char *name;
	char *address;
};

static void
camel_internet_address_class_init(CamelInternetAddressClass *klass)
{
	CamelAddressClass *address = (CamelAddressClass *) klass;

	camel_internet_address_parent = CAMEL_ADDRESS_CLASS(camel_type_get_global_classfuncs(camel_address_get_type()));

	address->decode = internet_decode;
	address->encode = internet_encode;
	address->unformat = internet_unformat;
	address->format = internet_format;
	address->remove = internet_remove;
	address->cat = internet_cat;
}

static void
camel_internet_address_init(CamelInternetAddress *obj)
{
}

CamelType
camel_internet_address_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_address_get_type(), "CamelInternetAddress",
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
	int count = a->addresses->len;

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
					/* otherwise, it's an error, infact */
					g = g->next;
				}
			}
			n = n->next;
		}
		header_address_list_clear(&ha);
	}
	
	return a->addresses->len - count;
}

static char *
internet_encode	(CamelAddress *a)
{
	int i;
	GString *out;
	char *ret;
	
	if (a->addresses->len == 0)
		return NULL;
	
	out = g_string_new("");
	
	for (i = 0;i < a->addresses->len; i++) {
		struct _address *addr = g_ptr_array_index(a->addresses, i);
		char *enc;

		if (i != 0)
			g_string_append(out, ", ");

		enc = camel_internet_address_encode_address(addr->name, addr->address);
		g_string_sprintfa(out, "%s", enc);
		g_free(enc);
	}
	
	ret = out->str;
	g_string_free(out, FALSE);
	
	return ret;
}

static int
internet_unformat(CamelAddress *a, const char *raw)
{
	char *buffer, *p, *name, *addr;
	int c;
	int count = a->addresses->len;

	if (raw == NULL)
		return 0;

	d(printf("unformatting address: %s\n", raw));

	/* we copy, so we can modify as we go */
	buffer = g_strdup(raw);

	/* this can be simpler than decode, since there are much fewer rules */
	p = buffer;
	name = NULL;
	addr = p;
	do {
		c = (unsigned char)*p++;
		switch (c) {
			/* HMMM.  Not sure we need this, we dont quote the names anyway ... */
		case '"':
			while (*p && *p != '"')
				p++;
			break;
		case '<':
			if (name == NULL)
				name = addr;
			addr = p;
			addr[-1] = 0;
			while (*p && *p != '>')
				p++;
			if (*p == 0)
				break;
			p++;
			/* falls through */
		case ',':
			p[-1] = 0;
			/* falls through */
		case 0:
			if (name)
				name = g_strstrip(name);
			addr = g_strstrip(addr);
			if (addr[0]) {
				d(printf("found address: %s <%s>\n", name, addr));
				camel_internet_address_add((CamelInternetAddress *)a, name, addr);
			}
			name = NULL;
			addr = p;
			break;
		}
	} while (c);

	g_free(buffer);

	return a->addresses->len - count;
}

static char *
internet_format	(CamelAddress *a)
{
	int i;
	GString *out;
	char *ret;
	
	if (a->addresses->len == 0)
		return NULL;
	
	out = g_string_new("");
	
	for (i = 0;i < a->addresses->len; i++) {
		struct _address *addr = g_ptr_array_index(a->addresses, i);
		char *enc;

		if (i != 0)
			g_string_append(out, ", ");

		enc = camel_internet_address_format_address(addr->name, addr->address);
		g_string_sprintfa(out, "%s", enc);
		g_free(enc);
	}
	
	ret = out->str;
	g_string_free(out, FALSE);
	
	return ret;
}

static int    internet_cat		(CamelAddress *dest, const CamelAddress *source)
{
	int i;

	g_assert(IS_CAMEL_INTERNET_ADDRESS(source));

	for (i=0;i<source->addresses->len;i++) {
		struct _address *addr = g_ptr_array_index(source->addresses, i);
		camel_internet_address_add((CamelInternetAddress *)dest, addr->name, addr->address);
	}

	return i;
}

static void
internet_remove	(CamelAddress *a, int index)
{
	struct _address *addr;
	
	if (index < 0 || index >= a->addresses->len)
		return;
	
	addr = g_ptr_array_index(a->addresses, index);
	g_free(addr->name);
	g_free(addr->address);
	g_free(addr);
	g_ptr_array_remove_index(a->addresses, index);
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
	CamelInternetAddress *new = CAMEL_INTERNET_ADDRESS(camel_object_new(camel_internet_address_get_type()));
	return new;
}

/**
 * camel_internet_address_add:
 * @a: internet address object
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

	g_assert(IS_CAMEL_INTERNET_ADDRESS(a));

	new = g_malloc(sizeof(*new));
	new->name = g_strdup(name);
	new->address = g_strdup(address);
	index = ((CamelAddress *)a)->addresses->len;
	g_ptr_array_add(((CamelAddress *)a)->addresses, new);

	return index;
}

/**
 * camel_internet_address_get:
 * @a: internet address object
 * @index: address's array index
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

	g_assert(IS_CAMEL_INTERNET_ADDRESS(a));

	if (index < 0 || index >= ((CamelAddress *)a)->addresses->len)
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

	g_assert(IS_CAMEL_INTERNET_ADDRESS(a));

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

	g_assert(IS_CAMEL_INTERNET_ADDRESS(a));

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

/**
 * camel_internet_address_encode_address:
 * @name: 
 * @addr: 
 * 
 * Encode a single address ready for internet usage.
 * 
 * Return value: The encoded address.
 **/
char *
camel_internet_address_encode_address(const char *real, const char *addr)
{
	char *name = header_encode_phrase(real);
	char *ret = NULL;

	g_assert(addr);

	if (name && name[0])
		ret = g_strdup_printf("%s <%s>", name, addr);
	else
		ret = g_strdup_printf("%s", addr);

	g_free(name);

	return ret;
}

/**
 * camel_internet_address_format_address:
 * @name: 
 * @addr: 
 * 
 * Function to format a single address, suitable for display.
 * 
 * Return value: 
 **/
char *
camel_internet_address_format_address(const char *name, const char *addr)
{
	char *ret = NULL;

	g_assert(addr);

#warning "If name contains a quote, then we're thrown for six ... "
	if (name && name[0])
		ret = g_strdup_printf("%s <%s>", name, addr);
	else
		ret = g_strdup_printf("%s", addr);

	return ret;
}
