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

#include "e-mail-part-itip.h"

#define E_MAIL_PART_ITIP_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PART_ITIP, EMailPartItipPrivate))

struct _EMailPartItipPrivate {
	gint placeholder;
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

	g_clear_object (&part->cancellable);
	g_clear_object (&part->client_cache);
	g_clear_object (&part->comp);
	g_clear_object (&part->view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_part_itip_parent_class)->dispose (object);
}

static void
mail_part_itip_finalize (GObject *object)
{
	EMailPartItip *part = E_MAIL_PART_ITIP (object);

	g_free (part->vcalendar);
	g_free (part->calendar_uid);
	g_free (part->from_address);
	g_free (part->from_name);
	g_free (part->to_address);
	g_free (part->to_name);
	g_free (part->delegator_address);
	g_free (part->delegator_name);
	g_free (part->my_address);
	g_free (part->uid);

	if (part->top_level != NULL)
		icalcomponent_free (part->top_level);

	if (part->main_comp != NULL)
		icalcomponent_free (part->main_comp);

	g_hash_table_destroy (part->real_comps);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_itip_parent_class)->finalize (object);
}

static void
mail_part_itip_bind_dom_element (EMailPart *part,
                                 WebKitDOMElement *element)
{
	GString *buffer;
	WebKitDOMDocument *document;
	WebKitDOMElement *bind_element, *document_element;
	ItipView *view;
	EMailPartItip *pitip;

	pitip = E_MAIL_PART_ITIP (part);

	bind_element = element;
	if (!WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (bind_element))
		element = webkit_dom_element_query_selector (element, "iframe", NULL);

	g_return_if_fail (WEBKIT_DOM_IS_HTML_IFRAME_ELEMENT (element));

	buffer = g_string_new ("");
	document = webkit_dom_html_iframe_element_get_content_document (
		WEBKIT_DOM_HTML_IFRAME_ELEMENT (element));

	view = itip_view_new (pitip, pitip->client_cache);
	g_object_set_data_full (
		G_OBJECT (element), "view", view,
		(GDestroyNotify) g_object_unref);

	document_element = webkit_dom_document_get_document_element (document);
	itip_view_create_dom_bindings (view, document_element);
	g_object_unref (document_element);

	itip_view_init_view (view);
	g_string_free (buffer, TRUE);
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
	mail_part_class->bind_dom_element = mail_part_itip_bind_dom_element;
}

static void
e_mail_part_itip_class_finalize (EMailPartItipClass *class)
{
}

static void
e_mail_part_itip_init (EMailPartItip *part)
{
	part->priv = E_MAIL_PART_ITIP_GET_PRIVATE (part);

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

