/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-stream-mem.h>
#include <gtkhtml/gtkhtml-embedded.h>

#include "addressbook/gui/component/addressbook.h"
#include "addressbook/gui/merging/eab-contact-merging.h"
#include "addressbook/gui/widgets/eab-contact-display.h"
#include "addressbook/util/eab-book-util.h"
#include "mail/em-format-hook.h"
#include "mail/em-format-html.h"

#define d(x)

typedef struct _VCardInlinePObject VCardInlinePObject;

struct _VCardInlinePObject {
	EMFormatHTMLPObject object;

	GList *contact_list;
	GtkWidget *contact_display;
	GtkWidget *message_label;
	EABContactDisplayRenderMode mode;
};

static gint org_gnome_vcard_inline_classid;

/* Forward Declarations */
void org_gnome_vcard_inline_format (gpointer ep, EMFormatHookTarget *target);

static void
org_gnome_vcard_inline_pobject_free (EMFormatHTMLPObject *object)
{
	VCardInlinePObject *vcard_object;

	vcard_object = (VCardInlinePObject *) object;

	g_list_foreach (
		vcard_object->contact_list,
		(GFunc) g_object_unref, NULL);
	g_list_free (vcard_object->contact_list);
	vcard_object->contact_list = NULL;

	if (vcard_object->contact_display != NULL) {
		g_object_unref (vcard_object->contact_display);
		vcard_object->contact_display = NULL;
	}

	if (vcard_object->message_label != NULL) {
		g_object_unref (vcard_object->message_label);
		vcard_object->message_label = NULL;
	}
}

static void
org_gnome_vcard_inline_decode (VCardInlinePObject *vcard_object,
                               CamelMimePart *mime_part)
{
	CamelDataWrapper *data_wrapper;
	CamelMedium *medium;
	CamelStream *stream;
	GList *contact_list;
	GByteArray *array;
	const gchar *string;
	const guint8 padding[2] = {0};

	array = g_byte_array_new ();
	medium = CAMEL_MEDIUM (mime_part);

	/* Stream takes ownership of the byte array. */
	stream = camel_stream_mem_new_with_byte_array (array);
	data_wrapper = camel_medium_get_content_object (medium);
	camel_data_wrapper_decode_to_stream (data_wrapper, stream);

	/* because the result is not NULL-terminated */
	g_byte_array_append (array, padding, 2);

	string = (gchar *) array->data;
	contact_list = eab_contact_list_from_string (string);
	vcard_object->contact_list = contact_list;

	camel_object_unref (mime_part);
	camel_object_unref (stream);
}

static void
org_gnome_vcard_inline_book_open_cb (EBook *book,
                                     EBookStatus status,
                                     gpointer user_data)
{
	GList *contact_list = user_data;
	GList *iter;

	if (status != E_BOOK_ERROR_OK)
		goto exit;

	for (iter = contact_list; iter != NULL; iter = iter->next)
		eab_merging_book_add_contact (
			book, E_CONTACT (iter->data), NULL, NULL);

exit:
	if (book != NULL)
		g_object_unref (book);

	g_list_foreach (contact_list, (GFunc) g_object_unref, NULL);
	g_list_free (contact_list);
}

static void
org_gnome_vcard_inline_save_cb (VCardInlinePObject *vcard_object)
{
	GList *contact_list;

	contact_list = g_list_copy (vcard_object->contact_list);
	g_list_foreach (contact_list, (GFunc) g_object_ref, NULL);

	addressbook_load_default_book (
		org_gnome_vcard_inline_book_open_cb, contact_list);
}

static void
org_gnome_vcard_inline_toggle_cb (VCardInlinePObject *vcard_object,
                                  GtkButton *button)
{
	EABContactDisplay *contact_display;
	const gchar *label;

	contact_display = EAB_CONTACT_DISPLAY (vcard_object->contact_display);

	/* Toggle between "full" and "compact" modes. */
	if (vcard_object->mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL) {
		vcard_object->mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
		label = _("Show Full vCard");
	} else {
		vcard_object->mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
		label = _("Show Compact vCard");
	}

	gtk_button_set_label (button, label);

	eab_contact_display_render (
		EAB_CONTACT_DISPLAY (vcard_object->contact_display),
		E_CONTACT (vcard_object->contact_list->data),
		vcard_object->mode);
}

static gboolean
org_gnome_vcard_inline_embed (EMFormatHTML *format,
                              GtkHTMLEmbedded *embedded,
                              EMFormatHTMLPObject *object)
{
	VCardInlinePObject *vcard_object;
	GtkWidget *button_box;
	GtkWidget *container;
	GtkWidget *widget;
	guint length;

	vcard_object = (VCardInlinePObject *) object;
	length = g_list_length (vcard_object->contact_list);
	g_return_val_if_fail (length > 0, FALSE);

	container = GTK_WIDGET (embedded);

	widget = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	button_box = widget;

	widget = eab_contact_display_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	vcard_object->contact_display = g_object_ref (widget);
	gtk_widget_show (widget);

	eab_contact_display_render (
		EAB_CONTACT_DISPLAY (vcard_object->contact_display),
		E_CONTACT (vcard_object->contact_list->data),
		vcard_object->mode);

	widget = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	vcard_object->message_label = g_object_ref (widget);

	if (length == 2) {
		const gchar *text;

		text = _("There is one other contact.");
		gtk_label_set_text (GTK_LABEL (widget), text);
		gtk_widget_show (widget);

	} else if (length > 2) {
		gchar *text;

		/* Translators: This will always be two or more. */
		text = g_strdup_printf (ngettext (
			"There is %d other contact.",
			"There are %d other contacts.",
			length - 1), length - 1);
		gtk_label_set_text (GTK_LABEL (widget), text);
		gtk_widget_show (widget);
		g_free (text);

	} else
		gtk_widget_hide (widget);

	container = button_box;

	widget = gtk_button_new_with_label (_("Show Full vCard"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (org_gnome_vcard_inline_toggle_cb),
		vcard_object);

	widget = gtk_button_new_with_label (_("Save in Address Book"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (org_gnome_vcard_inline_save_cb),
		vcard_object);

	return TRUE;
}

void
org_gnome_vcard_inline_format (gpointer ep, EMFormatHookTarget *target)
{
	VCardInlinePObject *vcard_object;
	gchar *classid;

	classid = g_strdup_printf (
		"org-gnome-vcard-inline-display-%d",
		org_gnome_vcard_inline_classid++);

	vcard_object = (VCardInlinePObject *)
		em_format_html_add_pobject (
			EM_FORMAT_HTML (target->format),
			sizeof (VCardInlinePObject),
			classid, target->part,
			org_gnome_vcard_inline_embed);

	camel_object_ref (target->part);

	vcard_object->mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
	vcard_object->object.free = org_gnome_vcard_inline_pobject_free;
	org_gnome_vcard_inline_decode (vcard_object, target->part);

	camel_stream_printf (
		target->stream, "<object classid=%s></object>", classid);

	g_free (classid);
}
