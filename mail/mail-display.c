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
#include <gnome.h>
#include "e-util/e-util.h"
#include "mail-display.h"
#include "mail-format.h"

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *mail_display_parent_class;


/*----------------------------------------------------------------------*
 *                        Callbacks
 *----------------------------------------------------------------------*/

static CamelStream *
cid_stream (const char *cid, CamelMimeMessage *message)
{
	CamelDataWrapper *data;

	data = gtk_object_get_data (GTK_OBJECT (message), cid);
	g_return_val_if_fail (CAMEL_IS_DATA_WRAPPER (data), NULL);

	return camel_data_wrapper_get_output_stream (data);
}	

static void
on_link_clicked (GtkHTML *html, const char *url, gpointer user_data)
{
	gnome_url_show (url);
}

static void
on_url_requested (GtkHTML *html, const char *url, GtkHTMLStreamHandle handle,
		  gpointer user_data)
{
	char buf[1024];
	int nread;
	CamelStream *output;
	CamelMimeMessage *message = CAMEL_MIME_MESSAGE (user_data);

	if (strncmp (url, "camel:", 6) == 0) {
		output = GUINT_TO_POINTER (strtoul (url + 6, NULL, 0));
		g_return_if_fail (CAMEL_IS_STREAM (output));
	} else if (strncmp (url, "cid:", 4) == 0) {
		output = cid_stream (url + 4, message);
		g_return_if_fail (CAMEL_IS_STREAM (output));
	} else
		return;

	camel_stream_reset (output);
	do {
		nread = camel_stream_read (output, buf, sizeof (buf));
		if (nread > 0)
			gtk_html_write (html, handle, buf, nread);
	} while (!camel_stream_eos (output));
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
