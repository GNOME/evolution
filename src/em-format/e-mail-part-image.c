/*
 * e-mail-part-image.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "e-mail-part-image.h"

G_DEFINE_TYPE (
	EMailPartImage,
	e_mail_part_image,
	E_TYPE_MAIL_PART)

static void
mail_part_image_constructed (GObject *object)
{
	EMailPart *part;
	CamelMimePart *mime_part;
	CamelContentType *content_type;
	const gchar *content_id;
	const gchar *disposition;

	part = E_MAIL_PART (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_part_image_parent_class)->constructed (object);

	e_mail_part_set_is_attachment (part, TRUE);

	mime_part = e_mail_part_ref_mime_part (part);

	content_id = camel_mime_part_get_content_id (mime_part);
	content_type = camel_mime_part_get_content_type (mime_part);
	disposition = camel_mime_part_get_disposition (mime_part);

	if (content_id != NULL) {
		gchar *cid;

		cid = g_strconcat ("cid:", content_id, NULL);
		e_mail_part_set_cid (part, cid);
		g_free (cid);
	}

	if (content_type != NULL) {
		gchar *mime_type;

		mime_type = camel_content_type_simple (content_type);
		e_mail_part_set_mime_type (part, mime_type);
		g_free (mime_type);
	} else {
		e_mail_part_set_mime_type (part, "image/*");
	}

	if (disposition == NULL)
		disposition = "inline";

	part->is_hidden =
		(content_id != NULL) &&
		(g_ascii_strcasecmp (disposition, "attachment") != 0);

	g_object_unref (mime_part);
}

static void
e_mail_part_image_class_init (EMailPartImageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_part_image_constructed;
}

static void
e_mail_part_image_init (EMailPartImage *part)
{
}

EMailPart *
e_mail_part_image_new (CamelMimePart *mime_part,
                       const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_IMAGE,
		"id", id, "mime-part", mime_part, NULL);
}

