/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _E_ASN1_OBJECT_H_
#define _E_ASN1_OBJECT_H_

#include <glib-object.h>

#include "e-cert.h"

#include <nspr.h>

#define E_TYPE_ASN1_OBJECT            (e_asn1_object_get_type ())
#define E_ASN1_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ASN1_OBJECT, EASN1Object))
#define E_ASN1_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ASN1_OBJECT, EASN1ObjectClass))
#define E_IS_ASN1_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ASN1_OBJECT))
#define E_IS_ASN1_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_ASN1_OBJECT))
#define E_ASN1_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_ASN1_OBJECT, EASN1ObjectClass))

typedef struct _EASN1Object EASN1Object;
typedef struct _EASN1ObjectClass EASN1ObjectClass;
typedef struct _EASN1ObjectPrivate EASN1ObjectPrivate;

enum {
  E_ASN1_OBJECT_TYPE_APPLICATION,
  E_ASN1_OBJECT_TYPE_CONTEXT_SPECIFIC,
  E_ASN1_OBJECT_TYPE_PRIVATE
};

struct _EASN1Object {
	GObject parent;

	EASN1ObjectPrivate *priv;
};

struct _EASN1ObjectClass {
	GObjectClass parent_class;

	/* Padding for future expansion */
	void (*_ecert_reserved0) (void);
	void (*_ecert_reserved1) (void);
	void (*_ecert_reserved2) (void);
	void (*_ecert_reserved3) (void);
	void (*_ecert_reserved4) (void);
};

EASN1Object     *e_asn1_object_new_from_cert      (ECert *cert);
EASN1Object     *e_asn1_object_new_from_der       (char *data, guint32 len);
EASN1Object     *e_asn1_object_new                (void);

gboolean         e_asn1_object_is_valid_container (EASN1Object *obj);
PRUint32         e_asn1_object_get_asn1_type      (EASN1Object *obj);
PRUint32         e_asn1_object_get_asn1_tag       (EASN1Object *obj);
GList           *e_asn1_object_get_children       (EASN1Object *obj);
const char      *e_asn1_object_get_display_name   (EASN1Object *obj);
const char      *e_asn1_object_get_display_value  (EASN1Object *obj);

void             e_asn1_object_get_data           (EASN1Object *obj, char **data, guint32 *len);

GType            e_asn1_object_get_type (void);

#endif /* _E_ASN1_OBJECT_H_ */
