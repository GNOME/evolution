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
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <gnome.h>
#include "e-util/e-util.h"
#include "e-util/e-html-utils.h"
#include "e-util/e-popup-menu.h"
#include "mail-display.h"
#include "mail.h"

#include <bonobo.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-stream-memory.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *mail_display_parent_class;

static void redisplay (MailDisplay *md, gboolean unscroll);

/*----------------------------------------------------------------------*
 *                        Callbacks
 *----------------------------------------------------------------------*/

static gboolean
write_data_to_file (CamelMimePart *part, const char *name, gboolean unique)
{
	CamelDataWrapper *data;
	CamelStream *stream_fs;
	int fd;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), FALSE);
	data = camel_medium_get_content_object (CAMEL_MEDIUM (part));

	fd = open (name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd == -1 && errno == EEXIST && !unique) {
		GtkWidget *dlg;
		GtkWidget *text;

		dlg = gnome_dialog_new (_("Overwrite file?"),
					GNOME_STOCK_BUTTON_YES, 
					GNOME_STOCK_BUTTON_NO,
					NULL);
		text = gtk_label_new (_("A file by that name already exists.\nOverwrite it?"));
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox), text, TRUE, TRUE, 4);
		gtk_widget_show (text);

		if (gnome_dialog_run_and_close (GNOME_DIALOG (dlg)) != 0)
			return FALSE;
		gtk_widget_destroy (dlg);

		fd = open (name, O_WRONLY | O_TRUNC);
	}

	if (fd == -1) {
		char *msg;

		msg = g_strdup_printf ("Could not open file %s:\n%s",
				       name, g_strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);
		return FALSE;
	}

	stream_fs = camel_stream_fs_new_with_fd (fd);
	if (camel_data_wrapper_write_to_stream (data, stream_fs) == -1
	    || camel_stream_flush (stream_fs) == -1) {
		char *msg;

		msg = g_strdup_printf ("Could not write data: %s",
				       strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);
		camel_object_unref (CAMEL_OBJECT (stream_fs));
		return FALSE;
	}
	camel_object_unref (CAMEL_OBJECT (stream_fs));
	return TRUE;
}

static char *
make_safe_filename (const char *prefix, CamelMimePart *part)
{
	const char *name = NULL;
	char *safe, *p;

	name = camel_mime_part_get_filename (part);
	if (!name)
		name = "attachment";

	p = strrchr (name, '/');
	if (p)
		safe = g_strdup_printf ("%s%s", prefix, p);
	else
		safe = g_strdup_printf ("%s/%s", prefix, name);

	for (p = strrchr (safe, '/') + 1; *p; p++) {
		if (!isascii ((unsigned char)*p) ||
		    strchr (" /'\"`&();|<>${}!", *p))
			*p = '_';
	}

	return safe;
}

static void
save_data_cb (GtkWidget *widget, gpointer user_data)
{
	GtkFileSelection *file_select = (GtkFileSelection *)
		gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_SELECTION);

	write_data_to_file (user_data,
			    gtk_file_selection_get_filename (file_select),
			    FALSE);
	gtk_widget_destroy (GTK_WIDGET (file_select));
}

static gboolean
idle_redisplay (gpointer data)
{
	MailDisplay *md = data;

	md->idle_id = 0;
	redisplay (md, FALSE);
	return FALSE;
}

static void
queue_redisplay (MailDisplay *md)
{
	if (!md->idle_id) {
		md->idle_id = g_idle_add_full (G_PRIORITY_LOW, idle_redisplay,
					       md, NULL);
	}
}

static void
on_link_clicked (GtkHTML *html, const char *url, gpointer user_data)
{
	MailDisplay *md = user_data;

	if (!g_strncasecmp (url, "news:", 5) ||
	    !g_strncasecmp (url, "nntp:", 5))
		g_warning ("Can't handle news URLs yet.");
	else if (!g_strncasecmp (url, "mailto:", 7))
		send_to_url (url);
	else if (!strcmp (url, "x-evolution-decode-pgp:")) {
		g_datalist_set_data (md->data, "show_pgp",
				     GINT_TO_POINTER (1));
		queue_redisplay (md);
	} else
		gnome_url_show (url);
}

static void
save_cb (GtkWidget *widget, gpointer user_data)
{
	CamelMimePart *part = gtk_object_get_data (user_data, "CamelMimePart");
	GtkFileSelection *file_select;
	char *filename;

	filename = make_safe_filename (g_get_home_dir (), part);
	file_select = GTK_FILE_SELECTION (
		gtk_file_selection_new ("Save Attachment"));
	gtk_file_selection_set_filename (file_select, filename);
	g_free (filename);

	gtk_signal_connect (GTK_OBJECT (file_select->ok_button), "clicked", 
			    GTK_SIGNAL_FUNC (save_data_cb), part);
	gtk_signal_connect_object (GTK_OBJECT (file_select->cancel_button),
				   "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (file_select));

	gtk_widget_show (GTK_WIDGET (file_select));
}

static void
launch_cb (GtkWidget *widget, gpointer user_data)
{
	CamelMimePart *part = gtk_object_get_data (user_data, "CamelMimePart");
	GnomeVFSMimeApplication *app;
	GMimeContentField *content_type;
	char *mime_type, *tmpl, *tmpdir, *filename, *argv[2];

	content_type = camel_mime_part_get_content_type (part);
	mime_type = gmime_content_field_get_mime_type (content_type);
	app = gnome_vfs_mime_get_default_application (mime_type);
	g_free (mime_type);

	g_return_if_fail (app != NULL);

	tmpl = g_strdup ("/tmp/evolution.XXXXXX");
#ifdef HAVE_MKDTEMP
	tmpdir = mkdtemp (tmpl);
#else
	tmpdir = mktemp (tmpl);
	if (tmpdir) {
		if (mkdir (tmpdir, S_IRWXU) == -1)
			tmpdir = NULL;
	}
#endif
	if (!tmpdir) {
		char *msg = g_strdup_printf ("Could not create temporary "
					     "directory: %s",
					     g_strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);
		return;
	}

	filename = make_safe_filename (tmpdir, part);

	if (!write_data_to_file (part, filename, TRUE)) {
		g_free (tmpl);
		g_free (filename);
		return;
	}

	argv[0] = app->command;
	argv[1] = filename;

	gnome_execute_async (tmpdir, 2, argv);
	g_free (tmpdir);
	g_free (filename);
}

static void
inline_cb (GtkWidget *widget, gpointer user_data)
{
	MailDisplay *md = gtk_object_get_data (user_data, "MailDisplay");
	CamelMimePart *part = gtk_object_get_data (user_data, "CamelMimePart");

	if (mail_part_is_inline (part))
		camel_mime_part_set_disposition (part, "attachment");
	else
		camel_mime_part_set_disposition (part, "inline");

	queue_redisplay (md);
}

static gboolean
pixmap_press (GtkWidget *ebox, GdkEventButton *event, gpointer user_data)
{
	EPopupMenu menu[] = {
		{ N_("Save to Disk..."), NULL,
		  GTK_SIGNAL_FUNC (save_cb), 0 },
		{ N_("Open in %s..."), NULL,
		  GTK_SIGNAL_FUNC (launch_cb), 1 },
		{ N_("View Inline"), NULL,
		  GTK_SIGNAL_FUNC (inline_cb), 2 },
		{ NULL, NULL, NULL, 0 }
	};
	CamelMimePart *part;
	MailMimeHandler *handler;
	int mask = 0;

	if (event->button != 3)
		return FALSE;

	part = gtk_object_get_data (user_data, "CamelMimePart");
	handler = mail_lookup_handler (gtk_object_get_data (user_data,
							    "mime_type"));

	/* External view item */
	if (handler && handler->application) {
		menu[1].name = g_strdup_printf (menu[1].name,
						handler->application->name);
	} else {
		menu[1].name = g_strdup_printf (menu[1].name,
						N_("External Viewer"));
		mask |= 1;
	}

	/* Inline view item */
	if (handler && handler->builtin) {
		if (!mail_part_is_inline (part)) {
			if (handler->component) {
				OAF_Property *prop;
				char *name;

				prop = oaf_server_info_prop_find (
					handler->component, "name");
				if (!prop) {
					prop = oaf_server_info_prop_find (
						handler->component,
						"description");
				}
				if (prop && prop->v._d == OAF_P_STRING)
					name = prop->v._u.value_string;
				else
					name = "bonobo";
				menu[2].name = g_strdup_printf (
					N_("View Inline (via %s)"), name);
			} else
				menu[2].name = g_strdup (menu[2].name);
		} else
			menu[2].name = g_strdup (N_("Hide"));
	} else {
		menu[2].name = g_strdup (menu[2].name);
		mask |= 2;
	}

	e_popup_menu_run (menu, event, mask, 0, user_data);
	g_free (menu[1].name);
	g_free (menu[2].name);
	return TRUE;
}	

static gboolean
on_object_requested (GtkHTML *html, GtkHTMLEmbedded *eb, gpointer data)
{
	MailDisplay *md = data;
	GHashTable *urls;
	CamelMedium *medium;
	CamelDataWrapper *wrapper;
	OAF_ServerInfo *component;
	GtkWidget *embedded;
	BonoboObjectClient *server;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	GByteArray *ba;
	CamelStream *cstream;
	BonoboStream *bstream;
	char *cid;

	cid = eb->classid;
	if (!strncmp (cid, "popup:", 6))
		cid += 6;
	if (strncmp (cid, "cid:", 4) != 0)
		return FALSE;

	urls = g_datalist_get_data (md->data, "urls");
	g_return_val_if_fail (urls != NULL, FALSE);

	medium = g_hash_table_lookup (urls, cid);
	g_return_val_if_fail (CAMEL_IS_MEDIUM (medium), FALSE);

	if (cid != eb->classid) {
		/* This is a part wrapper */
		const char *icon;
		GtkWidget *pixmap, *ebox;

		icon = gnome_vfs_mime_get_value (eb->type, "icon-filename");
		if (icon) {
			pixmap = gnome_pixmap_new_from_file_at_size (icon,
								     24, 24);
		} else {
			char *filename;

			filename = gnome_pixmap_file ("gnome-unknown.png");
			pixmap = gnome_pixmap_new_from_file_at_size (filename,
								     24, 24);
			g_free (filename);
		}

		ebox = gtk_event_box_new ();
		gtk_widget_set_sensitive (GTK_WIDGET (ebox), TRUE);
		gtk_widget_add_events (GTK_WIDGET (ebox),
				       GDK_BUTTON_PRESS_MASK);
		gtk_signal_connect (GTK_OBJECT (ebox), "button_press_event",
				    GTK_SIGNAL_FUNC (pixmap_press), ebox);
		gtk_object_set_data (GTK_OBJECT (ebox), "MailDisplay", md);
		gtk_object_set_data (GTK_OBJECT (ebox), "CamelMimePart",
				     medium);
		gtk_object_set_data_full (GTK_OBJECT (ebox), "mime_type",
					  g_strdup (eb->type),
					  (GDestroyNotify)g_free);

		gtk_container_add (GTK_CONTAINER (ebox), pixmap);
		gtk_widget_show_all (ebox);
		gtk_container_add (GTK_CONTAINER (eb), ebox);
		return TRUE;
	}

	component = gnome_vfs_mime_get_default_component (eb->type);
	if (!component)
		return FALSE;

	embedded = bonobo_widget_new_subdoc (component->iid, NULL);
	if (embedded) {
		/* FIXME: as of bonobo 0.18, there's an extra
		 * client_site dereference in the BonoboWidget
		 * destruction path that we have to balance out to
		 * prevent problems.
		 */
		bonobo_object_ref (BONOBO_OBJECT(bonobo_widget_get_client_site (
			BONOBO_WIDGET (embedded))));
	} else
		embedded = bonobo_widget_new_control (component->iid, NULL);
	CORBA_free (component);
	if (!embedded)
		return FALSE;

	server = bonobo_widget_get_server (BONOBO_WIDGET (embedded));
	persist = (Bonobo_PersistStream) bonobo_object_client_query_interface (
		server, "IDL:Bonobo/PersistStream:1.0", NULL);
	if (persist == CORBA_OBJECT_NIL) {
		gtk_object_sink (GTK_OBJECT (embedded));
		return FALSE;
	}

	/* Write the data to a CamelStreamMem... */
	ba = g_byte_array_new ();
	cstream = camel_stream_mem_new_with_byte_array (ba);
	wrapper = camel_medium_get_content_object (medium);
	camel_data_wrapper_write_to_stream (wrapper, cstream);

	/* ...convert the CamelStreamMem to a BonoboStreamMem... */
	bstream = bonobo_stream_mem_create (ba->data, ba->len, TRUE, FALSE);
	camel_object_unref (CAMEL_OBJECT (cstream));

	/* ...and hydrate the PersistStream from the BonoboStream. */
	CORBA_exception_init (&ev);
	Bonobo_PersistStream_load (persist,
				   bonobo_object_corba_objref (
					   BONOBO_OBJECT (bstream)),
				   eb->type, &ev);
	bonobo_object_unref (BONOBO_OBJECT (bstream));
	Bonobo_Unknown_unref (persist, &ev);
	CORBA_Object_release (persist, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		gtk_object_sink (GTK_OBJECT (embedded));
		CORBA_exception_free (&ev);				
		return FALSE;
	}
	CORBA_exception_free (&ev);

	gtk_widget_show (embedded);
	gtk_container_add (GTK_CONTAINER (eb), embedded);

	return TRUE;
}

static void
on_url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle,
		  gpointer user_data)
{
	MailDisplay *md = user_data;
	GHashTable *urls;

	urls = g_datalist_get_data (md->data, "urls");
	g_return_if_fail (urls != NULL);

	user_data = g_hash_table_lookup (urls, url);
	if (user_data == NULL)
		return;

	if (strncmp (url, "cid:", 4) == 0) {
		CamelMedium *medium = user_data;
		CamelDataWrapper *data;
		CamelStream *stream_mem;
		GByteArray *ba;

		g_return_if_fail (CAMEL_IS_MEDIUM (medium));
		data = camel_medium_get_content_object (medium);

		ba = g_byte_array_new ();
		stream_mem = camel_stream_mem_new_with_byte_array (ba);
		camel_data_wrapper_write_to_stream (data, stream_mem);
		gtk_html_write (html, handle, ba->data, ba->len);
		camel_object_unref (CAMEL_OBJECT (stream_mem));
	} else if (strncmp (url, "x-evolution-data:", 17) == 0) {
		GByteArray *ba = user_data;

		g_return_if_fail (ba != NULL);
		gtk_html_write (html, handle, ba->data, ba->len);
	}
}

void
mail_html_write (GtkHTML *html, GtkHTMLStream *stream,
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
mail_text_write (GtkHTML *html, GtkHTMLStream *stream,
		 const char *format, ...)
{
	char *buf, *htmltext;
	va_list ap;

	va_start (ap, format);
	buf = g_strdup_vprintf (format, ap);
	va_end (ap);

	htmltext = e_text_to_html (buf,
				   E_TEXT_TO_HTML_CONVERT_URLS |
				   E_TEXT_TO_HTML_CONVERT_NL |
				   E_TEXT_TO_HTML_CONVERT_SPACES);
	gtk_html_write (html, stream, "<tt>", 4);
	gtk_html_write (html, stream, htmltext, strlen (htmltext));
	gtk_html_write (html, stream, "</tt>", 5);
	g_free (htmltext);
	g_free (buf);
}

void
mail_error_write (GtkHTML *html, GtkHTMLStream *stream,
		  const char *format, ...)
{
	char *buf, *htmltext;
	va_list ap;

	va_start (ap, format);
	buf = g_strdup_vprintf (format, ap);
	va_end (ap);

	htmltext = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL);
	gtk_html_write (html, stream, "<em><font color=red>", 20);
	gtk_html_write (html, stream, htmltext, strlen (htmltext));
	gtk_html_write (html, stream, "</font></em><br>", 16);
	g_free (htmltext);
	g_free (buf);
}

static void
clear_data (CamelObject *object, gpointer event_data, gpointer user_data)
{
	GData *data = user_data;

	g_datalist_clear (&data);
}

static void
redisplay (MailDisplay *md, gboolean unscroll)
{
	GtkAdjustment *adj;
	gfloat oldv = 0;

	if (!unscroll) {
		adj = e_scroll_frame_get_vadjustment (md->scroll);
		oldv = adj->value;
	}

	md->stream = gtk_html_begin (md->html);
	mail_html_write (md->html, md->stream, "%s%s", HTML_HEADER,
			 "<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">\n");

	if (md->current_message) {
		camel_object_ref (CAMEL_OBJECT (md->current_message));
		mail_format_mime_message (md->current_message, md);
	}

	mail_html_write (md->html, md->stream, "</BODY></HTML>\n");
	gtk_html_end (md->html, md->stream, GTK_HTML_STREAM_OK);
	md->stream = NULL;

	if (unscroll) {
		adj = e_scroll_frame_get_hadjustment (md->scroll);
		gtk_adjustment_set_value (adj, 0);
		e_scroll_frame_set_hadjustment (md->scroll, adj);
	} else {
		adj = e_scroll_frame_get_vadjustment (md->scroll);
		if (oldv < adj->upper) {
			gtk_adjustment_set_value (adj, oldv);
			e_scroll_frame_set_vadjustment (md->scroll, adj);
		}
	}
}

/**
 * mail_display_set_message:
 * @mail_display: the mail display object
 * @medium: the input camel medium, or %NULL
 *
 * Makes the mail_display object show the contents of the medium
 * param.
 **/
void 
mail_display_set_message (MailDisplay *md, CamelMedium *medium)
{
	/* For the moment, we deal only with CamelMimeMessage, but in
	 * the future, we should be able to deal with any medium.
	 */
	if (medium && !CAMEL_IS_MIME_MESSAGE (medium))
		return;

	/* Clean up from previous message. */
	if (md->current_message)
		camel_object_unref (CAMEL_OBJECT (md->current_message));

	md->current_message = (CamelMimeMessage*)medium;

	g_datalist_init (md->data);
	redisplay (md, TRUE);
	if (medium) {
		camel_object_hook_event (CAMEL_OBJECT (medium), "finalize",
					 clear_data, *(md->data));
	}
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
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	g_datalist_clear (mail_display->data);
	g_free (mail_display->data);

	mail_display_parent_class->destroy (object);
}

static void
mail_display_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = mail_display_destroy;
	mail_display_parent_class = gtk_type_class (PARENT_TYPE);
}

GtkWidget *
mail_display_new (void)
{
	MailDisplay *mail_display = gtk_type_new (mail_display_get_type ());
	GtkWidget *scroll, *html;

	gtk_box_set_homogeneous (GTK_BOX (mail_display), FALSE);
	gtk_widget_show (GTK_WIDGET (mail_display));

	scroll = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scroll),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (scroll), GTK_SHADOW_IN);
	gtk_box_pack_start_defaults (GTK_BOX (mail_display), GTK_WIDGET (scroll));
	gtk_widget_show (GTK_WIDGET (scroll));

	html = gtk_html_new ();
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	gtk_signal_connect (GTK_OBJECT (html), "url_requested",
			    GTK_SIGNAL_FUNC (on_url_requested),
			    mail_display);
	gtk_signal_connect (GTK_OBJECT (html), "object_requested",
			    GTK_SIGNAL_FUNC (on_object_requested),
			    mail_display);
	gtk_signal_connect (GTK_OBJECT (html), "link_clicked",
			    GTK_SIGNAL_FUNC (on_link_clicked),
			    mail_display);
	gtk_container_add (GTK_CONTAINER (scroll), html);
	gtk_widget_show (GTK_WIDGET (html));

	mail_display->scroll = E_SCROLL_FRAME (scroll);
	mail_display->html = GTK_HTML (html);
	mail_display->stream = NULL;
	mail_display->data = g_new0 (GData *, 1);
	g_datalist_init (mail_display->data);

	return GTK_WIDGET (mail_display);
}



E_MAKE_TYPE (mail_display, "MailDisplay", MailDisplay, mail_display_class_init, mail_display_init, PARENT_TYPE);
