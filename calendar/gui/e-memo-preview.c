/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-tasks.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#include "e-memo-preview.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-categories.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <libedataserver/e-time-utils.h>
#include <e-util/e-categories-config.h>
#include "calendar-config.h"
#include "e-cal-component-preview.h"
#include <camel/camel-mime-filter-tohtml.h>

#define E_MEMO_PREVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_PREVIEW, EMemoPreviewPrivate))

struct _EMemoPreviewPrivate {
	icaltimezone *zone;
};

static gpointer parent_class;

static void
memo_preview_link_clicked (GtkHTML *html,
                           const gchar *url)
{
	GdkScreen *screen;
        GError *error = NULL;

	screen = gtk_widget_get_screen (GTK_WIDGET (html));
	gtk_show_uri (screen, url, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
                g_error_free (error);
        }
}

static void
memo_preview_on_url (GtkHTML *html,
                     const gchar *url)
{
#if 0
	char *msg;
	EMemoPreview *preview = data;

	if (url && *url) {
		msg = g_strdup_printf (_("Click to open %s"), url);
		e_calendar_table_set_status_message (e_tasks_get_calendar_table (tasks), msg);
		g_free (msg);
	} else
		e_calendar_table_set_status_message (e_tasks_get_calendar_table (tasks), NULL);
#endif
}

/* Converts a time_t to a string, relative to the specified timezone */
static char *
timet_to_str_with_zone (ECalComponentDateTime *dt,
                        ECal *ecal,
                        icaltimezone *default_zone)
{
	struct icaltimetype itt;
	icaltimezone *zone;
        struct tm tm;
        char buf[256];

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
memo_preview_write_html (GtkHTMLStream *stream,
                         ECal *ecal,
                         ECalComponent *comp,
                         icaltimezone *default_zone)
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
			const char *icon_file;

			icon_file = e_categories_get_icon_file_for ((const char *) node->data);
			if (icon_file && g_file_test(icon_file, G_FILE_TEST_EXISTS)) {
				gchar *icon_file_uri = g_filename_to_uri (icon_file, NULL, NULL);
				gtk_html_stream_printf (stream, "<IMG ALT=\"%s\" SRC=\"%s\">",
							(const char *) node->data, icon_file_uri);
				g_free (icon_file_uri);
				one_added = TRUE;
			}
			else{
				if(one_added == FALSE){
					g_string_append_printf (string, "%s", (const char *) node->data);
					one_added = TRUE;
				}
				else{
					g_string_append_printf (string, ", %s", (const char *) node->data);
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
			char *html;

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
	e_cal_component_get_url (comp, (const char **) &str);
	if (str) {
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD>", _("Web Page:"));
		gtk_html_stream_printf (stream, "<TD><A HREF=\"%s\">%s</A></TD></TR>", str, str);
	}

	gtk_html_stream_printf (stream, "</TABLE>");

	/* close document */
	gtk_html_stream_printf (stream, "</BODY></HTML>");
}

static void
memo_preview_finalize (GObject *object)
{
	EMemoPreviewPrivate *priv;

	priv = E_MEMO_PREVIEW_GET_PRIVATE (object);

	/* XXX Nothing to do? */

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
memo_preview_class_init (EMemoPreviewClass *class)
{
	GObjectClass *object_class;
	GtkHTMLClass *gtkhtml_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoPreviewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = memo_preview_finalize;

	gtkhtml_class = GTK_HTML_CLASS (class);
	gtkhtml_class->link_clicked = memo_preview_link_clicked;
	gtkhtml_class->on_url = memo_preview_on_url;
}

static void
memo_preview_init (EMemoPreview *preview)
{
	EMemoPreviewPrivate *priv;
	GtkHTML *html;

	preview->priv = E_MEMO_PREVIEW_GET_PRIVATE (preview);

	html = GTK_HTML (preview);
	gtk_html_set_default_content_type (html, "charset=utf-8");
	gtk_html_load_empty (html);

	priv->zone = icaltimezone_get_utc_timezone ();
}

GType
e_memo_preview_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMemoPreviewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) memo_preview_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMemoPreview),
			0,     /* n_preallocs */
			(GInstanceInitFunc) memo_preview_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_HTML, "EMemoPreview", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_memo_preview_new (void)
{
	return g_object_new (E_TYPE_MEMO_PREVIEW, NULL);
}

icaltimezone *
e_memo_preview_get_default_timezone (EMemoPreview *preview)
{
	g_return_val_if_fail (E_IS_MEMO_PREVIEW (preview), NULL);

	return preview->priv->zone;
}

void
e_memo_preview_set_default_timezone (EMemoPreview *preview,
                                     icaltimezone *zone)
{
	g_return_if_fail (E_IS_MEMO_PREVIEW (preview));
	g_return_if_fail (zone != NULL);

	preview->priv->zone = zone;
}

void
e_memo_preview_display (EMemoPreview *preview,
                        ECal *ecal,
                        ECalComponent *comp)
{
	GtkHTMLStream *stream;

	g_return_if_fail (E_IS_MEMO_PREVIEW (preview));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	stream = gtk_html_begin (GTK_HTML (preview));
	memo_preview_write_html (stream, ecal, comp, preview->priv->zone);
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
}

void
e_memo_preview_clear (EMemoPreview *preview)
{
	g_return_if_fail (E_IS_MEMO_PREVIEW (preview));

	gtk_html_load_empty (GTK_HTML (preview));
}

