/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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

#ifndef _CAMEL_ADDRESS_H
#define _CAMEL_ADDRESS_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <camel/camel-object.h>

#define CAMEL_ADDRESS(obj)         CAMEL_CHECK_CAST (obj, camel_address_get_type (), CamelAddress)
#define CAMEL_ADDRESS_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_address_get_type (), CamelAddressClass)
#define CAMEL_IS_ADDRESS(obj)      CAMEL_CHECK_TYPE (obj, camel_address_get_type ())

typedef struct _CamelAddressClass CamelAddressClass;

struct _CamelAddress {
	CamelObject parent;

	GPtrArray *addresses;

	struct _CamelAddressPrivate *priv;
};

struct _CamelAddressClass {
	CamelObjectClass parent_class;

	int   (*decode)		(CamelAddress *, const char *raw);
	char *(*encode)		(CamelAddress *);

	int   (*unformat)	(CamelAddress *, const char *raw);
	char *(*format)		(CamelAddress *);

	int   (*cat)		(CamelAddress *, const CamelAddress *);

	void  (*remove)		(CamelAddress *, int index);
};

CamelType	camel_address_get_type	(void);
CamelAddress   *camel_address_new	(void);
CamelAddress   *camel_address_new_clone	(const CamelAddress *);
int		camel_address_length	(CamelAddress *);

int	        camel_address_decode	(CamelAddress *, const char *);
char	       *camel_address_encode	(CamelAddress *);
int	        camel_address_unformat	(CamelAddress *, const char *);
char	       *camel_address_format	(CamelAddress *);

int		camel_address_cat	(CamelAddress *, const CamelAddress *);
int		camel_address_copy	(CamelAddress *, const CamelAddress *);

void		camel_address_remove	(CamelAddress *, int index);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_ADDRESS_H */
