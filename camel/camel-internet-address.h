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

#ifndef _CAMEL_INTERNET_ADDRESS_H
#define _CAMEL_INTERNET_ADDRESS_H

#include <gtk/gtk.h>
#include <camel/camel-address.h>

#define CAMEL_INTERNET_ADDRESS(obj)         GTK_CHECK_CAST (obj, camel_internet_address_get_type (), CamelInternetAddress)
#define CAMEL_INTERNET_ADDRESS_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_internet_address_get_type (), CamelInternetAddressClass)
#define IS_CAMEL_INTERNET_ADDRESS(obj)      GTK_CHECK_TYPE (obj, camel_internet_address_get_type ())

typedef struct _CamelInternetAddress      CamelInternetAddress;
typedef struct _CamelInternetAddressClass CamelInternetAddressClass;

struct _CamelInternetAddress {
	CamelAddress parent;

	struct _CamelInternetAddressPrivate *priv;
};

struct _CamelInternetAddressClass {
	CamelAddressClass parent_class;
};

guint			camel_internet_address_get_type	(void);
CamelInternetAddress   *camel_internet_address_new	(void);

int			camel_internet_address_add	(CamelInternetAddress *, const char *, const char *);
gboolean		camel_internet_address_get	(const CamelInternetAddress *, int, const char **, const char **);

int			camel_internet_address_find_name(CamelInternetAddress *, const char *, const char **);
int			camel_internet_address_find_address(CamelInternetAddress *, const char *, const char **);

#endif /* ! _CAMEL_INTERNET_ADDRESS_H */
