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
#include "e-util/e-html-utils.h"
#include <gal/util/e-util.h>
#include <gal/widgets/e-popup-menu.h>
#include "mail-display.h"
#include "mail-config.h"
#include "mail.h"
#include "art/empty.xpm"

#include <bonobo.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-stream-memory.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <bonobo/bonobo-ui-toolbar-icon.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <gtkhtml/gtkhtml-embedded.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *mail_display_parent_class;

static void redisplay (MailDisplay *md, gboolean unscroll);

struct _PixbufLoader {
	MailDisplay *md;
	CamelDataWrapper *wrapper; /* The data */
	CamelStream *mstream;
	GdkPixbufLoader *loader; 
	GtkHTMLEmbedded *eb;
	char *type; /* Type of data, in case the conversion fails */
	char *cid; /* Strdupped on creation, but not freed until 
		      the hashtable is destroyed */
	GtkWidget *pixmap;
	guint32 destroy_id;
};

/*----------------------------------------------------------------------*
 *                        Callbacks
 *----------------------------------------------------------------------*/

static gboolean
write_data_to_file (CamelMimePart *part, const char *name, gboolean unique)
{
	CamelMimeFilterCharset *charsetfilter;
	CamelContentType *content_type;
	CamelStreamFilter *filtered_stream;
	CamelStream *stream_fs;
	CamelDataWrapper *data;
	const char *charset;
	int fd;
	int ret = FALSE;
	
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
		gtk_window_set_policy(GTK_WINDOW(dlg), FALSE, TRUE, FALSE);
		gtk_widget_show (text);

		if (gnome_dialog_run_and_close (GNOME_DIALOG (dlg)) != 0)
			return FALSE;

		fd = open (name, O_WRONLY | O_TRUNC);
	}

	if (fd == -1
	    || (stream_fs = camel_stream_fs_new_with_fd (fd)) == NULL) {
		char *msg;

		msg = g_strdup_printf (_("Could not open file %s:\n%s"),
				       name, strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);
		return FALSE;
	}
	

	/* we only convert text/ parts, and we only convert if we have to
	   null charset param == us-ascii == utf8 always, and utf8 == utf8 obviously */
	/* this will also let "us-ascii that isn't really" parts pass out in
	   proper format, without us trying to treat it as what it isn't, which is
	   the same algorithm camel uses */
	
	content_type = camel_mime_part_get_content_type (part);
	if (header_content_type_is(content_type, "text", "*")
	    && (charset = header_content_type_param (content_type, "charset"))
	    && strcasecmp(charset, "utf-8") != 0) {
		charsetfilter = camel_mime_filter_charset_new_convert ("utf-8", charset);
		filtered_stream = camel_stream_filter_new_with_stream (stream_fs);
		camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (charsetfilter));
		camel_object_unref (CAMEL_OBJECT (charsetfilter));
	} else {
		/* no we can't use a CAMEL_BLAH() cast here, since its not true, HOWEVER
		   we only treat it as a normal stream from here on, so it is OK */
		filtered_stream = (CamelStreamFilter *)stream_fs;
		camel_object_ref (CAMEL_OBJECT (stream_fs));
	}
	
	if (camel_data_wrapper_write_to_stream (data, CAMEL_STREAM (filtered_stream)) == -1
	    || camel_stream_flush (CAMEL_STREAM (filtered_stream)) == -1) {
		char *msg;
		
		msg = g_strdup_printf (_("Could not write data: %s"),
				       strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);
		ret = FALSE;
	} else {
		ret = TRUE;
	}

	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream_fs));

	return ret;
}

static char *
make_safe_filename (const char *prefix, CamelMimePart *part)
{
	const char *name = NULL;
	char *safe, *p;

	name = camel_mime_part_get_filename (part);
	if (!name) {
		/* This is a filename. Translators take note. */
		name = _("attachment");
	}

	p = strrchr (name, '/');
	if (p)
		safe = g_strdup_printf ("%s%s", prefix, p);
	else
		safe = g_strdup_printf ("%s/%s", prefix, name);
	
	p = strrchr (safe, '/') + 1;
	if (p)
		e_filename_make_safe (p);
	
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
		gtk_file_selection_new (_("Save Attachment")));
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
	CamelContentType *content_type;
	char *mime_type, *tmpl, *tmpdir, *filename, *argv[2];

	content_type = camel_mime_part_get_content_type (part);
	mime_type = header_content_type_simple (content_type);
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
		char *msg = g_strdup_printf (_("Could not create temporary "
					       "directory: %s"),
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
pixmap_press (GtkWidget *ebox, GdkEventButton *event, EScrollFrame *user_data)
{
	EPopupMenu menu[] = {
		{ N_("Save to Disk..."), NULL,
		  GTK_SIGNAL_FUNC (save_cb), NULL, 0 },
		{ N_("Open in %s..."), NULL,
		  GTK_SIGNAL_FUNC (launch_cb), NULL, 1 },
		{ N_("View Inline"), NULL,
		  GTK_SIGNAL_FUNC (inline_cb), NULL, 2 },
		{ NULL, NULL, NULL, 0 }
	};
	CamelMimePart *part;
	MailMimeHandler *handler;
	int mask = 0;

	if (event->button != 3) {
		gtk_propagate_event (GTK_WIDGET (user_data),
				     (GdkEvent *)event);
		return TRUE;
	}

	part = gtk_object_get_data (GTK_OBJECT (ebox), "CamelMimePart");
	handler = mail_lookup_handler (gtk_object_get_data (GTK_OBJECT (ebox),
							    "mime_type"));

	/* Save item */
	menu[0].name = _(menu[0].name);

	/* External view item */
	if (handler && handler->application) {
		menu[1].name = g_strdup_printf (_(menu[1].name),
						handler->application->name);
	} else {
		menu[1].name = g_strdup_printf (_(menu[1].name),
						_("External Viewer"));
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
					_("View Inline (via %s)"), name);
			} else
				menu[2].name = g_strdup (_(menu[2].name));
		} else
			menu[2].name = g_strdup (_("Hide"));
	} else {
		menu[2].name = g_strdup (_(menu[2].name));
		mask |= 2;
	}

	e_popup_menu_run (menu, event, mask, 0, ebox);
	g_free (menu[1].name);
	g_free (menu[2].name);
	return TRUE;
}	

static GdkPixbuf *
pixbuf_for_mime_type (const char *mime_type)
{
	const char *icon_name;
	char *filename = NULL;
	GdkPixbuf *pixbuf = NULL;

	icon_name = gnome_vfs_mime_get_value (mime_type, "icon-filename");
	if (icon_name) {
		if (*icon_name == '/') {
			pixbuf = gdk_pixbuf_new_from_file (icon_name);
			if (pixbuf)
				return pixbuf;
		}

		filename = gnome_pixmap_file (icon_name);
		if (!filename) {
			char *fm_icon;

			fm_icon = g_strdup_printf ("nautilus/%s", icon_name);
			filename = gnome_pixmap_file (fm_icon);
			if (!filename) {
				fm_icon = g_strdup_printf ("mc/%s", icon_name);
				filename = gnome_pixmap_file (fm_icon);
			}
			g_free (fm_icon);
		}
	}
	
	if (filename) {
		pixbuf = gdk_pixbuf_new_from_file (filename);
		g_free (filename);
	}

	if (!pixbuf) {
		filename = gnome_pixmap_file ("gnome-unknown.png");
		if (filename) {
			pixbuf = gdk_pixbuf_new_from_file (filename);
			g_free (filename);
		} else {
			g_warning ("Could not get any icon for %s!",mime_type);
			pixbuf = gdk_pixbuf_new_from_xpm_data (
				(const char **)empty_xpm);
		}
	}

	return pixbuf;
}

static gint
pixbuf_gen_idle (struct _PixbufLoader *pbl)
{
	GdkPixbuf *pixbuf, *mini;
	gboolean error = FALSE;
	char tmp[4096];
	int len, width, height, ratio;

	/* Get the pixbuf from the cache */
	mini = g_hash_table_lookup (pbl->md->thumbnail_cache, pbl->cid);
	if (mini) {
		width = gdk_pixbuf_get_width (mini);
		height = gdk_pixbuf_get_height (mini);

		bonobo_ui_toolbar_icon_set_pixbuf (
		        BONOBO_UI_TOOLBAR_ICON (pbl->pixmap), mini);
		gtk_widget_set_usize (pbl->pixmap, width, height);
		
		if (pbl->loader) {
			gdk_pixbuf_loader_close (pbl->loader);
			gtk_object_destroy (GTK_OBJECT (pbl->loader));
			camel_object_unref (CAMEL_OBJECT (pbl->mstream));
		}
		gtk_signal_disconnect (GTK_OBJECT (pbl->eb), pbl->destroy_id);
		g_free (pbl->type);
		g_free (pbl);

		return FALSE;
	}

	/* Not in cache, so get a pixbuf from the wrapper */
	if (!GTK_IS_WIDGET (pbl->pixmap)) {
		/* Widget has died */
		if (pbl->mstream)
			camel_object_unref (CAMEL_OBJECT (pbl->mstream));

		if (pbl->loader) {
			gdk_pixbuf_loader_close (pbl->loader);
			gtk_object_destroy (GTK_OBJECT (pbl->loader));
		}
	
		g_free (pbl->type);
		g_free (pbl);
		return FALSE;
	}

	if (pbl->mstream) {
		if (pbl->loader == NULL)
			pbl->loader = gdk_pixbuf_loader_new ();

		len = camel_stream_read (pbl->mstream, tmp, 4096);
		if (len > 0) {
			error = !gdk_pixbuf_loader_write (pbl->loader, tmp, len);
			if (!error)
				return TRUE;
		} else if (!camel_stream_eos (pbl->mstream))
			error = TRUE;
	}

	if (error || !pbl->mstream)
		pixbuf = pixbuf_for_mime_type (pbl->type);
	else
		pixbuf = gdk_pixbuf_loader_get_pixbuf (pbl->loader);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	
	if (width >= height) {
		if (width > 24) {
			ratio = width / 24;
			width = 24;
			height /= ratio;
		}
	} else {
		if (height > 24) {
			ratio = height / 24;
			height = 24;
			width /= ratio;
		}
	}

	mini = gdk_pixbuf_scale_simple (pixbuf, width, height,
					GDK_INTERP_BILINEAR);
	if (error || !pbl->mstream)
		gdk_pixbuf_unref (pixbuf);
	bonobo_ui_toolbar_icon_set_pixbuf (
		BONOBO_UI_TOOLBAR_ICON (pbl->pixmap), mini);

	/* Add the pixbuf to the cache */

	g_hash_table_insert (pbl->md->thumbnail_cache, pbl->cid, mini);
	gtk_widget_set_usize (pbl->pixmap, width, height);

	gtk_signal_disconnect (GTK_OBJECT (pbl->eb), pbl->destroy_id);
	if (pbl->loader) {
		gdk_pixbuf_loader_close (pbl->loader);
		gtk_object_destroy (GTK_OBJECT (pbl->loader));
		camel_object_unref (CAMEL_OBJECT (pbl->mstream));
	}
	g_free (pbl->type);
	g_free (pbl);
	return FALSE;
}

/* Stop the idle function and free the pbl structure
   as the widget that the pixbuf was to be rendered to
   has died on us. */
static void
embeddable_destroy_cb (GtkObject *embeddable,
		       struct _PixbufLoader *pbl)
{
	g_idle_remove_by_data (pbl);
	if (pbl->mstream)
		camel_object_unref (CAMEL_OBJECT (pbl->mstream));
	
	if (pbl->loader) {
		gdk_pixbuf_loader_close (pbl->loader);
		gtk_object_destroy (GTK_OBJECT (pbl->loader));
	}
	
	g_free (pbl->type);
	g_free (pbl);
};

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
	BonoboControlFrame *control_frame;
	Bonobo_PropertyBag prop_bag;
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
		GtkWidget *ebox;
		struct _PixbufLoader *pbl;

		pbl = g_new0 (struct _PixbufLoader, 1);
		if (g_strncasecmp (eb->type, "image/", 6) == 0) {
			pbl->mstream = camel_stream_mem_new ();
			camel_data_wrapper_write_to_stream (
				camel_medium_get_content_object (medium),
				pbl->mstream);
			camel_stream_reset (pbl->mstream);
		}
		pbl->type = g_strdup (eb->type);
		pbl->cid = g_strdup (cid);
		pbl->pixmap = bonobo_ui_toolbar_icon_new ();
		pbl->eb = eb;
		pbl->md = md;
		pbl->destroy_id = gtk_signal_connect (GTK_OBJECT (eb),
						      "destroy",
						      embeddable_destroy_cb,
						      pbl);

		g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)pixbuf_gen_idle, 
				 pbl, NULL);

		ebox = gtk_event_box_new ();
		gtk_widget_set_sensitive (GTK_WIDGET (ebox), TRUE);
		gtk_widget_add_events (GTK_WIDGET (ebox),
				       GDK_BUTTON_PRESS_MASK);
		gtk_object_set_data (GTK_OBJECT (ebox), "MailDisplay", md);
		gtk_object_set_data (GTK_OBJECT (ebox), "CamelMimePart",
				     medium);
		gtk_object_set_data_full (GTK_OBJECT (ebox), "mime_type",
					  g_strdup (eb->type),
					  (GDestroyNotify)g_free);

		gtk_signal_connect (GTK_OBJECT (ebox), "button_press_event",
				    GTK_SIGNAL_FUNC (pixmap_press), md->scroll);

		gtk_container_add (GTK_CONTAINER (ebox), pbl->pixmap);
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
	} else {
		embedded = bonobo_widget_new_control (component->iid, NULL);
		if (!embedded) {
			CORBA_free (component);
			return FALSE;
		}

		control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (embedded));
		if (control_frame) {
			prop_bag = bonobo_control_frame_get_control_property_bag ( control_frame, NULL );
			
			if (prop_bag != CORBA_OBJECT_NIL) {
				/* Now we can take care of business. Currently, the only control
				   that needs something passed to it through a property bag is
				   the iTip control, and it needs only the From email address,
				   but perhaps in the future we can generalize this section of code
				   to pass a bunch of useful things to all embedded controls. */
				const CamelInternetAddress *from;
				char *from_address;
				MailConfigIdentity *id;

				id = mail_config_get_default_identity ();
				if (!id)
					g_warning ("No identity configured!");

				CORBA_exception_init (&ev);

				from = camel_mime_message_get_from (md->current_message);
				from_address = camel_address_encode((CamelAddress *)from);
				bonobo_property_bag_client_set_value_string (prop_bag, "from_address", 
									     from_address, &ev);
				bonobo_property_bag_client_set_value_string (prop_bag, "my_address", 
									     id ? id->address : "", &ev);
				g_free(from_address);
				CORBA_exception_free (&ev);
			}
		}
	}  /* end else (we're going to use a control) */
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
  	md->stream = gtk_html_begin (GTK_HTML (md->html));
	mail_html_write (md->html, md->stream, "%s%s", HTML_HEADER, "<BODY>\n");

	if (md->current_message) {
		camel_object_ref (CAMEL_OBJECT (md->current_message));
		if (mail_config_get_view_source ())
			mail_format_raw_message (md->current_message, md);
		else
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
 * mail_display_redisplay:
 * @mail_display: the mail display object
 * @unscroll: specifies whether or not to lose current scroll
 *
 * Force a redraw of the message display.
 **/
void
mail_display_redisplay (MailDisplay *mail_display, gboolean unscroll)
{
	redisplay (mail_display, unscroll);
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
	mail_display->thumbnail_cache = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
thumbnail_cache_free (gpointer key,
		      gpointer value,
		      gpointer user_data)
{
	g_free (key);
	gdk_pixbuf_unref (value);
}

static void
mail_display_destroy (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	g_hash_table_foreach (mail_display->thumbnail_cache, 
			      thumbnail_cache_free, NULL);
	g_hash_table_destroy (mail_display->thumbnail_cache);
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
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (scroll), GTK_SHADOW_IN);
	gtk_box_pack_start_defaults (GTK_BOX (mail_display), GTK_WIDGET (scroll));
	gtk_widget_show (GTK_WIDGET (scroll));

	html = gtk_html_new ();
	gtk_html_set_default_content_type (GTK_HTML (html),
					   "text/html; charset=utf-8");

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
