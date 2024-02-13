/*
 * e-mail-part-audio.c
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

#include "e-mail-part-audio.h"

struct _EMailPartAudioPrivate {
	gboolean dummy;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailPartAudio, e_mail_part_audio, E_TYPE_MAIL_PART)

static void
mail_part_audio_constructed (GObject *object)
{
	EMailPart *part;
	CamelMimePart *mime_part;
	CamelContentType *content_type;

	part = E_MAIL_PART (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_part_audio_parent_class)->constructed (object);

	e_mail_part_set_is_attachment (part, TRUE);

	mime_part = e_mail_part_ref_mime_part (part);

	content_type = camel_mime_part_get_content_type (mime_part);

	if (content_type != NULL) {
		gchar *mime_type;

		mime_type = camel_content_type_simple (content_type);
		e_mail_part_set_mime_type (part, mime_type);
		g_free (mime_type);
	} else {
		e_mail_part_set_mime_type (part, "audio/*");
	}

	g_object_unref (mime_part);
}

static void
e_mail_part_audio_class_init (EMailPartAudioClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_part_audio_constructed;
}

static void
e_mail_part_audio_init (EMailPartAudio *part)
{
	part->priv = e_mail_part_audio_get_instance_private (part);
}

EMailPart *
e_mail_part_audio_new (CamelMimePart *mime_part,
                       const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_AUDIO,
		"id", id, "mime-part", mime_part, NULL);
}

