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

struct _EMailPartItipPrivate {
	GSList *views; /* ItipView * */
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailPartItip, e_mail_part_itip, E_TYPE_MAIL_PART, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailPartItip))

static void
mail_part_itip_dispose (GObject *object)
{
	EMailPartItip *part = E_MAIL_PART_ITIP (object);

	g_cancellable_cancel (part->cancellable);

	g_clear_pointer (&part->message_uid, g_free);
	g_clear_pointer (&part->vcalendar, g_free);
	g_clear_pointer (&part->alternative_html, g_free);

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
itip_view_alternative_html_clicked_cb (EWebView *web_view,
				       const gchar *iframe_id,
				       const gchar *element_id,
				       const gchar *element_class,
				       const gchar *element_value,
				       const GtkAllocation *element_position,
				       gpointer user_data)
{
	EMailPart *mail_part = user_data;
	gchar tmp[128];

	g_return_if_fail (E_IS_MAIL_PART_ITIP (mail_part));

	if (!element_id || !element_value)
		return;

	g_return_if_fail (g_snprintf (tmp, sizeof (tmp), "%p:", mail_part) < sizeof (tmp));

	if (g_str_has_prefix (element_id, tmp)) {
		gchar spn[128];

		g_return_if_fail (g_snprintf (spn, sizeof (spn), "%s-spn", element_value) < sizeof (spn));
		g_return_if_fail (g_snprintf (tmp, sizeof (tmp), "%s-img", element_value) < sizeof (tmp));

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"EvoItip.FlipAlternativeHTMLPart(%s,%s,%s,%s);", iframe_id, element_value, tmp, spn);
	}
}

static void
e_mail_part_itip_web_view_load_changed_cb (WebKitWebView *webkit_web_view,
					   WebKitLoadEvent load_event,
					   gpointer user_data)
{
	EMailPartItip *pitip = user_data;

	g_return_if_fail (E_IS_MAIL_PART_ITIP (pitip));

	if (load_event == WEBKIT_LOAD_STARTED) {
		EWebView *web_view = E_WEB_VIEW (webkit_web_view);
		ItipView *itip_view;
		GSList *link;

		for (link = pitip->priv->views; link; link = g_slist_next (link)) {
			EWebView *used_web_view;

			itip_view = link->data;
			used_web_view = itip_view_ref_web_view (itip_view);

			if (used_web_view == web_view) {
				pitip->priv->views = g_slist_remove (pitip->priv->views, itip_view);
				g_clear_object (&used_web_view);
				g_clear_object (&itip_view);
				return;
			}

			g_clear_object (&used_web_view);
		}
	}
}

static void
mail_part_itip_content_loaded (EMailPart *part,
			       EWebView *web_view,
			       const gchar *iframe_id)
{
	EMailPartItip *pitip;

	g_return_if_fail (E_IS_MAIL_PART_ITIP (part));
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	if (g_strcmp0 ((iframe_id && *iframe_id) ? iframe_id : NULL, e_mail_part_get_id (part)) != 0)
		return;

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

	e_web_view_register_element_clicked (web_view, "itip-view-alternative-html", itip_view_alternative_html_clicked_cb, pitip);

	g_signal_connect_object (web_view, "load-changed",
		G_CALLBACK (e_mail_part_itip_web_view_load_changed_cb), pitip, 0);
}

static void
e_mail_part_itip_class_init (EMailPartItipClass *class)
{
	GObjectClass *object_class;
	EMailPartClass *mail_part_class;

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
	part->priv = e_mail_part_itip_get_instance_private (part);
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
