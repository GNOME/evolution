/*
 * e-mail-part-attachment.c
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

#include "evolution-config.h"

#include "e-mail-part-attachment.h"

struct _EMailPartAttachmentPrivate {
	EAttachment *attachment;
	gchar *guessed_mime_type;
	gboolean expandable;
};

enum {
	PROP_0,
	PROP_ATTACHMENT,
	PROP_EXPANDABLE
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailPartAttachment, e_mail_part_attachment, E_TYPE_MAIL_PART)

static void
mail_part_attachment_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPANDABLE:
			e_mail_part_attachment_set_expandable (
				E_MAIL_PART_ATTACHMENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_attachment_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ATTACHMENT:
			g_value_take_object (
				value,
				e_mail_part_attachment_ref_attachment (
				E_MAIL_PART_ATTACHMENT (object)));
			return;

		case PROP_EXPANDABLE:
			g_value_set_boolean (
				value,
				e_mail_part_attachment_get_expandable (
				E_MAIL_PART_ATTACHMENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_attachment_dispose (GObject *object)
{
	EMailPartAttachment *self = E_MAIL_PART_ATTACHMENT (object);

	g_clear_object (&self->priv->attachment);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_part_attachment_parent_class)->dispose (object);
}

static void
mail_part_attachment_finalize (GObject *object)
{
	EMailPartAttachment *part = E_MAIL_PART_ATTACHMENT (object);

	g_free (part->part_id_with_attachment);
	g_free (part->priv->guessed_mime_type);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_attachment_parent_class)->
		finalize (object);
}

static void
mail_part_attachment_constructed (GObject *object)
{
	CamelMimePart *mime_part;
	EAttachment *attachment;
	EMailPart *part;
	EMailPartAttachment *self;
	const gchar *cid;

	part = E_MAIL_PART (object);
	self = E_MAIL_PART_ATTACHMENT (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_part_attachment_parent_class)->constructed (object);

	e_mail_part_set_mime_type (part, E_MAIL_PART_ATTACHMENT_MIME_TYPE);
	e_mail_part_set_is_attachment (part, TRUE);

	mime_part = e_mail_part_ref_mime_part (part);

	cid = camel_mime_part_get_content_id (mime_part);
	if (cid != NULL) {
		gchar *cid_uri;

		cid_uri = g_strconcat ("cid:", cid, NULL);
		e_mail_part_set_cid (part, cid_uri);
		g_free (cid_uri);
	}

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	self->priv->attachment = attachment;

	g_object_unref (mime_part);
}

static void
e_mail_part_attachment_class_init (EMailPartAttachmentClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_part_attachment_get_property;
	object_class->set_property = mail_part_attachment_set_property;
	object_class->dispose = mail_part_attachment_dispose;
	object_class->finalize = mail_part_attachment_finalize;
	object_class->constructed = mail_part_attachment_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ATTACHMENT,
		g_param_spec_object (
			"attachment",
			"Attachment",
			"The attachment object",
			E_TYPE_ATTACHMENT,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_EXPANDABLE,
		g_param_spec_boolean (
			"expandable",
			"Expandable",
			"Whether the attachment can be expanded",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_part_attachment_init (EMailPartAttachment *part)
{
	part->priv = e_mail_part_attachment_get_instance_private (part);
	part->priv->expandable = FALSE;
}

EMailPartAttachment *
e_mail_part_attachment_new (CamelMimePart *mime_part,
                            const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_ATTACHMENT,
		"id", id, "mime-part", mime_part, NULL);
}

EAttachment *
e_mail_part_attachment_ref_attachment (EMailPartAttachment *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART_ATTACHMENT (part), NULL);

	return g_object_ref (part->priv->attachment);
}

void
e_mail_part_attachment_set_expandable (EMailPartAttachment *part,
				       gboolean expandable)
{
	g_return_if_fail (E_IS_MAIL_PART_ATTACHMENT (part));

	if ((part->priv->expandable ? 1 : 0) == (expandable ? 1 : 0))
		return;

	part->priv->expandable = expandable;

	g_object_notify (G_OBJECT (part), "expandable");
}

gboolean
e_mail_part_attachment_get_expandable (EMailPartAttachment *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART_ATTACHMENT (part), FALSE);

	return part->priv->expandable;
}

void
e_mail_part_attachment_take_guessed_mime_type (EMailPartAttachment *part,
					       gchar *guessed_mime_type)
{
	g_return_if_fail (E_IS_MAIL_PART_ATTACHMENT (part));

	if (g_strcmp0 (guessed_mime_type, part->priv->guessed_mime_type) != 0) {
		g_free (part->priv->guessed_mime_type);
		part->priv->guessed_mime_type = guessed_mime_type;
	} else if (guessed_mime_type != part->priv->guessed_mime_type) {
		g_free (guessed_mime_type);
	}
}

const gchar *
e_mail_part_attachment_get_guessed_mime_type (EMailPartAttachment *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART_ATTACHMENT (part), NULL);

	return part->priv->guessed_mime_type;
}
