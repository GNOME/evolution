/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * mail-display.c: Mail display widget
 *
 * Author:
 *   Miguel de Icaza
 *   Bertrand Guiheneuf (bg@aful.org)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <gnome.h>
#include "e-util/e-setup.h"
#include "e-util/e-util.h"
#include "mail-display.h"
#include "mail-format.h"
#include "mail-ops.h"

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *mail_display_parent_class;


/*----------------------------------------------------------------------*
 *                        Callbacks
 *----------------------------------------------------------------------*/

static void
save_data_eexist_cb (int reply, gpointer user_data)
{
	gboolean *ok = user_data;

	*ok = reply == 0;
	gtk_main_quit ();
}

static void
save_data_cb (GtkWidget *widget, gpointer user_data)
{
	CamelStream *output = CAMEL_STREAM (user_data);
	GtkFileSelection *file_select = (GtkFileSelection *)
		gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_SELECTION);
	char *name, buf[1024];
	int fd, nread;

	name = gtk_file_selection_get_filename (file_select);

	fd = open (name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd == -1 && errno == EEXIST) {
		gboolean ok = FALSE;

		gnome_ok_cancel_dialog_modal_parented (
			"A file by that name already exists.\nOverwrite it?",
			save_data_eexist_cb, &ok, GTK_WINDOW (file_select));
		gtk_main ();
		if (!ok)
			return;
		fd = open (name, O_WRONLY | O_TRUNC);
	}

	if (fd == -1) {
		char *msg;

		msg = g_strdup_printf ("Could not open file %s:\n%s",
				       name, g_strerror (errno));
		gnome_error_dialog_parented (msg, GTK_WINDOW (file_select));
		return;
	}

	camel_stream_reset (output);
	do {
		nread = camel_stream_read (output, buf, sizeof (buf));
		if (nread > 0)
			write (fd, buf, nread);
	} while (!camel_stream_eos (output));
	close (fd);

	gtk_widget_destroy (GTK_WIDGET (file_select));
}

static void
save_data (const char *cid, CamelMimeMessage *message)
{
	CamelDataWrapper *data;
	CamelStream *output;
	GtkFileSelection *file_select;
	char *filename;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	data = gtk_object_get_data (GTK_OBJECT (message), cid);
	g_return_if_fail (CAMEL_IS_DATA_WRAPPER (data));
	output = camel_data_wrapper_get_output_stream (data);
	g_return_if_fail (CAMEL_IS_STREAM (output));

	file_select = GTK_FILE_SELECTION (gtk_file_selection_new ("Save Attachment"));
	filename = gtk_object_get_data (GTK_OBJECT (data), "filename");
	if (filename)
		filename = g_strdup_printf ("%s/%s", evolution_dir, filename);
	else
		filename = g_strdup_printf ("%s/attachment", evolution_dir);
	gtk_file_selection_set_filename (file_select, filename);
	g_free (filename);

	gtk_signal_connect (GTK_OBJECT (file_select->ok_button), "clicked", 
			    GTK_SIGNAL_FUNC (save_data_cb), output);
	gtk_signal_connect_object (GTK_OBJECT (file_select->cancel_button),
				   "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (file_select));

	gtk_widget_show (GTK_WIDGET (file_select));
}

static void
on_link_clicked (GtkHTML *html, const char *url, gpointer user_data)
{
	if (!strncasecmp (url, "news:", 5) ||
	    !strncasecmp (url, "nntp:", 5))
		g_warning ("Can't handle news URLs yet.");
	else if (!strncasecmp (url, "mailto:", 7))
		send_to_url (url);
	else if (!strncasecmp (url, "cid:", 4))
		save_data (url + 4, user_data);
	else
		gnome_url_show (url);
}

static void
on_url_requested (GtkHTML *html, const char *url, GtkHTMLStreamHandle handle,
		  gpointer user_data)
{
	char buf[1024];
	int nread;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (user_data);

	if (strncmp (url, "x-gnome-icon:", 13) == 0) {
		const char *name = url + 13;
		char *path = gnome_pixmap_file (name);
		int fd;

		g_return_if_fail (path != NULL);
		fd = open (path, O_RDONLY);
		g_free (path);
		g_return_if_fail (fd != -1);

		while (1) {
			nread = read (fd, buf, sizeof (buf));
			if (nread < 1)
				break;
			gtk_html_write (html, handle, buf, nread);
		}
		close (fd);
	} else if (strncmp (url, "cid:", 4) == 0) {
		const char *cid = url + 4;
		CamelDataWrapper *data;
		CamelStream *output;

		data = gtk_object_get_data (GTK_OBJECT (message), cid);
		g_return_if_fail (CAMEL_IS_DATA_WRAPPER (data));

		output = camel_data_wrapper_get_output_stream (data);
		g_return_if_fail (CAMEL_IS_STREAM (output));

		camel_stream_reset (output);
		do {
			nread = camel_stream_read (output, buf, sizeof (buf));
			if (nread > 0)
				gtk_html_write (html, handle, buf, nread);
		} while (!camel_stream_eos (output));
	} else
		return;
}

/* HTML part code */
static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	requisition->height = GTK_LAYOUT (widget)->height;
	requisition->width = GTK_LAYOUT (widget)->width;
}

void
mail_html_new (GtkHTML **html, GtkHTMLStreamHandle **stream,
	       CamelMimeMessage *root, gboolean init)
{
	*html = GTK_HTML (gtk_html_new ());
	gtk_html_set_editable (*html, FALSE);
	gtk_signal_connect (GTK_OBJECT (*html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_signal_connect (GTK_OBJECT (*html), "url_requested",
			    GTK_SIGNAL_FUNC (on_url_requested), root);
	gtk_signal_connect (GTK_OBJECT (*html), "link_clicked",
			    GTK_SIGNAL_FUNC (on_link_clicked), root);

	*stream = gtk_html_begin (*html, "");
	if (init) {
		mail_html_write (*html, *stream, HTML_HEADER
				 "<BODY TEXT=\"#000000\" "
				 "BGCOLOR=\"#FFFFFF\">\n");
	}
}

void
mail_html_write (GtkHTML *html, GtkHTMLStreamHandle *stream,
		 const char *format, ...)
{
	char *buf;
	va_list ap;

	va_start (ap, format);
	buf = g_strdup_vprintf (format, ap);
	va_end (ap);
	gtk_html_write (html, stream, buf, strlen (buf));
	g_free (buf);
}

void
mail_html_end (GtkHTML *html, GtkHTMLStreamHandle *stream,
	       gboolean finish, GtkBox *box)
{
	GtkWidget *scroll;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (html));

	if (finish)
		mail_html_write (html, stream, "</BODY></HTML>\n");
	gtk_html_end (html, stream, GTK_HTML_STREAM_OK);

	gtk_box_pack_start (box, scroll, FALSE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (html));
	gtk_widget_show (scroll);
}




/**
 * mail_display_set_message:
 * @mail_display: the mail display object
 * @mime_message: the input camel medium
 *
 * Makes the mail_display object show the contents of the medium
 * param. This means feeding mail_display->body_stream and
 * mail_display->headers_stream with html.
 *
 **/
void 
mail_display_set_message (MailDisplay *mail_display, 
			  CamelMedium *medium)
{
	GtkAdjustment *adj;

	/*
	 * for the moment, camel-formatter deals only with 
	 * mime messages, but in the future, it should be 
	 * able to deal with any medium.
	 * It can work on pretty much data wrapper, but in 
	 * fact, only the medium class has the distinction 
	 * header / body 
	 */
	if (!CAMEL_IS_MIME_MESSAGE (medium))
		return;

	/* Clean up from previous message. */
	if (mail_display->current_message) {
		GtkContainer *container =
			GTK_CONTAINER (mail_display->inner_box);
		GList *htmls;

		htmls = gtk_container_children (container);
		while (htmls) {
			gtk_container_remove (container, htmls->data);
			htmls = htmls->next;
		}

		gtk_object_unref (GTK_OBJECT (mail_display->current_message));
	}

	mail_display->current_message = CAMEL_MIME_MESSAGE (medium);
	gtk_object_ref (GTK_OBJECT (medium));

	mail_format_mime_message (CAMEL_MIME_MESSAGE (medium),
				  mail_display->inner_box);

	adj = gtk_scrolled_window_get_vadjustment (mail_display->scroll);
	gtk_adjustment_set_value (adj, 0);
	gtk_scrolled_window_set_vadjustment (mail_display->scroll, adj);

	adj = gtk_scrolled_window_get_hadjustment (mail_display->scroll);
	gtk_adjustment_set_value (adj, 0);
	gtk_scrolled_window_set_hadjustment (mail_display->scroll, adj);
}


/*----------------------------------------------------------------------*
 *                     Standard Gtk+ Class functions
 *----------------------------------------------------------------------*/

static void
mail_display_init (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	/* various other initializations */
	mail_display->current_message = NULL;
}

static void
mail_display_destroy (GtkObject *object)
{
	/* MailDisplay *mail_display = MAIL_DISPLAY (object); */

	mail_display_parent_class->destroy (object);
}

static void
mail_display_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = mail_display_destroy;
	mail_display_parent_class = gtk_type_class (PARENT_TYPE);
}

GtkWidget *
mail_display_new (FolderBrowser *parent_folder_browser)
{
	MailDisplay *mail_display = gtk_type_new (mail_display_get_type ());
	GtkWidget *scroll, *vbox;

	g_assert (parent_folder_browser);

	mail_display->parent_folder_browser = parent_folder_browser;

	gtk_box_set_homogeneous (GTK_BOX (mail_display), FALSE);
	gtk_widget_show (GTK_WIDGET (mail_display));

	/* For now, the box only contains a single scrolled window,
	 * which in turn contains a vbox itself.
	 */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_ALWAYS);
	gtk_box_pack_start_defaults (GTK_BOX (mail_display),
				     GTK_WIDGET (scroll));
	gtk_widget_show (GTK_WIDGET (scroll));

	vbox = gtk_vbox_new (FALSE, 2);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll),
					       vbox);
	gtk_widget_show (GTK_WIDGET (vbox));

	mail_display->scroll = GTK_SCROLLED_WINDOW (scroll);
	mail_display->inner_box = GTK_BOX (vbox);

	return GTK_WIDGET (mail_display);
}



E_MAKE_TYPE (mail_display, "MailDisplay", MailDisplay, mail_display_class_init, mail_display_init, PARENT_TYPE);
