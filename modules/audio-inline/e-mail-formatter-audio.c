/*
 * e-mail-formatter-audio.c
 *
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
 */

#include "e-mail-formatter-audio.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <gst/gst.h>

#include <libebackend/libebackend.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>

#include "e-mail-part-audio.h"

#define d(x)

typedef EMailFormatterExtension EMailFormatterAudio;
typedef EMailFormatterExtensionClass EMailFormatterAudioClass;

typedef EExtension EMailFormatterAudioLoader;
typedef EExtensionClass EMailFormatterAudioLoaderClass;

GType e_mail_formatter_audio_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterAudio,
	e_mail_formatter_audio,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.widget.audio",
	"audio/ac3",
	"audio/x-ac3",
	"audio/basic",
	"audio/mpeg",
	"audio/x-mpeg",
	"audio/mpeg3",
	"audio/x-mpeg3",
	"audio/mp3",
	"audio/x-mp3",
	"audio/mp4",
	"audio/flac",
	"audio/x-flac",
	"audio/mod",
	"audio/x-mod",
	"audio/x-wav",
	"audio/microsoft-wav",
	"audio/x-wma",
	"audio/x-ms-wma",
	"audio/ogg",
	"audio/x-vorbis+ogg",
	"application/ogg",
	"application/x-ogg",
	NULL
};

static void
pause_clicked (GtkWidget *button,
               EMailPartAudio *part)
{
	if (part->playbin) {
		/* pause playing */
		gst_element_set_state (part->playbin, GST_STATE_PAUSED);
	}
}

static void
stop_clicked (GtkWidget *button,
              EMailPartAudio *part)
{
	if (part->playbin) {
		/* ready to play */
		gst_element_set_state (part->playbin, GST_STATE_READY);
		part->target_state = GST_STATE_READY;
	}
}

static void
set_audiosink (GstElement *playbin)
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
gst_callback (GstBus *bus,
              GstMessage *message,
              gpointer data)
{
	EMailPartAudio *part = data;
	GstMessageType msg_type;

	g_return_val_if_fail (part != NULL, TRUE);
	g_return_val_if_fail (part->playbin != NULL, TRUE);

	msg_type = GST_MESSAGE_TYPE (message);

	switch (msg_type) {
		case GST_MESSAGE_ERROR:
			gst_element_set_state (part->playbin, GST_STATE_NULL);
			break;
		case GST_MESSAGE_EOS:
			gst_element_set_state (part->playbin, GST_STATE_READY);
			break;
		case GST_MESSAGE_STATE_CHANGED:
			{
			      GstState old_state, new_state;

			      if (GST_MESSAGE_SRC (message) != GST_OBJECT (part->playbin))
				      break;

			      gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

			      if (old_state == new_state)
				      break;

			      if (part->play_button)
					gtk_widget_set_sensitive (
						part->play_button,
						new_state <= GST_STATE_PAUSED);
			      if (part->pause_button)
					gtk_widget_set_sensitive (
						part->pause_button,
						new_state > GST_STATE_PAUSED);
			      if (part->stop_button)
					gtk_widget_set_sensitive (
						part->stop_button,
						new_state >= GST_STATE_PAUSED);
			}

			break;
		default:
			break;
	}

	return TRUE;
}

static void
play_clicked (GtkWidget *button,
              EMailPartAudio *part)
{
	GstState cur_state;

	d (printf ("audio formatter: play\n"));

	if (!part->filename) {
		CamelStream *stream;
		CamelDataWrapper *data;
		CamelMimePart *mime_part;
		GError *error = NULL;
		gint argc = 1;
		const gchar *argv[] = { "org_gnome_audio", NULL };

		/* FIXME this is ugly, we should stream this directly to gstreamer */
		part->filename = e_mktemp ("org-gnome-audio-file-XXXXXX");

		d (printf ("audio formatter: write to temp file %s\n", part->filename));

		stream = camel_stream_fs_new_with_name (
			part->filename, O_RDWR | O_CREAT | O_TRUNC, 0600, NULL);
		mime_part = e_mail_part_ref_mime_part (E_MAIL_PART (part));
		data = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
		camel_data_wrapper_decode_to_stream_sync (data, stream, NULL, NULL);
		camel_stream_flush (stream, NULL, NULL);
		g_object_unref (mime_part);
		g_object_unref (stream);

		d (printf ("audio formatter: init gst playbin\n"));

		if (gst_init_check (&argc, (gchar ***) &argv, &error)) {
			gchar *uri;
			GstBus *bus;

			/* create a disk reader */
			part->playbin = gst_element_factory_make ("playbin", "playbin");
			if (part->playbin == NULL) {
				g_printerr ("Failed to create gst_element_factory playbin; check your installation\n");
				return;

			}

			uri = g_filename_to_uri (part->filename, NULL, NULL);
			g_object_set (part->playbin, "uri", uri, NULL);
			g_free (uri);
			set_audiosink (part->playbin);

			bus = gst_element_get_bus (part->playbin);
			part->bus_id = gst_bus_add_watch (bus, gst_callback, part);
			gst_object_unref (bus);

		} else {
			g_printerr ("GStreamer failed to initialize: %s",error ? error->message : "");
			g_error_free (error);
		}
	}

	gst_element_get_state (part->playbin, &cur_state, NULL, 0);

	if (cur_state >= GST_STATE_PAUSED) {
		gst_element_set_state (part->playbin, GST_STATE_READY);
	}

	if (part->playbin) {
		/* start playing */
		gst_element_set_state (part->playbin, GST_STATE_PLAYING);
	}
}

static GtkWidget *
add_button (GtkWidget *box,
            const gchar *stock_icon,
            GCallback cb,
            gpointer data,
            gboolean sensitive)
{
	GtkWidget *button;

	button = gtk_button_new_from_stock (stock_icon);
	gtk_widget_set_sensitive (button, sensitive);
	g_signal_connect (button, "clicked", cb, data);

	gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (box), button, TRUE, TRUE, 0);

	return button;
}

static gboolean
mail_formatter_audio_format (EMailFormatterExtension *extension,
                             EMailFormatter *formatter,
                             EMailFormatterContext *context,
                             EMailPart *part,
                             CamelStream *stream,
                             GCancellable *cancellable)
{
	gchar *str;

	str = g_strdup_printf (
		"<object type=\"application/vnd.evolution.widget.audio\" "
			"width=\"100%%\" height=\"auto\" data=\"%s\" id=\"%s\"></object>",
		e_mail_part_get_id (part),
		e_mail_part_get_id (part));

	camel_stream_write_string (stream, str, cancellable, NULL);

	g_free (str);

	return TRUE;
}

static GtkWidget *
mail_formatter_audio_get_widget (EMailFormatterExtension *extension,
                                 EMailPartList *context,
                                 EMailPart *part,
                                 GHashTable *params)
{
	GtkWidget *box;
	EMailPartAudio *ai_part;

	g_return_val_if_fail (E_IS_MAIL_PART_AUDIO (part), NULL);

	ai_part = (EMailPartAudio *) part;

	/* it is OK to call UI functions here, since we are called from UI thread */
	box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	ai_part->play_button = g_object_ref (
		add_button (box, GTK_STOCK_MEDIA_PLAY,
		G_CALLBACK (play_clicked), part, TRUE));
	ai_part->pause_button = g_object_ref (
		add_button (box, GTK_STOCK_MEDIA_PAUSE,
		G_CALLBACK (pause_clicked), part, FALSE));
	ai_part->stop_button = g_object_ref (
		add_button (box, GTK_STOCK_MEDIA_STOP,
		G_CALLBACK (stop_clicked), part, FALSE));

	gtk_widget_show (box);

	return box;
}

static void
e_mail_formatter_audio_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Audio Player");
	class->description = _("Play the attachment in embedded audio player");
	class->mime_types = formatter_mime_types;
	class->format = mail_formatter_audio_format;
	class->get_widget = mail_formatter_audio_get_widget;
}

static void
e_mail_formatter_audio_class_finalize (EMailFormatterExtensionClass *class)
{
}

static void
e_mail_formatter_audio_init (EMailFormatterExtension *extension)
{
}

void
e_mail_formatter_audio_type_register (GTypeModule *type_module)
{
	e_mail_formatter_audio_register_type (type_module);
}

