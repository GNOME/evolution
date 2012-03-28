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
 *
 * Authors:
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include <em-format/em-format.h>
#include <mail/em-config.h>
#include <mail/em-format-hook.h>

void org_gnome_prefer_plain_multipart_alternative (gpointer ep, EMFormatHookTarget *t);
void org_gnome_prefer_plain_text_html (gpointer ep, EMFormatHookTarget *t);
GtkWidget *org_gnome_prefer_plain_config_mode (EPlugin *epl, struct _EConfigHookItemFactoryData *data);

enum {
	EPP_NORMAL,
	EPP_PREFER,
	EPP_TEXT
};

static GSettings *epp_settings = NULL;
static gint epp_mode = -1;
static gboolean epp_show_suppressed = TRUE;

static void
make_part_attachment (EMFormat *format,
                                      CamelMimePart *part,
                                      GString *part_id,
                                      gboolean force_html,
                                      GCancellable *cancellable)
{
	EMFormatParserInfo info = {0};

	if (camel_content_type_is (camel_mime_part_get_content_type (part), "text", "html")) {
		/* always show HTML as attachments and not inline */
		camel_mime_part_set_disposition (part, "attachment");

		if (!camel_mime_part_get_filename (part)) {
			gchar *str = g_strdup_printf ("%s.html", _("attachment"));
			camel_mime_part_set_filename (part, str);
			g_free (str);
		}

		em_format_parse_part_as (
			format, part, part_id, &info, "application/octet-stream", cancellable);
	} else if (force_html && CAMEL_IS_MIME_MESSAGE (part)) {
		/* message was asked to be formatted as text/html;
		 * might be for cases where message itself is a text/html part */
		CamelMimePart *new_part;
		CamelDataWrapper *content;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));
		g_return_if_fail (content != NULL);

		new_part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (new_part), content);

		em_format_parse_part (format, new_part, part_id, &info, cancellable);

		g_object_unref (new_part);
	} else {
		em_format_parse_part (format, part, part_id, &info, cancellable);
	}
}

void
org_gnome_prefer_plain_text_html (gpointer ep,
                                  EMFormatHookTarget *t)
{
	/* In text-only mode, all html output is suppressed for the first processing */
	if (epp_mode != EPP_TEXT
	    || strstr (t->part_id->str, ".alternative-prefer-plain.") != NULL
	    || em_format_is_inline (t->format, t->part_id->str, t->part, t->info->handler)) {
		/* FIXME Not passing a GCancellable here. */
		t->info->handler->old->parse_func (
			t->format, t->part, t->part_id,
			t->info, NULL);
	} else if (epp_show_suppressed)
		make_part_attachment (t->format, t->part, t->part_id, TRUE, NULL);
}

static void
export_as_attachments (CamelMultipart *mp,
                                           EMFormat *format,
                                           CamelMimePart *except,
                                           GString *part_id)
{
	gint i, nparts;
	CamelMimePart *part;
	gint len;

	if (!mp || !CAMEL_IS_MULTIPART (mp))
		return;

	len = part_id->len;
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		part = camel_multipart_get_part (mp, i);

		if (part != except) {
			CamelMultipart *multipart = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

			g_string_append_printf (part_id, ".aleternative-prefer-plain.%d", i);
			if (CAMEL_IS_MULTIPART (multipart)) {
				export_as_attachments (multipart, format, except, part_id);
			} else {
				make_part_attachment (format, part, part_id, FALSE, NULL);
			}
			g_string_truncate (part_id, len);
		}
	}
}

void
org_gnome_prefer_plain_multipart_alternative (gpointer ep,
                                              EMFormatHookTarget *t)
{
	CamelMultipart *mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) t->part);
	CamelMimePart *part, *display_part = NULL, *calendar_part = NULL;
	gint i, nparts, partidlen, displayid = 0, calendarid = 0;

	partidlen = t->part_id->len;

	if (epp_mode == EPP_NORMAL) {
		gboolean have_plain = FALSE;

		/* Try to find text/html part even when not as last and force
		 * to show it.  Old handler will show the last part of
		 * multipart/alternate, but if we can offer HTML, then
		 * offer it, regardless of position in multipart.  But do
		 * this when have only text/plain and text/html parts,
		 * not more. */
		nparts = camel_multipart_get_number (mp);
		for (i = 0; i < nparts; i++) {
			CamelContentType *content_type;

			part = camel_multipart_get_part (mp, i);

			if (!part)
				continue;

			content_type = camel_mime_part_get_content_type (part);

			if (camel_content_type_is (content_type, "text", "html")) {
				displayid = i;
				display_part = part;

				if (have_plain)
					break;
			} else if (camel_content_type_is (content_type, "text", "plain")) {
				have_plain = TRUE;

				if (display_part)
					break;
			}
		}

		if (display_part && have_plain && nparts == 2) {
			g_string_append_printf (t->part_id, ".alternative-prefer-plain.%d", displayid);
			/* FIXME Not passing a GCancellable here. */
			em_format_parse_part_as (
				t->format, display_part, t->part_id, t->info, "text/html", NULL);
			g_string_truncate (t->part_id, partidlen);
		} else {
			/* FIXME Not passing a GCancellable here. */
			t->info->handler->old->parse_func (
				t->format, t->part, t->part_id, t->info, NULL);
		}
		return;
	} else if (!CAMEL_IS_MULTIPART (mp)) {
		/* FIXME Not passing GCancellable here. */
		em_format_parse_part_as (t->format, t->part, t->part_id, t->info, "x-evolution/message/source", NULL);
		return;
	}

	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelContentType *ct;

		part = camel_multipart_get_part (mp, i);

		if (!part)
			continue;

		ct = camel_mime_part_get_content_type (part);
		if (!display_part && camel_content_type_is (ct, "text", "plain")) {
			displayid = i;
			display_part = part;
		} else if (!calendar_part && (camel_content_type_is (ct, "text", "calendar") || camel_content_type_is (ct, "text", "x-calendar"))) {
			calendarid = i;
			calendar_part = part;
		}
	}

	/* if we found a text part, show it */
	if (display_part) {
		g_string_append_printf(t->part_id, ".alternative-prefer-plain.%d", displayid);
		/* FIXME Not passing a GCancellable here. */
		em_format_parse_part_as (
			t->format, display_part, t->part_id, t->info, "text/plain", NULL);
		g_string_truncate (t->part_id, partidlen);
	}

	/* all other parts are attachments */
	if (epp_show_suppressed)
		export_as_attachments (mp, t->format, display_part, t->part_id);
	else if (calendar_part) {
		g_string_append_printf(t->part_id, ".alternative-prefer-plain.%d", calendarid);
		make_part_attachment (t->format, calendar_part, t->part_id, FALSE, NULL);
	}

	g_string_truncate (t->part_id, partidlen);
}

static struct {
	const gchar *key;
	const gchar *label;
	const gchar *description;
} epp_options[] = {
	{ "normal",
	  N_("Show HTML if present"),
	  N_("Let Evolution choose the best part to show.") },

	{ "prefer_plain",
	  N_("Show plain text if present"),
	  N_("Show plain text part, if present, otherwise "
	     "let Evolution choose the best part to show.") },

	{ "only_plain",
	  N_("Only ever show plain text"),
	  N_("Always show plain text part and make attachments "
	     "from other parts, if requested.") },
};

static void
update_info_label (GtkWidget *info_label,
                   guint mode)
{
	gchar *str = g_strconcat ("<i>", _(epp_options[mode > 2 ? 0 : mode].description), "</i>", NULL);

	gtk_label_set_markup (GTK_LABEL (info_label), str);

	g_free (str);
}

static void
epp_mode_changed (GtkComboBox *dropdown,
                  GtkWidget *info_label)
{
	epp_mode = gtk_combo_box_get_active (dropdown);
	if (epp_mode > 2)
		epp_mode = 0;

	g_settings_set_string (epp_settings, "mode", epp_options[epp_mode].key);
	update_info_label (info_label, epp_mode);
}

static void
epp_show_suppressed_toggled (GtkToggleButton *check,
                             gpointer data)
{
	g_return_if_fail (check != NULL);

	epp_show_suppressed = gtk_toggle_button_get_active (check);
	g_settings_set_boolean (epp_settings, "show-suppressed", epp_show_suppressed);
}

GtkWidget *
org_gnome_prefer_plain_config_mode (EPlugin *epl,
                                    struct _EConfigHookItemFactoryData *data)
{
	GtkComboBox *dropdown;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkWidget *dropdown_label, *info, *check;
	guint i;
	GtkTreeIter iter;

	if (data->old)
		return data->old;

	check = gtk_check_button_new_with_mnemonic (_("Show s_uppressed HTML parts as attachments"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), epp_show_suppressed);
	gtk_widget_show (check);
	g_signal_connect (
		check, "toggled",
		G_CALLBACK (epp_show_suppressed_toggled), NULL);

	dropdown = (GtkComboBox *) gtk_combo_box_new ();
	cell = gtk_cell_renderer_text_new ();
	store = gtk_list_store_new (1, G_TYPE_STRING);
	for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _(epp_options[i].label), -1);
	}

	gtk_cell_layout_pack_start ((GtkCellLayout *) dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);
	gtk_combo_box_set_model (dropdown, (GtkTreeModel *) store);
	/*gtk_combo_box_set_active(dropdown, -1);*/
	gtk_combo_box_set_active (dropdown, epp_mode);
	gtk_widget_show ((GtkWidget *) dropdown);

	dropdown_label = gtk_label_new_with_mnemonic (_("HTML _Mode"));
	gtk_widget_show (dropdown_label);
	gtk_label_set_mnemonic_widget (GTK_LABEL (dropdown_label), (GtkWidget *) dropdown);

	info = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (info), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (info), TRUE);

	gtk_widget_show (info);
	update_info_label (info, epp_mode);

	g_signal_connect (
		dropdown, "changed",
		G_CALLBACK (epp_mode_changed), info);

	g_object_get (data->parent, "n-rows", &i, NULL);
	gtk_table_attach ((GtkTable *) data->parent, check, 0, 2, i, i + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach ((GtkTable *) data->parent, dropdown_label, 0, 1, i + 1, i + 2, 0, 0, 0, 0);
	gtk_table_attach ((GtkTable *) data->parent, (GtkWidget *) dropdown, 1, 2, i + 1, i + 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach ((GtkTable *) data->parent, info, 1, 2, i + 2, i + 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	/* since this isnt dynamic, we don't need to track each item */

	return (GtkWidget *) dropdown;
}

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	gchar *key;
	gint i;

	if (epp_settings || epp_mode != -1)
		return 0;

	if (enable) {

		epp_settings = g_settings_new ("org.gnome.evolution.plugin.prefer-plain");
		key = g_settings_get_string (epp_settings, "mode");
		if (key) {
			for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
				if (!strcmp (epp_options[i].key, key)) {
					epp_mode = i;
					break;
				}
			}
			g_free (key);
		} else {
			epp_mode = 0;
		}

		epp_show_suppressed = g_settings_get_boolean (epp_settings, "show-suppressed");
	} else {
		if (epp_settings) {
			g_object_unref (epp_settings);
			epp_settings = NULL;
		}
	}

	return 0;
}
