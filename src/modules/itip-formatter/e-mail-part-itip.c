/*
 * e-mail-part-itip.c
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

#include <string.h>
#include <e-util/e-util.h>

#include "e-mail-part-itip.h"

#define E_MAIL_PART_ITIP_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PART_ITIP, EMailPartItipPrivate))

struct _EMailPartItipPrivate {
	GSList *views; /* ItipView * */
};

G_DEFINE_DYNAMIC_TYPE (
	EMailPartItip,
	e_mail_part_itip,
	E_TYPE_MAIL_PART)

static void
mail_part_itip_dispose (GObject *object)
{
	EMailPartItip *part = E_MAIL_PART_ITIP (object);

	g_cancellable_cancel (part->cancellable);

	g_free (part->message_uid);
	part->message_uid = NULL;

	g_free (part->vcalendar);
	part->vcalendar = NULL;

	g_clear_object (&part->folder);
	g_clear_object (&part->message);
	g_clear_object (&part->itip_mime_part);
	g_clear_object (&part->cancellable);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_part_itip_parent_class)->dispose (object);
}

static void
mail_part_itip_finalize (GObject *object)
{
	EMailPartItip *part = E_MAIL_PART_ITIP (object);

	g_slist_free_full (part->priv->views, g_object_unref);
	part->priv->views = NULL;

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_itip_parent_class)->finalize (object);
}

static void
mail_part_itip_content_loaded (EMailPart *part,
			       EWebView *web_view)
{
	EMailPartItip *pitip;

	g_return_if_fail (E_IS_MAIL_PART_ITIP (part));
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	pitip = E_MAIL_PART_ITIP (part);

	if (pitip->message) {
		ItipView *itip_view;
		GSList *link;

		for (link = pitip->priv->views; link; link = g_slist_next (link)) {
			EWebView *used_web_view;

			itip_view = link->data;
			used_web_view = itip_view_ref_web_view (itip_view);

			if (used_web_view == web_view) {
				g_clear_object (&used_web_view);
				return;
			}

			g_clear_object (&used_web_view);
		}

		itip_view = itip_view_new (
			e_mail_part_get_id (part),
			pitip,
			pitip->folder,
			pitip->message_uid,
			pitip->message,
			pitip->itip_mime_part,
			pitip->vcalendar,
			pitip->cancellable);

		itip_view_set_web_view (itip_view, web_view);

		pitip->priv->views = g_slist_prepend (pitip->priv->views, itip_view);
	}
}

static void
e_mail_part_itip_class_init (EMailPartItipClass *class)
{
	GObjectClass *object_class;
	EMailPartClass *mail_part_class;

	g_type_class_add_private (class, sizeof (EMailPartItipPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_part_itip_dispose;
	object_class->finalize = mail_part_itip_finalize;

	mail_part_class = E_MAIL_PART_CLASS (class);
	mail_part_class->content_loaded = mail_part_itip_content_loaded;
}

static void
e_mail_part_itip_class_finalize (EMailPartItipClass *class)
{
}

static void
e_mail_part_itip_init (EMailPartItip *part)
{
	part->priv = E_MAIL_PART_ITIP_GET_PRIVATE (part);
	part->cancellable = g_cancellable_new ();

	e_mail_part_set_mime_type (E_MAIL_PART (part), "text/calendar");

	E_MAIL_PART (part)->force_collapse = TRUE;
}

void
e_mail_part_itip_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_part_itip_register_type (type_module);
}

EMailPartItip *
e_mail_part_itip_new (CamelMimePart *mime_part,
                      const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_ITIP,
		"id", id, "mime-part", mime_part, NULL);
}
