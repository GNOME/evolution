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
 *		Radek Doulik <rodo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "e-util/e-mktemp.h"
#include "mail/em-format-hook.h"
#include "mail/em-format-html.h"
#include "gtkhtml/gtkhtml-embedded.h"
#include "gst/gst.h"

#define d(x)

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

void org_gnome_audio_inline_format (gpointer ep, EMFormatHookTarget *t);

static volatile gint org_gnome_audio_class_id_counter = 0;

struct _org_gnome_audio_inline_pobject {
	EMFormatHTMLPObject object;

	CamelMimePart *part;
	gchar *filename;
	GstElement *playbin;
	gulong      bus_id;
	GstState    target_state;
	GtkWidget  *play_button;
	GtkWidget  *pause_button;
	GtkWidget  *stop_button;
};

static void
org_gnome_audio_inline_pobject_free (EMFormatHTMLPObject *o)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) o;

	d(printf ("audio inline formatter: pobject free\n"));

	if (po->play_button) {
		g_object_unref (po->play_button);
		po->play_button = NULL;
	}

	if (po->pause_button) {
		g_object_unref (po->pause_button);
		po->pause_button = NULL;
	}

	if (po->stop_button) {
		g_object_unref (po->stop_button);
		po->stop_button = NULL;
	}

	if (po->part) {
		g_object_unref (po->part);
		po->part = NULL;
	}
	if (po->filename) {
		g_unlink (po->filename);
		g_free (po->filename);
		po->filename = NULL;
	}

	if (po->bus_id) {
		g_source_remove (po->bus_id);
		po->bus_id = 0;
	}

	if (po->playbin) {
		gst_element_set_state (po->playbin, GST_STATE_NULL);
		gst_object_unref (po->playbin);
		po->playbin = NULL;
	}
}

static void
org_gnome_audio_inline_pause_clicked (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;

	if (po->playbin) {
		/* pause playing */
		gst_element_set_state (po->playbin, GST_STATE_PAUSED);
	}
}

static void
org_gnome_audio_inline_stop_clicked (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;

	if (po->playbin) {
		/* ready to play */
		gst_element_set_state (po->playbin, GST_STATE_READY);
		po->target_state = GST_STATE_READY;
	}
}

static void
org_gnome_audio_inline_set_audiosink (GstElement *playbin)
{
	GstElement *audiosink;

	/* now it's time to get the audio sink */
	audiosink = gst_element_factory_make ("gconfaudiosink", "play_audio");
	if (audiosink == NULL) {
		audiosink = gst_element_factory_make ("autoaudiosink", "play_audio");
	}

	if (audiosink) {
		g_object_set (playbin, "audio-sink", audiosink, NULL);
	}
}

static gboolean
org_gnome_audio_inline_gst_callback (GstBus * bus, GstMessage * message, gpointer data)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) data;
	GstMessageType msg_type;

	g_return_val_if_fail (po != NULL, TRUE);
	g_return_val_if_fail (po->playbin != NULL, TRUE);

	msg_type = GST_MESSAGE_TYPE (message);

	switch (msg_type) {
		case GST_MESSAGE_ERROR:
			gst_element_set_state (po->playbin, GST_STATE_NULL);
			break;
		case GST_MESSAGE_EOS:
			gst_element_set_state (po->playbin, GST_STATE_READY);
			break;
		case GST_MESSAGE_STATE_CHANGED:
			{
			      GstState old_state, new_state;

			      if (GST_MESSAGE_SRC(message) != GST_OBJECT (po->playbin))
				      break;

			      gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

			      if (old_state == new_state)
				      break;

			      if (po->play_button)
					gtk_widget_set_sensitive (po->play_button, new_state <= GST_STATE_PAUSED);
			      if (po->pause_button)
					gtk_widget_set_sensitive (po->pause_button, new_state > GST_STATE_PAUSED);
			      if (po->stop_button)
					gtk_widget_set_sensitive (po->stop_button, new_state >= GST_STATE_PAUSED);
			}

			break;
		default:
			break;
	}

	return TRUE;
}

static void
org_gnome_audio_inline_play_clicked (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;
	GstState cur_state;

	d(printf ("audio inline formatter: play\n"));

	if (!po->filename) {
		CamelStream *stream;
		CamelDataWrapper *data;
		GError *error = NULL;
		gint argc = 1;
		const gchar *argv [] = { "org_gnome_audio_inline", NULL };

		/* FIXME this is ugly, we should stream this directly to gstreamer */
		po->filename = e_mktemp ("org-gnome-audio-inline-file-XXXXXX");

		d(printf ("audio inline formatter: write to temp file %s\n", po->filename));

		stream = camel_stream_fs_new_with_name (po->filename, O_RDWR | O_CREAT | O_TRUNC, 0600, NULL);
		data = camel_medium_get_content (CAMEL_MEDIUM (po->part));
		camel_data_wrapper_decode_to_stream (data, stream, NULL);
		camel_stream_flush (stream, NULL);
		g_object_unref (stream);

		d(printf ("audio inline formatter: init gst playbin\n"));

		if (gst_init_check (&argc, (gchar ***) &argv, &error)) {
			gchar *uri;
			GstBus *bus;

			/* create a disk reader */
			po->playbin = gst_element_factory_make ("playbin", "playbin");
			if (po->playbin == NULL) {
				g_printerr ("Failed to create gst_element_factory playbin; check your installation\n");
				return;

			}

			uri = g_filename_to_uri (po->filename, NULL, NULL);
			g_object_set (G_OBJECT (po->playbin), "uri", uri, NULL);
			g_free (uri);
			org_gnome_audio_inline_set_audiosink(po->playbin);

			bus = gst_element_get_bus (po->playbin);
			po->bus_id = gst_bus_add_watch (bus, org_gnome_audio_inline_gst_callback, po);
			gst_object_unref (bus);

		} else {
			g_printerr ("GStreamer failed to initialize: %s",error ? error->message : "");
			g_error_free (error);
		}
	}

        gst_element_get_state (po->playbin, &cur_state, NULL, 0);

        if (cur_state >= GST_STATE_PAUSED) {
		gst_element_set_state (po->playbin, GST_STATE_READY);
	}

	if (po->playbin) {
		/* start playing */
		gst_element_set_state (po->playbin, GST_STATE_PLAYING);
	}
}

static GtkWidget *
org_gnome_audio_inline_add_button (GtkWidget *box, const gchar *stock_icon, GCallback cb, gpointer data, gboolean sensitive)
{
	GtkWidget *button;

	button = gtk_button_new_from_stock(stock_icon);
	gtk_widget_set_sensitive (button, sensitive);
	g_signal_connect (button, "clicked", cb, data);

	gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (box), button, TRUE, TRUE, 0);

	return button;
}

static gboolean
org_gnome_audio_inline_button_panel (EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	GtkWidget *box;
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;

	/* it is OK to call UI functions here, since we are called from UI thread */

	box = gtk_hbutton_box_new ();
	po->play_button = g_object_ref (org_gnome_audio_inline_add_button (box, GTK_STOCK_MEDIA_PLAY, G_CALLBACK (org_gnome_audio_inline_play_clicked), po, TRUE));
	po->pause_button = g_object_ref (org_gnome_audio_inline_add_button (box, GTK_STOCK_MEDIA_PAUSE, G_CALLBACK (org_gnome_audio_inline_pause_clicked), po, FALSE));
	po->stop_button = g_object_ref (org_gnome_audio_inline_add_button (box, GTK_STOCK_MEDIA_STOP, G_CALLBACK (org_gnome_audio_inline_stop_clicked), po, FALSE));

	gtk_widget_show (box);
	gtk_container_add ((GtkContainer *) eb, box);

	return TRUE;
}

void
org_gnome_audio_inline_format (gpointer ep, EMFormatHookTarget *t)
{
	struct _org_gnome_audio_inline_pobject *pobj;
	gchar *classid = g_strdup_printf ("org-gnome-audio-inline-button-panel-%d", org_gnome_audio_class_id_counter);

	org_gnome_audio_class_id_counter++;

	d(printf ("audio inline formatter: format classid %s\n", classid));

	pobj = (struct _org_gnome_audio_inline_pobject *) em_format_html_add_pobject ((EMFormatHTML *) t->format, sizeof(*pobj), classid,
										      t->part, org_gnome_audio_inline_button_panel);
	g_object_ref (t->part);
	pobj->part = t->part;
	pobj->filename = NULL;
	pobj->playbin = NULL;
	pobj->play_button = NULL;
	pobj->stop_button = NULL;
	pobj->pause_button = NULL;
	pobj->bus_id = 0;
	pobj->object.free = org_gnome_audio_inline_pobject_free;
	pobj->target_state = GST_STATE_NULL;

	camel_stream_printf (t->stream, "<object classid=%s></object>\n", classid);
}
