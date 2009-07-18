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
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-categories.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <libedataserver/e-time-utils.h>
#include <e-util/e-util.h>
#include <e-util/e-categories-config.h>
#include "calendar-config.h"
#include "e-cal-component-memo-preview.h"
#include "e-cal-component-preview.h"
#include <camel/camel-mime-filter-tohtml.h>

struct _ECalComponentMemoPreviewPrivate {
	GtkWidget *html;

	icaltimezone *zone;
};

G_DEFINE_TYPE (ECalComponentMemoPreview, e_cal_component_memo_preview, GTK_TYPE_TABLE)

static void
on_link_clicked (GtkHTML *html, const gchar *url, gpointer data)
{
	/* FIXME Pass a parent window. */
	e_show_uri (NULL, url);
}

static void
on_url_cb (GtkHTML *html, const gchar *url, gpointer data)
{
#if 0
	gchar *msg;
	ECalComponentMemoPreview *preview = data;

	if (url && *url) {
		msg = g_strdup_printf (_("Click to open %s"), url);
		e_calendar_table_set_status_message (e_tasks_get_calendar_table (tasks), msg);
		g_free (msg);
	} else
		e_calendar_table_set_status_message (e_tasks_get_calendar_table (tasks), NULL);
#endif
}

/* Converts a time_t to a string, relative to the specified timezone */
static gchar *
timet_to_str_with_zone (ECalComponentDateTime *dt, ECal *ecal, icaltimezone *default_zone)
{
	struct icaltimetype itt;
	icaltimezone *zone;
        struct tm tm;
        gchar buf[256];

	if (dt->tzid) {
		/* If we can't find the zone, we'll guess its "local" */
		if (!e_cal_get_timezone (ecal, dt->tzid, &zone, NULL))
			zone = NULL;
	} else if (dt->value->is_utc) {
		zone = icaltimezone_get_utc_timezone ();
	} else {
		zone = NULL;
	}

	itt = *dt->value;
	if (zone)
		icaltimezone_convert_time (&itt, zone, default_zone);
        tm = icaltimetype_to_tm (&itt);

        e_time_format_date_and_time (&tm, calendar_config_get_24_hour_format (),
                                     FALSE, FALSE, buf, sizeof (buf));

	return g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
}

static void
write_html (GtkHTMLStream *stream, ECal *ecal, ECalComponent *comp, icaltimezone *default_zone)
{
	ECalComponentText text;
	ECalComponentDateTime dt;
	gchar *str;
	GSList *l;
	gboolean one_added = FALSE;

	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* write document header */
	e_cal_component_get_summary (comp, &text);

	if (text.value)
		gtk_html_stream_printf (stream,
					"<HTML><BODY><H1>%s</H1>",
					text.value);
	else
		gtk_html_stream_printf (stream,
					"<HTML><BODY><H1><I>%s</I></H1>",
					_("Untitled"));

	/* write icons for the categories */
	e_cal_component_get_categories_list (comp, &l);
	if (l) {
		GSList *node;
		GString *string = g_string_new (NULL);

		gtk_html_stream_printf(stream, "<H3>%s: ", _("Categories"));

		for (node = l; node != NULL; node = node->next) {
			const gchar *icon_file;

			icon_file = e_categories_get_icon_file_for ((const gchar *) node->data);
			if (icon_file && g_file_test(icon_file, G_FILE_TEST_EXISTS)) {
				gchar *icon_file_uri = g_filename_to_uri (icon_file, NULL, NULL);
				gtk_html_stream_printf (stream, "<IMG ALT=\"%s\" SRC=\"%s\">",
							(const gchar *) node->data, icon_file_uri);
				g_free (icon_file_uri);
				one_added = TRUE;
			}
			else{
				if (one_added == FALSE) {
					g_string_append_printf (string, "%s", (const gchar *) node->data);
					one_added = TRUE;
				}
				else{
					g_string_append_printf (string, ", %s", (const gchar *) node->data);
				}
			}
		}

		if (string->len > 0)
			gtk_html_stream_printf(stream, "%s", string->str);

		g_string_free (string, TRUE);

		gtk_html_stream_printf(stream, "</H3>");

		e_cal_component_free_categories_list (l);
	}

	/* Start table */
	gtk_html_stream_printf (stream, "<TABLE BORDER=\"0\" WIDTH=\"80%%\">"
				"<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\" WIDTH=\"15%%\"></TD></TR>");

	/* write start date */
	e_cal_component_get_dtstart (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, ecal, default_zone);
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD><TD>%s</TD></TR>",
					_("Start Date:"), str);

		g_free (str);
	}
	e_cal_component_free_datetime (&dt);

	/* write description and URL */
	gtk_html_stream_printf (stream, "<TR><TD COLSPAN=\"2\"><HR></TD></TR>");

	e_cal_component_get_description_list (comp, &l);
	if (l) {
		GSList *node;

		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD>", _("Description:"));

		gtk_html_stream_printf (stream, "<TD><TT>");

		for (node = l; node != NULL; node = node->next) {
			gchar *html;

			text = * (ECalComponentText *) node->data;
			html = camel_text_to_html (text.value ? text.value : "", CAMEL_MIME_FILTER_TOHTML_CONVERT_NL | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES | CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);

			if (html)
				gtk_html_stream_printf (stream, "%s", html);

			g_free (html);
		}

		gtk_html_stream_printf (stream, "</TT></TD></TR>");

		e_cal_component_free_text_list (l);
	}

	/* URL */
	e_cal_component_get_url (comp, (const gchar **) &str);
	if (str) {
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD>", _("Web Page:"));
		gtk_html_stream_printf (stream, "<TD><A HREF=\"%s\">%s</A></TD></TR>", str, str);
	}

	gtk_html_stream_printf (stream, "</TABLE>");

	/* close document */
	gtk_html_stream_printf (stream, "</BODY></HTML>");
}

static void
e_cal_component_memo_preview_init (ECalComponentMemoPreview *preview)
{
	ECalComponentMemoPreviewPrivate *priv;
	GtkWidget *scroll;

	priv = g_new0 (ECalComponentMemoPreviewPrivate, 1);
	preview->priv = priv;

	priv->html = gtk_html_new ();
	gtk_html_set_default_content_type (GTK_HTML (priv->html), "charset=utf-8");
	gtk_html_load_empty (GTK_HTML (priv->html));

	g_signal_connect (G_OBJECT (priv->html), "url_requested",
			  G_CALLBACK (e_cal_comp_preview_url_requested_cb), NULL);
	g_signal_connect (G_OBJECT (priv->html), "link_clicked",
			  G_CALLBACK (on_link_clicked), preview);
	g_signal_connect (G_OBJECT (priv->html), "on_url",
			  G_CALLBACK (on_url_cb), preview);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);

	gtk_container_add (GTK_CONTAINER (scroll), priv->html);
	gtk_container_add (GTK_CONTAINER (preview), scroll);
	gtk_widget_show_all (scroll);

	priv->zone = icaltimezone_get_utc_timezone ();
}

static void
e_cal_component_memo_preview_destroy (GtkObject *object)
{
	ECalComponentMemoPreview *preview;
	ECalComponentMemoPreviewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_MEMO_PREVIEW (object));

	preview = E_CAL_COMPONENT_MEMO_PREVIEW (object);
	priv = preview->priv;

	if (priv) {

		g_free (priv);
		preview->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (e_cal_component_memo_preview_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (e_cal_component_memo_preview_parent_class)->destroy) (object);
}

static void
e_cal_component_memo_preview_class_init (ECalComponentMemoPreviewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	object_class->destroy = e_cal_component_memo_preview_destroy;
}

GtkWidget *
e_cal_component_memo_preview_new (void)
{
	ECalComponentMemoPreview *preview;

	preview = g_object_new (e_cal_component_memo_preview_get_type (), NULL);

	return GTK_WIDGET (preview);
}

icaltimezone *
e_cal_component_memo_preview_get_default_timezone (ECalComponentMemoPreview *preview)
{
	ECalComponentMemoPreviewPrivate *priv;

	g_return_val_if_fail (preview != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT_MEMO_PREVIEW (preview), NULL);

	priv = preview->priv;

	return priv->zone;
}

void
e_cal_component_memo_preview_set_default_timezone (ECalComponentMemoPreview *preview, icaltimezone *zone)
{
	ECalComponentMemoPreviewPrivate *priv;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_MEMO_PREVIEW (preview));
	g_return_if_fail (zone != NULL);

	priv = preview->priv;

	priv->zone = zone;
}

void
e_cal_component_memo_preview_display (ECalComponentMemoPreview *preview, ECal *ecal, ECalComponent *comp)
{
	ECalComponentMemoPreviewPrivate *priv;
	GtkHTMLStream *stream;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_MEMO_PREVIEW (preview));
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	priv = preview->priv;

	stream = gtk_html_begin (GTK_HTML (priv->html));
	write_html (stream, ecal, comp, priv->zone);
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
}

void
e_cal_component_memo_preview_clear (ECalComponentMemoPreview *preview)
{
	ECalComponentMemoPreviewPrivate *priv;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_MEMO_PREVIEW (preview));

	priv = preview->priv;

	gtk_html_load_empty (GTK_HTML (priv->html));
}

GtkWidget *
e_cal_component_memo_preview_get_html (ECalComponentMemoPreview *preview)
{
	g_return_val_if_fail (preview != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT_MEMO_PREVIEW (preview), NULL);

	return preview->priv->html;
}
