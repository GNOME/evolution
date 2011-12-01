/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* The following is the mozilla license blurb, as the bodies some of
 * these functions were derived from the mozilla source. */
/*
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 */

/*
 * Author: Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-asn1-object.h"

#include "secasn1.h"

#define E_ASN1_OBJECT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ASN1_OBJECT, EASN1ObjectPrivate))

struct _EASN1ObjectPrivate {
	PRUint32 tag;
	PRUint32 type;
	gboolean valid_container;

	GList *children;

	gchar *display_name;
	gchar *value;

	gchar *data;
	guint data_len;
};

G_DEFINE_TYPE (EASN1Object, e_asn1_object, G_TYPE_OBJECT)

static void
e_asn1_object_finalize (GObject *object)
{
	EASN1ObjectPrivate *priv;

	priv = E_ASN1_OBJECT_GET_PRIVATE (object);

	g_free (priv->display_name);
	g_free (priv->value);

	g_list_free_full (priv->children, (GDestroyNotify) g_object_unref);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_asn1_object_parent_class)->finalize (object);
}

static void
e_asn1_object_class_init (EASN1ObjectClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EASN1ObjectPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_asn1_object_finalize;
}

static void
e_asn1_object_init (EASN1Object *asn1)
{
	asn1->priv = E_ASN1_OBJECT_GET_PRIVATE (asn1);

	asn1->priv->valid_container = TRUE;
}

/* This function is used to interpret an integer that
 * was encoded in a DER buffer. This function is used
 * when converting a DER buffer into a nsIASN1Object
 * structure.  This interprets the buffer in data
 * as defined by the DER (Distinguised Encoding Rules) of
 * ASN1.
*/
static gint
get_integer_256 (guchar *data,
                 guint nb)
{
	gint val;

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
 * item.  It looks to see if this a multibyte length and then
 * interprets the buffer accordingly to get the actual length value.
 * This funciton is used mostly while parsing the DER headers.
 *
 * A DER encoded item has the following structure:
 *
 * <tag><length<data consisting of lenght bytes>
 */
static guint32
get_der_item_length (guchar *data,
                     guchar *end,
                     gulong *bytesUsed,
                     gboolean *indefinite)
{
	guchar lbyte = *data++;
	PRInt32 length = -1;

	*indefinite = FALSE;
	if (lbyte >= 0x80) {
		/* Multibyte length */
		guint nb = (guint) (lbyte & 0x7f);
		if (nb > 4) {
			return -1;
		}
		if (nb > 0) {

			if ((data + nb) > end) {
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
build_from_der (EASN1Object *parent,
                gchar *data,
                gchar *end)
{
	gulong bytesUsed;
	gboolean indefinite;
	PRInt32 len;
	PRUint32 type;
	guchar code, tagnum;
	EASN1Object *asn1object = NULL;

	if (data >= end)
		return TRUE;

	/*
	 * A DER item has the form of |tag|len|data
	 * tag is one byte and describes the type of elment
	 * we are dealing with.
	 * len is a DER encoded gint telling us how long the data is
	 * data is a buffer that is len bytes long and has to be
	 * interpreted according to its type.
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
		len = get_der_item_length (
			(guchar *) data, (guchar *) end,
			&bytesUsed, &indefinite);
		data += bytesUsed;
		if ((len < 0) || ((data + len) > end))
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

				if (!build_from_der (
					asn1object, data,
					(len == 0) ? end : data + len)) {
					g_object_unref (asn1object);
					return FALSE;
				}
			}
		} else {
			asn1object = e_asn1_object_new ();

			asn1object->priv->type = tagnum;
			asn1object->priv->tag = tagnum;

			/*printableItem->SetData((gchar *)data, len);*/
		}
		data += len;

		parent->priv->children = g_list_append (parent->priv->children, asn1object);
	}

	return TRUE;
}

EASN1Object *
e_asn1_object_new_from_der (gchar *data,
                            guint32 len)
{
	EASN1Object *obj = g_object_new (E_TYPE_ASN1_OBJECT, NULL);

	if (!build_from_der (obj, data, data + len)) {
		g_object_unref (obj);
		return NULL;
	}

	return obj;
}

EASN1Object *
e_asn1_object_new (void)
{
	return E_ASN1_OBJECT (g_object_new (E_TYPE_ASN1_OBJECT, NULL));
}

void
e_asn1_object_set_valid_container (EASN1Object *obj,
                                   gboolean flag)
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

GList *
e_asn1_object_get_children (EASN1Object *obj)
{
	GList *children = g_list_copy (obj->priv->children);

	g_list_foreach (children, (GFunc) g_object_ref, NULL);

	return children;
}

void
e_asn1_object_append_child (EASN1Object *parent,
                            EASN1Object *child)
{
	parent->priv->children = g_list_append (
		parent->priv->children, g_object_ref (child));
}

void
e_asn1_object_set_display_name (EASN1Object *obj,
                                const gchar *name)
{
	g_free (obj->priv->display_name);
	obj->priv->display_name = g_strdup (name);
}

const gchar *
e_asn1_object_get_display_name (EASN1Object *obj)
{
	return obj->priv->display_name;
}

void
e_asn1_object_set_display_value (EASN1Object *obj,
                                 const gchar *value)
{
	g_free (obj->priv->value);
	obj->priv->value = g_strdup (value);
}

const gchar *
e_asn1_object_get_display_value (EASN1Object *obj)
{
	return obj->priv->value;
}

void
e_asn1_object_get_data (EASN1Object *obj,
                        gchar **data,
                        guint32 *len)
{
	*data = obj->priv->data;
	*len = obj->priv->data_len;
}
