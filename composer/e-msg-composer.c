/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer.c
 *
 * Copyright (C) 1999  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ettore Perazzoli, Jeffrey Stedfast
 */

/*

   TODO

   - Somehow users should be able to see if any file(s) are attached even when
     the attachment bar is not shown.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>

#include <bonobo.h>
#include <bonobo/bonobo-stream-memory.h>
#include <glade/glade.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <gtkhtml/gtkhtml.h>

#include "camel/camel.h"
#include "camel-charset-map.h"

#include "mail/mail.h"
#include "mail/mail-crypto.h"
#include "mail/mail-tools.h"
#include "mail/mail-threads.h"

#include "e-util/e-html-utils.h"
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-scroll-frame.h>

#include "e-msg-composer.h"
#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"
#include "e-msg-composer-select-file.h"

#include "Editor.h"
#include "listener.h"

#include <libgnomevfs/gnome-vfs.h>

#define GNOME_GTKHTML_EDITOR_CONTROL_ID "OAFIID:GNOME_GtkHTML_Editor"


#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 500

enum {
	SEND,
	POSTPONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GnomeAppClass *parent_class = NULL;

/* local prototypes */
static GList *add_recipients (GList *list, const char *recips, gboolean decode);
static void free_recipients (GList *list);
static void handle_multipart (EMsgComposer *composer, CamelMultipart *multipart, int depth);


static char *
get_text (Bonobo_PersistStream persist, char *format)
{
	BonoboStream *stream;
	BonoboStreamMem *stream_mem;
	CORBA_Environment ev;
	char *text;
	
	CORBA_exception_init (&ev);
	
	stream = bonobo_stream_mem_create (NULL, 0, FALSE, TRUE);
	Bonobo_PersistStream_save (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
				   format, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception getting mail '%s'",
			   bonobo_exception_get_text (&ev));
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	stream_mem = BONOBO_STREAM_MEM (stream);
	text = g_malloc (stream_mem->pos + 1);
	memcpy (text, stream_mem->buffer, stream_mem->pos);
	text[stream_mem->pos] = 0;
	bonobo_object_unref (BONOBO_OBJECT(stream));
	
	return text;
}

#define LINE_LEN 72

typedef enum {
	MSG_FORMAT_PLAIN,
	MSG_FORMAT_ALTERNATIVE,
} MsgFormat;

static gboolean
is_8bit (const guchar *text)
{
	guchar *c;
	
	for (c = (guchar *) text; *c; c++)
		if (*c > (guchar) 127)
			return TRUE;
	
	return FALSE;
}

static int
best_encoding (const guchar *text)
{
	guchar *ch;
	int count = 0;
	int total;
	
	for (ch = (guchar *) text; *ch; ch++)
		if (*ch > (guchar) 127)
			count++;
	
	total = (int) (ch - text);
	
	if ((float) count <= total * 0.17)
		return CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE;
	else
		return CAMEL_MIME_PART_ENCODING_BASE64;
}

static char *
best_content (gchar *plain)
{
	char *result;
	const char *best;

	if ((best = camel_charset_best (plain, strlen (plain)))) {
		result = g_strdup_printf ("text/plain; charset=%s", best);
	} else {
		result = g_strdup ("text/plain");
	}
	
	return result;
}

static gboolean
clear_inline_images (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);

	return TRUE;
}

void
e_msg_composer_clear_inlined_table (EMsgComposer *composer)
{
	g_hash_table_foreach_remove (composer->inline_images, clear_inline_images, NULL);
}

static void
add_inlined_image (gpointer key, gpointer value, gpointer data)
{
	gchar *file_name          = (gchar *) key;
	gchar *cid                = (gchar *) value;
	gchar *id, *mime_type;
	CamelMultipart *multipart = (CamelMultipart *) data;
	CamelStream *stream;
	CamelDataWrapper *wrapper;
	CamelMimePart *part;
	struct stat statbuf;

	/* check for regular file */
	if (stat (file_name, &statbuf) < 0 || !S_ISREG (statbuf.st_mode))
		return;

	if (!(stream = camel_stream_fs_new_with_name (file_name, O_RDONLY, 0)))
		return;

	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, stream);
	camel_object_unref (CAMEL_OBJECT (stream));

	mime_type = e_msg_composer_guess_mime_type (file_name);
	camel_data_wrapper_set_mime_type (wrapper, mime_type ? mime_type : "application/octet-stream");
	g_free (mime_type);

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (CAMEL_OBJECT (wrapper));

	id = g_strconcat ("<", cid, ">", NULL);
	camel_mime_part_set_content_id (part, id);
	g_free (id);
	/* FIXME: should this use g_basename (file_name)? */
	camel_mime_part_set_filename (part, strchr (file_name, '/') ? strrchr (file_name, '/') + 1 : file_name);
	camel_mime_part_set_encoding (part, CAMEL_MIME_PART_ENCODING_BASE64);

	camel_multipart_add_part (multipart, part);
	camel_object_unref (CAMEL_OBJECT (part));
}

static void
add_inlined_images (EMsgComposer *composer, CamelMultipart *multipart)
{
	g_hash_table_foreach (composer->inline_images, add_inlined_image, multipart);
}

/* This functions builds a CamelMimeMessage for the message that the user has
   composed in `composer'.  */
static CamelMimeMessage *
build_message (EMsgComposer *composer)
{
	EMsgComposerAttachmentBar *attachment_bar =
		E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar);
	MsgFormat type = MSG_FORMAT_ALTERNATIVE;
	CamelMimeMessage *new;
	CamelMultipart *body = NULL;
	CamelMimePart *part;
	gchar *from = NULL;
	gboolean plain_e8bit = FALSE, html_e8bit = FALSE;
	char *html = NULL, *plain = NULL;
	char *content_type = NULL;
	int i;
	

	if (composer->persist_stream_interface == CORBA_OBJECT_NIL)
		return NULL;
	
	if (composer->send_html)
		type = MSG_FORMAT_ALTERNATIVE;
	else
		type = MSG_FORMAT_PLAIN;
	
	/* get and/or set the From field */
	from = e_msg_composer_hdrs_get_from (E_MSG_COMPOSER_HDRS (composer->hdrs));
	if (!from) {
		const MailConfigIdentity *id = NULL;
		CamelInternetAddress *ciaddr;
		
		id = mail_config_get_default_identity ();
		ciaddr = camel_internet_address_new ();
		camel_internet_address_add (ciaddr, id->name, id->address);
		from = camel_address_encode (CAMEL_ADDRESS (ciaddr));
		e_msg_composer_hdrs_set_from (E_MSG_COMPOSER_HDRS (composer->hdrs), from);
		camel_object_unref (CAMEL_OBJECT (ciaddr));
	}
	g_free (from);
	
	new = camel_mime_message_new ();
	
	e_msg_composer_hdrs_to_message (E_MSG_COMPOSER_HDRS (composer->hdrs), new);
	for (i = 0; i < composer->extra_hdr_names->len; i++) {
		camel_medium_add_header (CAMEL_MEDIUM (new),
					 composer->extra_hdr_names->pdata[i],
					 composer->extra_hdr_values->pdata[i]);
	}
	
	plain = get_text (composer->persist_stream_interface, "text/plain");

	/* the component has probably died */ 
	if (plain == NULL)
		return NULL;

	plain_e8bit = is_8bit (plain);
	content_type = best_content (plain);

	if (type != MSG_FORMAT_PLAIN) {
		e_msg_composer_clear_inlined_table (composer);
		html = get_text (composer->persist_stream_interface, "text/html");
		
		html_e8bit = is_8bit (html);
		/* the component has probably died */
		if (html == NULL) {
			g_free (plain);
			g_free (content_type);
			return NULL;
		}
	}

	if (type == MSG_FORMAT_ALTERNATIVE) {
		body = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body),
						  "multipart/alternative");
		camel_multipart_set_boundary (body, NULL);
		
		part = camel_mime_part_new ();

		camel_mime_part_set_content (part, plain, strlen (plain), content_type);

		if (plain_e8bit)
			camel_mime_part_set_encoding (part, best_encoding (plain));		

		g_free (plain);
		g_free (content_type);
		camel_multipart_add_part (body, part);
		camel_object_unref (CAMEL_OBJECT (part));
		
		part = camel_mime_part_new ();
		if (g_hash_table_size (composer->inline_images)) {
			CamelMultipart *html_with_images;
			CamelMimePart  *text_html;

			html_with_images = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (html_with_images),
							  "multipart/related");
			camel_multipart_set_boundary (html_with_images, NULL);

			text_html = camel_mime_part_new ();
			camel_mime_part_set_content (text_html, html, strlen (html), "text/html; charset=utf-8");

			if (html_e8bit)
				camel_mime_part_set_encoding (text_html, best_encoding (html));		

			camel_multipart_add_part (html_with_images, text_html);
			camel_object_unref (CAMEL_OBJECT (text_html));

			add_inlined_images (composer, html_with_images);
			camel_medium_set_content_object (CAMEL_MEDIUM (part),
							 CAMEL_DATA_WRAPPER (html_with_images));
		} else {
			camel_mime_part_set_content (part, html, strlen (html), "text/html; charset=utf-8");
			
			if (html_e8bit)
				camel_mime_part_set_encoding (part, best_encoding (html));		
		}

		g_free (html);
		camel_multipart_add_part (body, part);
		camel_object_unref (CAMEL_OBJECT (part));
	}
	
	if (e_msg_composer_attachment_bar_get_num_attachments (attachment_bar)) {
		CamelMultipart *multipart = camel_multipart_new ();
		
		/* Generate a random boundary. */
		camel_multipart_set_boundary (multipart, NULL);
		
		part = camel_mime_part_new ();
		switch (type) {
		case MSG_FORMAT_ALTERNATIVE:
			camel_medium_set_content_object (CAMEL_MEDIUM (part),
							 CAMEL_DATA_WRAPPER (body));
			camel_object_unref (CAMEL_OBJECT (body));
			break;
		case MSG_FORMAT_PLAIN:
			camel_mime_part_set_content (part, plain, strlen (plain), best_content (plain));

			if (plain_e8bit)
				camel_mime_part_set_encoding (part, best_encoding (plain));

			g_free (plain);
			g_free (content_type);
			break;
		}
		camel_multipart_add_part (multipart, part);
		camel_object_unref (CAMEL_OBJECT (part));
		
		e_msg_composer_attachment_bar_to_multipart (attachment_bar, multipart);
		
		camel_medium_set_content_object (CAMEL_MEDIUM (new), CAMEL_DATA_WRAPPER (multipart));
		camel_object_unref (CAMEL_OBJECT (multipart));
	} else {
		switch (type) {
		case MSG_FORMAT_ALTERNATIVE:
			camel_medium_set_content_object (CAMEL_MEDIUM (new), CAMEL_DATA_WRAPPER (body));
			camel_object_unref (CAMEL_OBJECT (body));
			break;
		case MSG_FORMAT_PLAIN:
			camel_mime_part_set_content (CAMEL_MIME_PART (new), plain, strlen (plain), best_content (plain));
			
			if (plain_e8bit)
				camel_mime_part_set_encoding (CAMEL_MIME_PART (new), best_encoding (plain));
			
			g_free (plain);
			g_free (content_type);
			
			break;
		}
	}
	
	return new;
}

static char *
get_signature (const char *sigfile, gboolean in_html)
{
	GString *rawsig;
	gchar  buf[1024];
	gchar *file_name;
	gchar *htmlsig = NULL;
	int fd, n;

	if (!sigfile || !*sigfile) {
		return NULL;
	}

	file_name = in_html ? g_strconcat (sigfile, ".html", NULL) : (gchar *) sigfile;
	
	fd = open (file_name, O_RDONLY);
	if (fd == -1) {
		char *msg;
		
		msg = g_strdup_printf (_("Could not open signature file %s:\n"
					 "%s"), file_name, g_strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);
		
		htmlsig = NULL;
	} else {
		rawsig = g_string_new ("");
		while ((n = read (fd, buf, 1023)) > 0) {
			buf[n] = '\0';
			g_string_append (rawsig, buf);
		}
		close (fd);

		htmlsig = in_html ? rawsig->str : e_text_to_html (rawsig->str, 0);
		g_string_free (rawsig, !in_html);
	}
	if (in_html) g_free (file_name);

	return htmlsig;
}

static void
prepare_engine (EMsgComposer *composer)
{
	CORBA_Environment ev;

	g_assert (composer);
	g_assert (E_IS_MSG_COMPOSER (composer));

	/* printf ("prepare_engine\n"); */

	CORBA_exception_init (&ev);
	composer->editor_engine = (GNOME_GtkHTML_Editor_Engine) bonobo_object_client_query_interface
		(bonobo_widget_get_server (BONOBO_WIDGET (composer->editor)), "IDL:GNOME/GtkHTML/Editor/Engine:1.0", &ev);
	if (composer->editor_engine != CORBA_OBJECT_NIL) {
		
		/* printf ("trying set listener\n"); */
		composer->editor_listener = BONOBO_OBJECT (listener_new (composer));
		if (composer->editor_listener != CORBA_OBJECT_NIL)
			GNOME_GtkHTML_Editor_Engine__set_listener (composer->editor_engine,
							       (GNOME_GtkHTML_Editor_Listener)
							       bonobo_object_dup_ref
							       (bonobo_object_corba_objref (composer->editor_listener), &ev),
							       &ev);
		else
			g_warning ("Can't establish Editor Listener\n");
	} else
		g_warning ("Can't get Editor Engine\n");
	CORBA_exception_free (&ev);
}

static void
mark_orig_text (EMsgComposer *composer)
{
	g_assert (composer);
	g_assert (E_IS_MSG_COMPOSER (composer));

	if (composer->editor_engine != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		CORBA_any *flag = bonobo_arg_new (TC_boolean);
		*((CORBA_boolean *) flag->_value) = CORBA_TRUE;

		CORBA_exception_init (&ev);
		GNOME_GtkHTML_Editor_Engine_setObjectDataByType (composer->editor_engine, "ClueFlow", "orig", flag, &ev);
		CORBA_free (flag);
		CORBA_exception_free (&ev);
	}
}

static void
set_editor_text (EMsgComposer *composer, const char *sig_file, const char *text)
{
	Bonobo_PersistStream persist;
	BonoboStream *stream;
	BonoboWidget *editor;
	CORBA_Environment ev;
	char *sig, *fulltext;
	gboolean html_sig = composer->send_html;

	editor = BONOBO_WIDGET (composer->editor);
	sig    = get_signature (sig_file, html_sig);
	/* if we tried HTML sig and it's not available, try also non HTML signature */
	if (html_sig && !sig) {
		html_sig = FALSE;
		sig      = get_signature (sig_file, html_sig);
	}
		
	if (sig) {
		if (html_sig)
			fulltext = g_strdup_printf ("%s<br>%s",
						    text, sig);
		else if (!strncmp ("-- \n", sig, 3))
			fulltext = g_strdup_printf ("%s<br>\n<pre>\n%s</pre>",
						    text, sig);
		else
			fulltext = g_strdup_printf ("%s<br>\n<pre>\n-- \n%s</pre>",
						    text, sig);
	} else {
		if (!*text)
			return;
		fulltext = (char*)text;
	}
	
	CORBA_exception_init (&ev);
	persist = (Bonobo_PersistStream) bonobo_object_client_query_interface (
		bonobo_widget_get_server (editor), "IDL:Bonobo/PersistStream:1.0", &ev);
	g_assert (persist != CORBA_OBJECT_NIL);
	
	stream = bonobo_stream_mem_create (fulltext, strlen (fulltext),
					   TRUE, FALSE);
	if (sig)
		g_free (fulltext);
	Bonobo_PersistStream_load (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
				   "text/html", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME. Some error message. */
		return;
	}
	if (ev._major != CORBA_SYSTEM_EXCEPTION)
		CORBA_Object_release (persist, &ev);
	
	Bonobo_Unknown_unref (persist, &ev);
	CORBA_exception_free (&ev);
	bonobo_object_unref (BONOBO_OBJECT(stream));

	mark_orig_text (composer);
}


/* Commands.  */

static void
show_attachments (EMsgComposer *composer,
		  gboolean show)
{
	if (show) {
		gtk_widget_show (composer->attachment_scroll_frame);
		gtk_widget_show (composer->attachment_bar);
	} else {
		gtk_widget_hide (composer->attachment_scroll_frame);
		gtk_widget_hide (composer->attachment_bar);
	}

	composer->attachment_bar_visible = show;

	/* Update the GUI.  */

#if 0
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM
		 (glade_xml_get_widget (composer->menubar_gui,
					"menu_view_attachments")),
		 show);
#endif

	/* XXX we should update the toggle toolbar item as well.  At
	   this point, it is not a toggle because Glade is broken.  */
}

static void
save (EMsgComposer *composer,
      const char *file_name)
{
	CORBA_Environment ev;
	char *my_file_name;

	if (file_name != NULL)
		my_file_name = g_strdup (file_name);
	else
		my_file_name = e_msg_composer_select_file (composer, _("Save as..."));

	if (my_file_name == NULL)
		return;

	CORBA_exception_init (&ev);

	Bonobo_PersistFile_save (composer->persist_file_interface, my_file_name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error saving file: %s"), g_basename (file_name));
	}

	CORBA_exception_free (&ev);

	g_free (my_file_name);
}

static void
load (EMsgComposer *composer,
      const char *file_name)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_PersistFile_load (composer->persist_file_interface, file_name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error loading file: %s"), g_basename (file_name));

	CORBA_exception_free (&ev);
}

/* Exit dialog.  (Displays a "Save composition to 'Drafts' before exiting?" warning before actually exiting.)  */

enum { REPLY_YES = 0, REPLY_NO, REPLY_CANCEL };

typedef struct save_draft_input_s {
	EMsgComposer *composer;
} save_draft_input_t;

typedef struct save_draft_data_s {
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
} save_draft_data_t;

static gchar *
describe_save_draft (gpointer in_data, gboolean gerund)
{
	if (gerund) {
		return g_strdup (_("Saving changes to message..."));
	} else {
		return g_strdup (_("Save changes to message..."));
	}
}

static void
setup_save_draft (gpointer in_data, gpointer op_data, CamelException *ex)
{
	save_draft_input_t *input = (save_draft_input_t *) in_data;
	save_draft_data_t *data = (save_draft_data_t *) op_data;
	
	g_return_if_fail (input->composer != NULL);
	
	/* initialize op_data */
	input->composer->send_html = TRUE;  /* always save drafts as HTML to keep formatting */
	data->msg = e_msg_composer_get_message (input->composer);
	data->info = g_new0 (CamelMessageInfo, 1);
	data->info->flags = CAMEL_MESSAGE_DRAFT;
}

static void
do_save_draft (gpointer in_data, gpointer op_data, CamelException *ex)
{
	/*save_draft_input_t *input = (save_draft_input_t *) in_data;*/
	save_draft_data_t *data = (save_draft_data_t *) op_data;
	extern CamelFolder *drafts_folder;
	
	/* perform camel operations */
	mail_tool_camel_lock_up ();
	camel_folder_append_message (drafts_folder, data->msg, data->info, ex);
	mail_tool_camel_lock_down ();
}

static void
cleanup_save_draft (gpointer in_data, gpointer op_data, CamelException *ex)
{
	save_draft_input_t *input = (save_draft_input_t *) in_data;
	/*save_draft_data_t *data = (save_draft_data_t *) op_data;*/
	
	if (camel_exception_is_set (ex)) {
		char *reason;
		
		reason = g_strdup_printf (_("Error saving composition to 'Drafts': %s"),
					  camel_exception_get_description (ex));
		
		gnome_warning_dialog_parented (reason, GTK_WINDOW (input->composer));
		g_free (reason);
	} else {
		gtk_widget_destroy (GTK_WIDGET (input->composer));
	}
}

static const mail_operation_spec op_save_draft = {
	describe_save_draft,
	sizeof (save_draft_data_t),
	setup_save_draft,
	do_save_draft,
	cleanup_save_draft
};

static void
exit_dialog_cb (int reply, EMsgComposer *composer)
{
	save_draft_input_t *input;
	
	switch (reply) {
	case REPLY_YES:
		/* this has to be done async */
		input = g_new0 (save_draft_input_t, 1);
		input->composer = composer;
		mail_operation_queue (&op_save_draft, input, TRUE);
	case REPLY_NO:
		gtk_widget_destroy (GTK_WIDGET (composer));
		break;
	case REPLY_CANCEL:
	default:
	}
}
 
static void
do_exit (EMsgComposer *composer)
{
	GtkWidget *dialog;
	GtkWidget *label;
	gint button;
	
	if (E_MSG_COMPOSER_HDRS (composer->hdrs)->has_changed) {
		dialog = gnome_dialog_new (_("Evolution"),
					   GNOME_STOCK_BUTTON_YES,      /* Save */
					   GNOME_STOCK_BUTTON_NO,       /* Don't save */
					   GNOME_STOCK_BUTTON_CANCEL,   /* Cancel */
					   NULL);
		
		label = gtk_label_new (_("This message has not been sent.\n\nDo you wish to save your changes?"));
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 0);
		gtk_widget_show (label);
		gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (composer));
		gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
		button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		
		exit_dialog_cb (button, composer);
	} else {
		gtk_widget_destroy (GTK_WIDGET (composer));
	}
}

/* Menu callbacks.  */

static void
menu_file_open_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	EMsgComposer *composer;
	char *file_name;
	
	composer = E_MSG_COMPOSER (data);
	
	file_name = e_msg_composer_select_file (composer, _("Open file"));
	if (file_name == NULL)
		return;
	
	load (composer, file_name);
	
	g_free (file_name);
}

static void
menu_file_save_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	EMsgComposer *composer;
	CORBA_char *file_name;
	CORBA_Environment ev;
	
	composer = E_MSG_COMPOSER (data);
	
	CORBA_exception_init (&ev);
	
	file_name = Bonobo_PersistFile_getCurrentFile (composer->persist_file_interface, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		save (composer, NULL);
	} else {
		save (composer, file_name);
		CORBA_free (file_name);
	}
	
	CORBA_exception_free (&ev);
}

static void
menu_file_save_as_cb (BonoboUIComponent *uic,
		      void *data,
		      const char *path)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	save (composer, NULL);
}

static void
menu_file_send_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	gtk_signal_emit (GTK_OBJECT (data), signals[SEND]);
}

static void
menu_file_send_later_cb (BonoboUIComponent *uic,
			 void *data,
			 const char *path)
{
	gtk_signal_emit (GTK_OBJECT (data), signals[POSTPONE]);
}

static void
menu_file_close_cb (BonoboUIComponent *uic,
		    void *data,
		    const char *path)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	do_exit (composer);
}
	
static void
menu_file_add_attachment_cb (BonoboUIComponent *uic,
			     void *data,
			     const char *path)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	e_msg_composer_attachment_bar_attach
		(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
		 NULL);
}

static void
menu_view_attachments_activate_cb (BonoboUIComponent           *component,
				   const char                  *path,
				   Bonobo_UIComponent_EventType type,
				   const char                  *state,
				   gpointer                     user_data)

{
	gboolean new_state;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	new_state = atoi (state);
	
	e_msg_composer_show_attachments (E_MSG_COMPOSER (user_data), new_state);
}

#if 0
static void
insert_file_ok_cb (GtkWidget *widget, void *user_data)
{
	GtkFileSelection *fs;
	GdkAtom selection_atom = GDK_NONE;
	char *name;
	EMsgComposer *composer;
	struct stat sb;
	int fd;
	guint8 *buffer;
	size_t bufsz, actual;
	
	fs = GTK_FILE_SELECTION (gtk_widget_get_ancestor (widget,
							  GTK_TYPE_FILE_SELECTION));
	composer = E_MSG_COMPOSER (user_data);
	name = gtk_file_selection_get_filename (fs);
	
	if (stat (name, &sb) < 0) {
		GtkWidget *dlg;
		
		dlg = gnome_error_dialog_parented( _("That file does not exist."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}
	
	if (!(S_ISREG (sb.st_mode))) {
		GtkWidget *dlg;
		
		dlg = gnome_error_dialog_parented (_("That is not a regular file."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}
	
	if (access (name, R_OK) != 0) {
		GtkWidget *dlg;
		
		dlg = gnome_error_dialog_parented (_("That file exists but is not readable."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}
	
	if ((fd = open (name, O_RDONLY)) < 0) {
		GtkWidget *dlg;
		
		dlg = gnome_error_dialog_parented (_("That file appeared accesible but open(2) failed."),
						   GTK_WINDOW (fs));
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return;
	}
	
	buffer = NULL;
	bufsz = 0;
	actual = 0;
#define CHUNK 5120
	
	while (TRUE) {
		ssize_t chunk;
		
		if (bufsz - actual < CHUNK) {
			bufsz += CHUNK;
			
			if (bufsz >= 102400) {
				GtkWidget *dlg;
				gint result;
				
				dlg = gnome_dialog_new (_("The file is very large (more than 100K).\n"
							  "Are you sure you wish to insert it?"),
							GNOME_STOCK_BUTTON_YES,
							GNOME_STOCK_BUTTON_NO,
							NULL);
				gnome_dialog_set_parent (GNOME_DIALOG (dlg), GTK_WINDOW (fs));
				result = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
				gtk_widget_destroy (GTK_WIDGET (dlg));
				
				if (result == 1)
					goto cleanup;
			}
			
			buffer = g_realloc (buffer, bufsz * sizeof (guint8));
		}
		
		chunk = read (fd, &(buffer[actual]), CHUNK);

		if (chunk < 0) {
			GtkWidget *dlg;
			
			dlg = gnome_error_dialog_parented (_("An error occurred while reading the file."),
							   GTK_WINDOW (fs));
			gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
			gtk_widget_destroy (GTK_WIDGET (dlg));
			goto cleanup;
		}
		
		if (chunk == 0)
			break;
		
		actual += chunk;
	}
	
	buffer[actual] = '\0';
	
	if (selection_atom == GDK_NONE)
		selection_atom = gdk_atom_intern ("TEMP_PASTE", FALSE);
	gtk_object_set_data (GTK_OBJECT (fs), "ev_file_buffer", buffer);
	gtk_selection_owner_set (GTK_WIDGET (fs), selection_atom, GDK_CURRENT_TIME);
	/*gtk_html_paste (composer->send_html);*/
	
 cleanup:
	close (fd);
	g_free (buffer);
	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void
fs_selection_get (GtkWidget *widget, GtkSelectionData *sdata,
		  guint info, guint time)
{
	gchar *buffer;
	GdkAtom encoding;
	gint format;
	guchar *ctext;
	gint length;
	
	buffer = gtk_object_get_data (GTK_OBJECT (widget), "ev_file_buffer");
	if (gdk_string_to_compound_text (buffer, &encoding, &format, &ctext,
					 &length) == Success)
		gtk_selection_data_set (sdata, encoding, format, ctext, length);
	g_free (buffer);
	gtk_object_remove_data (GTK_OBJECT (widget), "ev_file_buffer");
}

#endif
static void
menu_file_insert_file_cb (BonoboUIComponent *uic,
		void *data,
		const char *path)
{
#if 0
	EMsgComposer *composer;
	GtkFileSelection *fs;
	
	composer = E_MSG_COMPOSER (data);
	
	fs = GTK_FILE_SELECTION (gtk_file_selection_new ("Choose File"));
	/* FIXME: remember the location or something */
	/*gtk_file_selection_set_filename( fs, g_get_home_dir() );*/
	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    GTK_SIGNAL_FUNC (insert_file_ok_cb), data);
	gtk_signal_connect_object (GTK_OBJECT (fs->cancel_button), 
				   "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy), 
				   GTK_OBJECT (fs));
	gtk_widget_show (GTK_WIDGET(fs));
#else
	g_message ("Insert file is unimplemented! oh no!");
#endif
}

static void
menu_format_html_cb (BonoboUIComponent           *component,
		     const char                  *path,
		     Bonobo_UIComponent_EventType type,
		     const char                  *state,
		     gpointer                     user_data)

{
	EMsgComposer *composer;
	gboolean new_state;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	composer = E_MSG_COMPOSER (user_data);
	
	new_state = atoi (state);
	
	if ((new_state && composer->send_html) ||
	    (! new_state && ! composer->send_html))
		return;
	
	e_msg_composer_set_send_html (composer, new_state);
}


static BonoboUIVerb verbs [] = {

	BONOBO_UI_VERB ("FileOpen",   menu_file_open_cb),
	BONOBO_UI_VERB ("FileSave",   menu_file_save_cb),
	BONOBO_UI_VERB ("FileSaveAs", menu_file_save_as_cb),
	BONOBO_UI_VERB ("FileClose",  menu_file_close_cb),
		  
	BONOBO_UI_VERB ("FileInsertFile", menu_file_insert_file_cb),
	BONOBO_UI_VERB ("FileAttach",     menu_file_add_attachment_cb),
		  
	BONOBO_UI_VERB ("FileSend",       menu_file_send_cb),
	BONOBO_UI_VERB ("FileSendLater",  menu_file_send_later_cb),
	
	BONOBO_UI_VERB_END
};	

static void
setup_ui (EMsgComposer *composer)
{
	BonoboUIContainer *container;
	
	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (composer));
	
	composer->uic = bonobo_ui_component_new ("evolution-message-composer");
	bonobo_ui_component_set_container (
		composer->uic, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	
	bonobo_ui_component_add_verb_list_with_data (
		composer->uic, verbs, composer);	
	
	bonobo_ui_util_set_ui (composer->uic, EVOLUTION_DATADIR,
			       "evolution-message-composer.xml",
			       "evolution-message-composer");
	
	bonobo_ui_component_set_prop (composer->uic, "/commands/FormatHtml",
				      "state", composer->send_html ? "1" : "0", NULL);
	
	bonobo_ui_component_add_listener (
		composer->uic, "FormatHtml",
		menu_format_html_cb, composer);
	
	bonobo_ui_component_add_listener (
		composer->uic, "ViewAttach",
		menu_view_attachments_activate_cb, composer);
}


/* Miscellaneous callbacks.  */

static void
attachment_bar_changed_cb (EMsgComposerAttachmentBar *bar,
			   void *data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	if (e_msg_composer_attachment_bar_get_num_attachments (bar) > 0)
		e_msg_composer_show_attachments (composer, TRUE);
	else
		e_msg_composer_show_attachments (composer, FALSE);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposer *composer;
	CORBA_Environment ev;
	
	composer = E_MSG_COMPOSER (object);
	
	if (composer->uic)
		bonobo_object_unref (BONOBO_OBJECT (composer->uic));
	composer->uic = NULL;

	/* FIXME?  I assume the Bonobo widget will get destroyed
           normally?  */
	
	if (composer->address_dialog != NULL)
		gtk_widget_destroy (composer->address_dialog);
	if (composer->hdrs != NULL)
		gtk_widget_destroy (composer->hdrs);
	
	if (composer->extra_hdr_names) {
		int i;
		
		for (i = 0; i < composer->extra_hdr_names->len; i++) {
			g_free (composer->extra_hdr_names->pdata[i]);
			g_free (composer->extra_hdr_values->pdata[i]);
		}
		g_ptr_array_free (composer->extra_hdr_names, TRUE);
		g_ptr_array_free (composer->extra_hdr_values, TRUE);
	}

	e_msg_composer_clear_inlined_table (composer);
	g_hash_table_destroy (composer->inline_images);
	
	CORBA_exception_init (&ev);
	
	if (composer->persist_stream_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_stream_interface, &ev);
		CORBA_Object_release (composer->persist_stream_interface, &ev);
	}
	
	if (composer->persist_file_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_file_interface, &ev);
		CORBA_Object_release (composer->persist_file_interface, &ev);
	}
	
	if (composer->editor_engine != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->editor_engine, &ev);
		CORBA_Object_release (composer->editor_engine, &ev);
	}

	CORBA_exception_free (&ev);

	if (composer->editor_listener)
		bonobo_object_unref (composer->editor_listener);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static int
delete_event (GtkWidget *widget,
	      GdkEventAny *event)
{
	do_exit (E_MSG_COMPOSER (widget));
	
	return TRUE;
}

static void
drag_data_received (EMsgComposer *composer,
		    GdkDragContext *context,
		    gint x,
		    gint y,
		    GtkSelectionData *selection,
		    guint info,
		    guint time)
{
	gchar *temp, *filename;
	
	filename = g_strdup (selection->data);
	temp = strchr (filename, '\n');
	if (temp) {
		if (*(temp - 1) == '\r')
			*(temp - 1) = '\0';
		*temp = '\0';
	}
	
	/* Chop the file: part off */
	if (strncasecmp (filename, "file:", 5) == 0) {
		temp = g_strdup (filename + 5);
		g_free (filename);
		filename = temp;
	}
	
	e_msg_composer_attachment_bar_attach
		(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
		 filename);
	
	g_free (filename);
}


static void
class_init (EMsgComposerClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	
	object_class->destroy = destroy;
	
	widget_class->delete_event = delete_event;
	
	parent_class = gtk_type_class (bonobo_window_get_type ());
	
	signals[SEND] =
		gtk_signal_new ("send",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, send),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	signals[POSTPONE] =
		gtk_signal_new ("postpone",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, postpone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposer *composer)
{
	composer->uic                      = NULL;
	
	composer->hdrs                     = NULL;
	composer->extra_hdr_names          = g_ptr_array_new ();
	composer->extra_hdr_values         = g_ptr_array_new ();
	
	composer->editor                   = NULL;
	
	composer->address_dialog           = NULL;
	
	composer->attachment_bar           = NULL;
	composer->attachment_scroll_frame  = NULL;
	
	composer->persist_file_interface   = CORBA_OBJECT_NIL;
	composer->persist_stream_interface = CORBA_OBJECT_NIL;

	composer->editor_engine            = CORBA_OBJECT_NIL;
	composer->inline_images            = g_hash_table_new (g_str_hash, g_str_equal);
	
	composer->attachment_bar_visible   = FALSE;
	composer->send_html                = FALSE;
	composer->pgp_sign                 = FALSE;
	composer->pgp_encrypt              = FALSE;
}


GtkType
e_msg_composer_get_type (void)
{
	static GtkType type = 0;
	
	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposer",
			sizeof (EMsgComposer),
			sizeof (EMsgComposerClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (bonobo_window_get_type (), &info);
	}
	
	return type;
}

/**
 * e_msg_composer_construct:
 * @composer: A message composer widget
 * 
 * Construct @composer.
 **/
void
e_msg_composer_construct (EMsgComposer *composer)
{
	GtkWidget *vbox;
	BonoboObject *editor_server;

	static GtkTargetEntry drop_types[] = {
		{"text/uri-list", 0, 1}
	};
	
	g_return_if_fail (gtk_main_level () > 0);
	
	gtk_window_set_default_size (GTK_WINDOW (composer),
				     DEFAULT_WIDTH, DEFAULT_HEIGHT);
	
	bonobo_window_construct (BONOBO_WINDOW (composer), "e-msg-composer",
				 _("Compose a message"));
	
	/* DND support */
	gtk_drag_dest_set (GTK_WIDGET (composer), GTK_DEST_DEFAULT_ALL,
			   drop_types, 1, GDK_ACTION_COPY);
	gtk_signal_connect (GTK_OBJECT (composer), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received), NULL);
	
	setup_ui (composer);
	
	vbox = gtk_vbox_new (FALSE, 0);
	
	composer->hdrs = e_msg_composer_hdrs_new ();
	gtk_box_pack_start (GTK_BOX (vbox), composer->hdrs, FALSE, FALSE, 0);
	gtk_widget_show (composer->hdrs);
	
	/* Editor component.  */
	composer->editor = bonobo_widget_new_control (
		GNOME_GTKHTML_EDITOR_CONTROL_ID,
		bonobo_ui_component_get_container (composer->uic));
	
	if (!composer->editor)
		return;
	
	editor_server = BONOBO_OBJECT (bonobo_widget_get_server (BONOBO_WIDGET (composer->editor)));
	
	composer->persist_file_interface
		= bonobo_object_query_interface (editor_server, "IDL:Bonobo/PersistFile:1.0");
	composer->persist_stream_interface
		= bonobo_object_query_interface (editor_server, "IDL:Bonobo/PersistStream:1.0");
	
	gtk_widget_show (composer->editor);
	gtk_box_pack_start (GTK_BOX (vbox), composer->editor, TRUE, TRUE, 0);
	gtk_widget_show (composer->editor);

	/* Attachment editor, wrapped into an EScrollFrame.  We don't
           show it for now.  */
	
	composer->attachment_scroll_frame = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (composer->attachment_scroll_frame),
					GTK_SHADOW_IN);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (composer->attachment_scroll_frame),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	composer->attachment_bar = e_msg_composer_attachment_bar_new (NULL);
	GTK_WIDGET_SET_FLAGS (composer->attachment_bar, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (composer->attachment_scroll_frame),
			   composer->attachment_bar);
	gtk_box_pack_start (GTK_BOX (vbox),
			    composer->attachment_scroll_frame,
			    FALSE, FALSE, GNOME_PAD_SMALL);
	
	gtk_signal_connect (GTK_OBJECT (composer->attachment_bar), "changed",
			    GTK_SIGNAL_FUNC (attachment_bar_changed_cb), composer);
	
	bonobo_window_set_contents (BONOBO_WINDOW (composer), vbox);
	gtk_widget_show (vbox);
	
	e_msg_composer_show_attachments (composer, FALSE);
	
	/* Set focus on the `To:' field.
	
	   gtk_widget_grab_focus (e_msg_composer_hdrs_get_to_entry (E_MSG_COMPOSER_HDRS (composer->hdrs)));
	   GTK_WIDGET_SET_FLAGS (composer->editor, GTK_CAN_FOCUS);
	   gtk_window_set_focus (GTK_WINDOW (composer), composer->editor); */
	gtk_widget_grab_focus (composer->editor);
}

static EMsgComposer *
create_composer (void)
{
	EMsgComposer *new;
	
	new = gtk_type_new (E_TYPE_MSG_COMPOSER);
	e_msg_composer_construct (new);
	if (!new->editor) {
		e_notice (GTK_WINDOW (new), GNOME_MESSAGE_BOX_ERROR,
			  _("Could not create composer window."));
		gtk_object_unref (GTK_OBJECT (new));
		return NULL;
	}
	prepare_engine (new);

	return new;
}

/**
 * e_msg_composer_new:
 *
 * Create a new message composer widget.
 * 
 * Return value: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new (void)
{
	EMsgComposer *new;
	
	new = create_composer ();
	if (new) {
		/* Load the signature, if any. */
		set_editor_text (new, NULL, "");
	}
	
	return new;
}

/**
 * e_msg_composer_new_with_sig_file:
 *
 * Create a new message composer widget. Sets the signature file.
 * 
 * Return value: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_with_sig_file (const char *sig_file, gboolean send_html)
{
	EMsgComposer *new;
	
	new = create_composer ();
	if (new) {
		e_msg_composer_set_send_html (new, send_html);
		/* Load the signature, if any. */
		set_editor_text (new, sig_file, "");
		
		e_msg_composer_set_sig_file (new, sig_file);
	}
	
	return new;
}

static void
handle_multipart_alternative (EMsgComposer *composer, CamelMultipart *multipart)
{
	/* Find the text/html part and set the composer body to it's contents */
	int i, nparts;
	
	nparts = camel_multipart_get_number (multipart);
	
	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelMimePart *mime_part;
		
		mime_part = camel_multipart_get_part (multipart, i);
		content_type = camel_mime_part_get_content_type (mime_part);
		
		if (header_content_type_is (content_type, "text", "html")) {
			CamelDataWrapper *contents;
			char *text, *final_text;
			gboolean is_html;
			
			contents = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			text = mail_get_message_body (contents, FALSE, &is_html);
			if (text) {
				if (is_html)
					final_text = g_strdup (text);
				else
					final_text = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_NL |
								     E_TEXT_TO_HTML_CONVERT_SPACES);
				g_free (text);
				
				e_msg_composer_set_body_text (composer, final_text);
			}
			
			return;
		}
	}
}

static void
handle_multipart (EMsgComposer *composer, CamelMultipart *multipart, int depth)
{
	int i, nparts;
	
	nparts = camel_multipart_get_number (multipart);
	
	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelMimePart *mime_part;
		
		mime_part = camel_multipart_get_part (multipart, i);
		content_type = camel_mime_part_get_content_type (mime_part);
		
		if (header_content_type_is (content_type, "multipart", "alternative")) {
			/* this structure contains the body */
			CamelDataWrapper *wrapper;
			CamelMultipart *mpart;
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			mpart = CAMEL_MULTIPART (wrapper);
			
			handle_multipart_alternative (composer, mpart);
		} else if (header_content_type_is (content_type, "multipart", "*")) {
			/* another layer of multipartness... */
			CamelDataWrapper *wrapper;
			CamelMultipart *mpart;
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			mpart = CAMEL_MULTIPART (wrapper);
			
			handle_multipart (composer, mpart, depth + 1);
		} else if (depth == 0 && i == 0) {
			/* Since the first part is not multipart/alternative, then this must be the body */
			CamelDataWrapper *contents;
			char *text, *final_text;
			gboolean is_html;
			
			contents = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			text = mail_get_message_body (contents, FALSE, &is_html);
			if (text) {
				if (is_html)
					final_text = g_strdup (text);
				else
					final_text = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_NL |
								     E_TEXT_TO_HTML_CONVERT_SPACES);
				g_free (text);
				
				e_msg_composer_set_body_text (composer, final_text);
			}
		} else {
			/* this is a leaf of the tree, so attach it */
			e_msg_composer_attach (composer, mime_part);
		}
	}
}

/**
 * e_msg_composer_new_with_message:
 *
 * Create a new message composer widget.
 * 
 * Return value: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_with_message (CamelMimeMessage *msg)
{
	const CamelInternetAddress *to, *cc, *bcc;
	GList *To = NULL, *Cc = NULL, *Bcc = NULL;
	CamelContentType *content_type;
	const gchar *subject;
	EMsgComposer *new;
	guint len, i;
	
	g_return_val_if_fail (gtk_main_level () > 0, NULL);
	
	new = create_composer ();
	if (!new)
		return NULL;
	
	subject = camel_mime_message_get_subject (msg);
	
	to = camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_BCC);
	
	len = CAMEL_ADDRESS (to)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *name, *addr;
		
		if (camel_internet_address_get (to, i, &name, &addr)) {
			CamelInternetAddress *cia;
			
			cia = camel_internet_address_new ();
			camel_internet_address_add (cia, name, addr);
			To = g_list_append (To, camel_address_encode (CAMEL_ADDRESS (cia)));
			camel_object_unref (CAMEL_OBJECT (cia));
		}
	}
	
	len = CAMEL_ADDRESS (cc)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *name, *addr;
		
		if (camel_internet_address_get (to, i, &name, &addr)) {
			CamelInternetAddress *cia;
			
			cia = camel_internet_address_new ();
			camel_internet_address_add (cia, name, addr);
			Cc = g_list_append (Cc, camel_address_encode (CAMEL_ADDRESS (cia)));
			camel_object_unref (CAMEL_OBJECT (cia));
		}
	}
	
	len = CAMEL_ADDRESS (bcc)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *name, *addr;
		
		if (camel_internet_address_get (to, i, &name, &addr)) {
			CamelInternetAddress *cia;
			
			cia = camel_internet_address_new ();
			camel_internet_address_add (cia, name, addr);
			Bcc = g_list_append (Bcc, camel_address_encode (CAMEL_ADDRESS (cia)));
			camel_object_unref (CAMEL_OBJECT (cia));
		}
	}
	
	e_msg_composer_set_headers (new, To, Cc, Bcc, subject);
	
	free_recipients (To);
	free_recipients (Cc);
	free_recipients (Bcc);
	
	content_type = camel_mime_part_get_content_type (CAMEL_MIME_PART (msg));
	if (header_content_type_is (content_type, "multipart", "alternative")) {
		/* multipart/alternative contains the text/plain and text/html versions of the message body */
		CamelDataWrapper *wrapper;
		CamelMultipart *multipart;
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (CAMEL_MIME_PART (msg)));
		multipart = CAMEL_MULTIPART (wrapper);
		
		handle_multipart_alternative (new, multipart);
	} else if (header_content_type_is (content_type, "multipart", "*")) {
		/* there must be attachments... */
		CamelDataWrapper *wrapper;
		CamelMultipart *multipart;
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (CAMEL_MIME_PART (msg)));
		multipart = CAMEL_MULTIPART (wrapper);
		
		handle_multipart (new, multipart, 0);
	} else {
		/* We either have a text/plain or a text/html part */
		CamelDataWrapper *contents;
		char *text, *final_text;
		gboolean is_html;
		
		contents = camel_medium_get_content_object (CAMEL_MEDIUM (msg));
		text = mail_get_message_body (contents, FALSE, &is_html);
		if (text) {
			if (is_html)
				final_text = g_strdup (text);
			else
				final_text = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_NL |
							     E_TEXT_TO_HTML_CONVERT_SPACES);
			g_free (text);
			
			e_msg_composer_set_body_text (new, final_text);
		}
	}
	
	return new;
}

#if 0
static GList *
add_recipients (GList *list, const char *recips, gboolean decode)
{
	int len;
	char *addr;
	
	while (*recips) {
		len = strcspn (recips, ",");
		if (len) {
			addr = g_strndup (recips, len);
			if (decode)
				camel_url_decode (addr);
			list = g_list_append (list, addr);
		}
		recips += len;
		if (*recips == ',')
			recips++;
	}
	
	return list;
}
#endif

static GList *
add_recipients (GList *list, const char *recips, gboolean decode)
{
	CamelInternetAddress *cia;
	const char *name, *addr;
	int num, i;
	
	cia = camel_internet_address_new ();
	if (decode)
		num = camel_address_decode (CAMEL_ADDRESS (cia), recips);
	else
		num = camel_address_unformat (CAMEL_ADDRESS (cia), recips);
	
	for (i = 0; i < num; i++) {
		if (camel_internet_address_get (cia, i, &name, &addr)) {
			char *str;
			
			str = camel_internet_address_format_address (name, addr);
			list = g_list_append (list, str);
		}
	}
	
	return list;
}

static void
free_recipients (GList *list)
{
	GList *l;
	
	for (l = list; l; l = l->next)
		g_free (l->data);
	g_list_free (list);
}

/**
 * e_msg_composer_new_from_url:
 * @url: a mailto URL
 *
 * Create a new message composer widget, and fill in fields as
 * defined by the provided URL.
 **/
EMsgComposer *
e_msg_composer_new_from_url (const char *url)
{
	EMsgComposer *composer;
	EMsgComposerHdrs *hdrs;
	GList *to = NULL, *cc = NULL, *bcc = NULL;
	char *subject = NULL, *body = NULL;
	const char *p, *header;
	int len, clen;
	char *content;
	
	g_return_val_if_fail (g_strncasecmp (url, "mailto:", 7) == 0, NULL);
	
	composer = e_msg_composer_new ();
	if (!composer)
		return NULL;

	/* Parse recipients (everything after ':' until '?' or eos. */
	p = url + 7;
	len = strcspn (p, "?,");
	if (len) {
		content = g_strndup (p, len);
		to = add_recipients (to, content, TRUE);
		g_free (content);
	}
	
	p += len;
	if (*p == '?') {
		p++;
		
		while (*p) {
			len = strcspn (p, "=&");
			
			/* If it's malformed, give up. */
			if (p[len] != '=')
				break;
			
			header = p;
			p += len + 1;
			
			clen = strcspn (p, "&");
			content = g_strndup (p, clen);
			camel_url_decode (content);
			
			if (!g_strncasecmp (header, "to", len))
				to = add_recipients (to, content, FALSE);
			else if (!g_strncasecmp (header, "cc", len))
				cc = add_recipients (cc, content, FALSE);
			else if (!g_strncasecmp (header, "bcc", len))
				bcc = add_recipients (bcc, content, FALSE);
			else if (!g_strncasecmp (header, "subject", len))
				subject = g_strdup (content);
			else if (!g_strncasecmp (header, "body", len))
				body = g_strdup (content);
			
			g_free (content);
			p += clen;
			if (*p == '&') {
				p++;
				if (!strcmp (p, "amp;"))
					p += 4;
			}
		}
	}
	
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	e_msg_composer_hdrs_set_to (hdrs, to);
	free_recipients (to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	free_recipients (cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	free_recipients (bcc);
	if (subject) {
		e_msg_composer_hdrs_set_subject (hdrs, subject);
		g_free (subject);
	}
	
	if (body) {
		char *htmlbody = e_text_to_html (body, E_TEXT_TO_HTML_PRE);
		set_editor_text (composer, NULL, htmlbody);
		g_free (htmlbody);
	}
	
	return composer;
}

/**
 * e_msg_composer_show_attachments:
 * @composer: A message composer widget
 * @show: A boolean specifying whether the attachment bar should be shown or
 * not
 * 
 * If @show is %FALSE, hide the attachment bar.  Otherwise, show it.
 **/
void
e_msg_composer_show_attachments (EMsgComposer *composer,
				 gboolean show)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	show_attachments (composer, show);
}

/**
 * e_msg_composer_set_headers:
 * @composer: a composer object
 * @to: the values for the "To" header
 * @cc: the values for the "Cc" header
 * @bcc: the values for the "Bcc" header
 * @subject: the value for the "Subject" header
 *
 * Sets the headers in the composer to the given values.
 **/
void 
e_msg_composer_set_headers (EMsgComposer *composer, const GList *to,
			    const GList *cc, const GList *bcc,
			    const char *subject)
{
	EMsgComposerHdrs *hdrs;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	
	e_msg_composer_hdrs_set_to (hdrs, to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	e_msg_composer_hdrs_set_subject (hdrs, subject);
}


/**
 * e_msg_composer_set_body_text:
 * @composer: a composer object
 * @text: the HTML text to initialize the editor with
 *
 * Loads the given HTML text into the editor.
 **/
void
e_msg_composer_set_body_text (EMsgComposer *composer, const char *text)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	set_editor_text (composer, composer->sig_file, text);
}


/**
 * e_msg_composer_add_header:
 * @composer: a composer object
 * @name: the header name
 * @value: the header value
 *
 * Adds a header with @name and @value to the message. This header
 * may not be displayed by the composer, but will be included in
 * the message it outputs.
 **/
void
e_msg_composer_add_header (EMsgComposer *composer, const char *name,
			   const char *value)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);
	
	g_ptr_array_add (composer->extra_hdr_names, g_strdup (name));
	g_ptr_array_add (composer->extra_hdr_values, g_strdup (value));
}


/**
 * e_msg_composer_attach:
 * @composer: a composer object
 * @attachment: the CamelMimePart to attach
 *
 * Attaches @attachment to the message being composed in the composer.
 **/
void
e_msg_composer_attach (EMsgComposer *composer, CamelMimePart *attachment)
{
	EMsgComposerAttachmentBar *bar;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (attachment));
	
	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar);
	e_msg_composer_attachment_bar_attach_mime_part (bar, attachment);
}


/**
 * e_msg_composer_get_message:
 * @composer: A message composer widget
 * 
 * Retrieve the message edited by the user as a CamelMimeMessage.  The
 * CamelMimeMessage object is created on the fly; subsequent calls to this
 * function will always create new objects from scratch.
 * 
 * Return value: A pointer to the new CamelMimeMessage object
 **/
CamelMimeMessage *
e_msg_composer_get_message (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return build_message (composer);
}



/**
 * e_msg_composer_set_sig:
 * @composer: A message composer widget
 * @path: Signature file
 * 
 * Set a signature
 **/
void
e_msg_composer_set_sig_file (EMsgComposer *composer, const char *sig_file)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	composer->sig_file = g_strdup (sig_file);
}

/**
 * e_msg_composer_get_sig_file:
 * @composer: A message composer widget
 * 
 * Get the signature file
 * 
 * Return value: The signature file.
 **/
const char *
e_msg_composer_get_sig_file (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return composer->sig_file;
}


/**
 * e_msg_composer_set_send_html:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "Send HTML" flag set
 * 
 * Set the status of the "Send HTML" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_send_html (EMsgComposer *composer,
			      gboolean send_html)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (composer->send_html && send_html)
		return;
	if (! composer->send_html && ! send_html)
		return;

	composer->send_html = send_html;

	bonobo_ui_component_set_prop (composer->uic, "/commands/FormatHtml",
				      "state", composer->send_html ? "1" : "0", NULL);
}

/**
 * e_msg_composer_get_send_html:
 * @composer: A message composer widget
 * 
 * Get the status of the "Send HTML mail" flag.
 * 
 * Return value: The status of the "Send HTML mail" flag.
 **/
gboolean
e_msg_composer_get_send_html (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->send_html;
}


/**
 * e_msg_composer_set_pgp_sign:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "PGP Sign" flag set
 * 
 * Set the status of the "PGP Sign" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_pgp_sign (EMsgComposer *composer, gboolean pgp_sign)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->pgp_sign && pgp_sign)
		return;
	if (!composer->pgp_sign && !pgp_sign)
		return;
	
	composer->pgp_sign = pgp_sign;
	
	bonobo_ui_component_set_prop (composer->uic, "/commands/SecuritySign",
				      "state", composer->pgp_sign ? "1" : "0", NULL);
}


/**
 * e_msg_composer_get_pgp_sign:
 * @composer: A message composer widget
 * 
 * Get the status of the "PGP Sign" flag.
 * 
 * Return value: The status of the "PGP Sign" flag.
 **/
gboolean
e_msg_composer_get_pgp_sign (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->pgp_sign;
}


/**
 * e_msg_composer_set_pgp_encrypt:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "PGP Encrypt" flag set
 * 
 * Set the status of the "PGP Encrypt" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_pgp_encrypt (EMsgComposer *composer, gboolean pgp_encrypt)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->pgp_encrypt && pgp_encrypt)
		return;
	if (!composer->pgp_encrypt && !pgp_encrypt)
		return;
	
	composer->pgp_encrypt = pgp_encrypt;
	
	bonobo_ui_component_set_prop (composer->uic, "/commands/SecurityEncrypt",
				      "state", composer->pgp_encrypt ? "1" : "0", NULL);
}


/**
 * e_msg_composer_get_pgp_encrypt:
 * @composer: A message composer widget
 * 
 * Get the status of the "PGP Encrypt" flag.
 * 
 * Return value: The status of the "PGP Encrypt" flag.
 **/
gboolean
e_msg_composer_get_pgp_encrypt (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->pgp_encrypt;
}


/**
 * e_msg_composer_guess_mime_type:
 * @file_name: filename
 *
 * Returns the guessed mime type of the file given by #file_name.
 **/
gchar *
e_msg_composer_guess_mime_type (const gchar *file_name)
{
	GnomeVFSFileInfo info;
	GnomeVFSResult result;

	result = gnome_vfs_get_file_info (file_name, &info,
					  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result == GNOME_VFS_OK) {
		gchar *type;

		type = g_strdup (gnome_vfs_file_info_get_mime_type (&info));
		gnome_vfs_file_info_unref (&info);
		return type;
	} else
		return NULL;
}
