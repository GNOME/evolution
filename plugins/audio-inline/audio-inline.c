/* 
   Copyright (C) 2004 Novell, Inc.
   Author: Radek Doulik

 */

/* This file is licensed under the GNU GPL v2 or later */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h> /* for unlink */
#include <string.h>
#include "e-util/e-icon-factory.h"
#include "e-util/e-mktemp.h"
#include "camel/camel-medium.h"
#include "camel/camel-mime-part.h"
#include "camel/camel-stream.h"
#include "camel/camel-stream-fs.h"
#include "mail/em-format-hook.h"
#include "mail/em-format-html.h"
#include "gtk/gtkbutton.h"
#include "gtk/gtkbox.h"
#include "gtk/gtkimage.h"
#include "gtk/gtkhbbox.h"
#include "gtkhtml/gtkhtml-embedded.h"
#include "gst/gst.h"

#define d(x) x

void org_gnome_audio_inline_format (void *ep, EMFormatHookTarget *t);

volatile static int org_gnome_audio_class_id_counter = 0;

struct _org_gnome_audio_inline_pobject {
	EMFormatHTMLPObject object;

	CamelMimePart *part;
	char *filename;
	GstElement *thread;
};

static void
org_gnome_audio_inline_pobject_free (EMFormatHTMLPObject *o)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) o;

	d(printf ("audio inline formatter: pobject free\n"));

	if (po->part) {
		camel_object_unref (po->part);
		po->part = NULL;
	}
	if (po->filename) {
		unlink (po->filename);
		g_free (po->filename);
		po->filename = NULL;
	}
	if (po->thread) {
		gst_element_set_state (po->thread, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (po->thread));
		po->thread = NULL;
	}
}

static void
org_gnome_audio_inline_pause_clicked (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;

	if (po->thread) {
		/* start playing */
		gst_element_set_state (po->thread, GST_STATE_PAUSED);
	}
}

static void
org_gnome_audio_inline_stop_clicked (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;

	if (po->thread) {
		/* start playing */
		gst_element_set_state (po->thread, GST_STATE_READY);
	}
}

static GstElement *
org_gnome_audio_inline_gst_mpeg_thread (GstElement *filesrc)
{
	GstElement *thread, *decoder, *audiosink;

	/* create a new thread to hold the elements */
	thread = gst_thread_new ("org-gnome-audio-inline-mpeg-thread");

	/* now it's time to get the decoder */
	decoder = gst_element_factory_make ("mad", "decoder");
                                
	/* and an audio sink */
	audiosink = gst_element_factory_make ("osssink", "play_audio");
                                    
	/* add objects to the main pipeline */
	gst_bin_add_many (GST_BIN (thread), filesrc, decoder, audiosink, NULL);
                                        
	/* link src to sink */
	gst_element_link_many (filesrc, decoder, audiosink, NULL);

	return thread;
}

static GstElement *
org_gnome_audio_inline_gst_ogg_thread (GstElement *filesrc)
{
	GstElement *thread, *demuxer, *decoder, *converter, *audiosink;

	/* create a new thread to hold the elements */
	thread = gst_thread_new ("org-gnome-audio-inline-mpeg-thread");

	/* create an ogg demuxer */
	demuxer = gst_element_factory_make ("oggdemux", "demuxer");
	g_assert (demuxer != NULL);
                                                                            
	/* create a vorbis decoder */
	decoder = gst_element_factory_make ("vorbisdec", "decoder");
	g_assert (decoder != NULL);
                                                                                  
	/* create an audio converter */
	converter = gst_element_factory_make ("audioconvert", "converter");
	g_assert (decoder != NULL);
                                                                                        
	/* and an audio sink */
	audiosink = gst_element_factory_make ("osssink", "play_audio");
	g_assert (audiosink != NULL);
                                                                                              
	/* add objects to the thread */
	gst_bin_add_many (GST_BIN (thread), filesrc, demuxer, decoder, converter, audiosink, NULL);

	/* link them in the logical order */
	gst_element_link_many (filesrc, demuxer, decoder, converter, audiosink, NULL);

	return thread;
}

static GstElement *
org_gnome_audio_inline_gst_flac_thread (GstElement *filesrc)
{
	GstElement *thread, *decoder, *audiosink;

	/* create a new thread to hold the elements */
	thread = gst_thread_new ("org-gnome-audio-inline-flac-thread");

	/* now it's time to get the decoder */
	decoder = gst_element_factory_make ("flacdec", "decoder");
                                
	/* and an audio sink */
	audiosink = gst_element_factory_make ("osssink", "play_audio");
                                    
	/* add objects to the main pipeline */
	gst_bin_add_many (GST_BIN (thread), filesrc, decoder, audiosink, NULL);
                                        
	/* link src to sink */
	gst_element_link_many (filesrc, decoder, audiosink, NULL);

	return thread;
}

static GstElement *
org_gnome_audio_inline_gst_mod_thread (GstElement *filesrc)
{
	GstElement *thread, *decoder, *audiosink;

	/* create a new thread to hold the elements */
	thread = gst_thread_new ("org-gnome-audio-inline-flac-thread");

	/* now it's time to get the decoder */
	decoder = gst_element_factory_make ("mikmod", "decoder");
                                
	/* and an audio sink */
	audiosink = gst_element_factory_make ("osssink", "play_audio");
                                    
	/* add objects to the main pipeline */
	gst_bin_add_many (GST_BIN (thread), filesrc, decoder, audiosink, NULL);
                                        
	/* link src to sink */
	gst_element_link_many (filesrc, decoder, audiosink, NULL);

	return thread;
}

static void
org_gnome_audio_inline_play_clicked (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;

	d(printf ("audio inline formatter: play\n"));

	if (!po->filename) {
		CamelStream *stream;
		CamelDataWrapper *data;
		int argc = 1;
		char *argv [] = { "org_gnome_audio_inline", NULL };

		po->filename = e_mktemp ("org-gnome-audio-inline-file-XXXXXX");

		d(printf ("audio inline formatter: write to temp file %s\n", po->filename));

		stream = camel_stream_fs_new_with_name (po->filename, O_RDWR | O_CREAT | O_TRUNC, 0600);
		data = camel_medium_get_content_object (CAMEL_MEDIUM (po->part));
		camel_data_wrapper_decode_to_stream (data, stream);
		camel_stream_flush (stream);
		camel_object_unref (stream);

		d(printf ("audio inline formatter: init gst thread\n"));

		if (gst_init_check (&argc, (char ***) &argv)) {
			CamelContentType *type;
			GstElement *filesrc;

			/* create a disk reader */
			filesrc = gst_element_factory_make ("filesrc", "disk_source");
			g_object_set (G_OBJECT (filesrc), "location", po->filename, NULL);

			type = camel_mime_part_get_content_type (po->part);
			if (type) {
				if (!strcasecmp (type->type, "audio")) {
					if (!strcasecmp (type->subtype, "mpeg") || !strcasecmp (type->subtype, "x-mpeg")
					    || !strcasecmp (type->subtype, "mpeg3") || !strcasecmp (type->subtype, "x-mpeg3")
					    || !strcasecmp (type->subtype, "mp3") || !strcasecmp (type->subtype, "x-mp3")) {
						po->thread = org_gnome_audio_inline_gst_mpeg_thread (filesrc);
					} else if (!strcasecmp (type->subtype, "flac") || !strcasecmp (type->subtype, "x-flac")) {
						po->thread = org_gnome_audio_inline_gst_flac_thread (filesrc);
					} else if (!strcasecmp (type->subtype, "mod") || !strcasecmp (type->subtype, "x-mod")) {
						po->thread = org_gnome_audio_inline_gst_mod_thread (filesrc);
					}
				} else if (!strcasecmp (type->type, "application")) {
					if (!strcasecmp (type->subtype, "ogg") || !strcasecmp (type->subtype, "x-ogg")) {
						po->thread = org_gnome_audio_inline_gst_ogg_thread (filesrc);
					}
				}
			}
		}
	}

	if (po->thread) {
		/* start playing */
		gst_element_set_state (po->thread, GST_STATE_PLAYING);
	}
}

static void
org_gnome_audio_inline_add_button (GtkWidget *box, char *icon_name, GCallback cb, gpointer data)
{
	GtkWidget *icon, *button;
	GdkPixbuf *pixbuf;

	icon = e_icon_factory_get_image (icon_name, E_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show (icon);

	button = gtk_button_new ();
	g_signal_connect (button, "clicked", cb, data);

	gtk_container_add ((GtkContainer *) button, icon);
	gtk_widget_show (button);
	gtk_box_pack_end_defaults (GTK_BOX (box), button);
}

static gboolean
org_gnome_audio_inline_button_panel (EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	GtkWidget *box;
	struct _org_gnome_audio_inline_pobject *po = (struct _org_gnome_audio_inline_pobject *) pobject;

	/* it is OK to call UI functions here, since we are called from UI thread */

	box = gtk_hbutton_box_new ();
	org_gnome_audio_inline_add_button (box, "stock_media-play", G_CALLBACK (org_gnome_audio_inline_play_clicked), po);
	org_gnome_audio_inline_add_button (box, "stock_media-pause", G_CALLBACK (org_gnome_audio_inline_pause_clicked), po);
	org_gnome_audio_inline_add_button (box, "stock_media-stop", G_CALLBACK (org_gnome_audio_inline_stop_clicked), po);
		
	gtk_widget_show (box);
	gtk_container_add ((GtkContainer *) eb, box);

	return TRUE;
}

void
org_gnome_audio_inline_format (void *ep, EMFormatHookTarget *t)
{
	struct _org_gnome_audio_inline_pobject *pobj;
	char *classid = g_strdup_printf ("org-gnome-audio-inline-button-panel-%d", org_gnome_audio_class_id_counter);

	org_gnome_audio_class_id_counter ++;

	d(printf ("audio inline formatter: format classid %s\n", classid));

	pobj = (struct _org_gnome_audio_inline_pobject *) em_format_html_add_pobject ((EMFormatHTML *) t->format, sizeof(*pobj), classid,
										      t->part, org_gnome_audio_inline_button_panel);
	camel_object_ref (t->part);
	pobj->part = t->part;
	pobj->filename = NULL;
	pobj->thread = NULL;
	pobj->object.free = org_gnome_audio_inline_pobject_free;

	camel_stream_printf (t->stream, "<object classid=%s></object>\n", classid);
}
