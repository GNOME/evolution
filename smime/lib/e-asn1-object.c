/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-cert.c
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 *
 * Author: Chris Toshok (toshok@ximian.com)
 */

/* The following is the mozilla license blurb, as the bodies some of
   these functions were derived from the mozilla source. */

/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

#include "e-asn1-object.h"

#include "secasn1.h"

struct _EASN1ObjectPrivate {
	PRUint32 tag;
	PRUint32 type;
	gboolean valid_container;

	GList *children;

	char *display_name;
	char *value;

	char *data;
	guint data_len;
};

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class;

static void
e_asn1_object_dispose (GObject *object)
{
	EASN1Object *obj = E_ASN1_OBJECT (object);
	if (obj->priv) {

		if (obj->priv->display_name)
			g_free (obj->priv->display_name);

		if (obj->priv->value)
			g_free (obj->priv->value);

		g_list_foreach (obj->priv->children, (GFunc)g_object_unref, NULL);
		g_list_free (obj->priv->children);

		g_free (obj->priv);
		obj->priv = NULL;
	}
}

static void
e_asn1_object_class_init (EASN1ObjectClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = e_asn1_object_dispose;
}

static void
e_asn1_object_init (EASN1Object *asn1)
{
	asn1->priv = g_new0 (EASN1ObjectPrivate, 1);

	asn1->priv->valid_container = TRUE;
}

GType
e_asn1_object_get_type (void)
{
	static GType asn1_object_type = 0;

	if (!asn1_object_type) {
		static const GTypeInfo asn1_object_info =  {
			sizeof (EASN1ObjectClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_asn1_object_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EASN1Object),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_asn1_object_init,
		};

		asn1_object_type = g_type_register_static (PARENT_TYPE, "EASN1Object", &asn1_object_info, 0);
	}

	return asn1_object_type;
}


/* This function is used to interpret an integer that
   was encoded in a DER buffer. This function is used
   when converting a DER buffer into a nsIASN1Object 
   structure.  This interprets the buffer in data
   as defined by the DER (Distinguised Encoding Rules) of
   ASN1.
*/
static int
get_integer_256 (unsigned char *data, unsigned int nb)
{
	int val;

	switch (nb) {
	case 1:
		val = data[0];
		break;
	case 2:
		val = (data[0] << 8) | data[1];
		break;
	case 3:
		val = (data[0] << 16) | (data[1] << 8) | data[2];
		break;
	case 4:
		val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
		break;
	default:
		return -1;
	}

	return val;
}

/* This function is used to retrieve the lenght of a DER encoded
   item.  It looks to see if this a multibyte length and then
   interprets the buffer accordingly to get the actual length value.
   This funciton is used mostly while parsing the DER headers.
   
   A DER encoded item has the following structure:

   <tag><length<data consisting of lenght bytes>
*/
static guint32
get_der_item_length (unsigned char *data, unsigned char *end,
		     unsigned long *bytesUsed, gboolean *indefinite)
{
	unsigned char lbyte = *data++;
	PRInt32 length = -1;
  
	*indefinite = FALSE;
	if (lbyte >= 0x80) {
		/* Multibyte length */
		unsigned nb = (unsigned) (lbyte & 0x7f);
		if (nb > 4) {
			return -1;
		}
		if (nb > 0) {
			
			if ((data+nb) > end) {
				return -1;
			}
			length = get_integer_256 (data, nb);
			if (length < 0)
				return -1;
		} else {
			*indefinite = TRUE;
			length = 0;
		}
		*bytesUsed = nb+1;
	} else {
		length = lbyte;
		*bytesUsed = 1; 
	}
	return length;
}

static gboolean
build_from_der (EASN1Object *parent, char *data, char *end)
{
	unsigned long bytesUsed;
	gboolean indefinite;
	PRInt32 len;
	PRUint32 type;
	unsigned char code, tagnum;
	EASN1Object *asn1object;

	if (data >= end)
		return TRUE;

	/*
	  A DER item has the form of |tag|len|data
	  tag is one byte and describes the type of elment
	  we are dealing with.
	  len is a DER encoded int telling us how long the data is
	  data is a buffer that is len bytes long and has to be
	  interpreted according to its type.
	*/

	while (data < end) {
		code = *data;
		tagnum = code & SEC_ASN1_TAGNUM_MASK;

		/*
		 * NOTE: This code does not (yet) handle the high-tag-number form!
		 */
		if (tagnum == SEC_ASN1_HIGH_TAG_NUMBER) {
			return FALSE;
		}
		data++;
		len = get_der_item_length (data, end, &bytesUsed, &indefinite);
		data += bytesUsed;
		if ((len < 0) || ((data+len) > end))
			return FALSE;

		if (code & SEC_ASN1_CONSTRUCTED) {
			if (len > 0 || indefinite) {
				switch (code & SEC_ASN1_CLASS_MASK) {
				case SEC_ASN1_UNIVERSAL:
					type = tagnum;
					break;
				case SEC_ASN1_APPLICATION:
					type = E_ASN1_OBJECT_TYPE_APPLICATION;
					break;
				case SEC_ASN1_CONTEXT_SPECIFIC:
					type = E_ASN1_OBJECT_TYPE_CONTEXT_SPECIFIC;
					break;
				case SEC_ASN1_PRIVATE:
					type = E_ASN1_OBJECT_TYPE_PRIVATE;
					break;
				default:
					g_warning ("bad DER");
					return FALSE;
				}

				asn1object = e_asn1_object_new ();
				asn1object->priv->tag = tagnum;
				asn1object->priv->type = type;
				
				if (!build_from_der (asn1object, data, (len == 0) ? end : data + len)) {
					g_object_unref (asn1object);
					return FALSE;
				}
			}
		} else {
			asn1object = e_asn1_object_new ();

			asn1object->priv->type = tagnum;
			asn1object->priv->tag = tagnum;

			/*printableItem->SetData((char*)data, len);*/
		}
		data += len;

		parent->priv->children = g_list_append (parent->priv->children, asn1object);
	}

	return TRUE;
}

EASN1Object*
e_asn1_object_new_from_der (char *data, guint32 len)
{
	EASN1Object *obj = g_object_new (E_TYPE_ASN1_OBJECT, NULL);

	if (!build_from_der (obj, data, data + len)) {
		g_object_unref (obj);
		return NULL;
	}

	return obj;
}

EASN1Object*
e_asn1_object_new (void)
{
	return E_ASN1_OBJECT (g_object_new (E_TYPE_ASN1_OBJECT, NULL));
}


void
e_asn1_object_set_valid_container (EASN1Object *obj, gboolean flag)
{
	obj->priv->valid_container = flag;
}

gboolean
e_asn1_object_is_valid_container (EASN1Object *obj)
{
	return obj->priv->valid_container;
}

PRUint32
e_asn1_object_get_asn1_type (EASN1Object *obj)
{
	return obj->priv->type;
}

PRUint32
e_asn1_object_get_asn1_tag (EASN1Object *obj)
{
	return obj->priv->tag;
}

GList*
e_asn1_object_get_children (EASN1Object *obj)
{
	GList *children = g_list_copy (obj->priv->children);

	g_list_foreach (children, (GFunc)g_object_ref, NULL);

	return children;
}

void
e_asn1_object_append_child (EASN1Object *parent, EASN1Object *child)
{
	parent->priv->children = g_list_append (parent->priv->children, g_object_ref (child));
}

void
e_asn1_object_set_display_name (EASN1Object *obj, const char *name)
{
	g_free (obj->priv->display_name);
	obj->priv->display_name = g_strdup (name);
}

const char*
e_asn1_object_get_display_name (EASN1Object *obj)
{
	return obj->priv->display_name;
}

void
e_asn1_object_set_display_value (EASN1Object *obj, const char *value)
{
	g_free (obj->priv->value);
	obj->priv->value = g_strdup (value);
}

const char*
e_asn1_object_get_display_value (EASN1Object *obj)
{
	return obj->priv->value;
}

void
e_asn1_object_get_data (EASN1Object *obj, char **data, guint32 *len)
{
	*data = obj->priv->data;
	*len = obj->priv->data_len;
}
