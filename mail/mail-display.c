/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Dan Winship <danw@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *           Michael Zucchi <notzed@ximian.com>
 *           Miguel de Icaza <miguel@ximian.com>
 *           Larry Ewing <lewing@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <libgnorba/gnorba.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs.h>
#include <bonobo/bonobo-control-frame.h>
#include <bonobo/bonobo-stream-memory.h>
#include <bonobo/bonobo-ui-toolbar-icon.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-socket.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <gal/util/e-util.h>
#include <gal/util/e-unicode-i18n.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-unicode.h>
#include <gtk/gtkinvisible.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlobject.h>
#include <gtkhtml/htmltext.h>
#include <gtkhtml/htmlinterval.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <libsoup/soup-message.h>

#include "e-util/e-html-utils.h"
#include "e-util/e-mktemp.h"
#include "addressbook/backend/ebook/e-book-util.h"

#include "e-searching-tokenizer.h"
#include "folder-browser-factory.h"
#include "mail-stream-gtkhtml.h"
#include "folder-browser.h"
#include "mail-display.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-mt.h"
#include "mail.h"

#include "camel/camel-data-cache.h"

#include "art/empty.xpm"

#define d(x)

struct _MailDisplayPrivate {

	/* because we want to control resource usage, we need our own queues, etc */
	EDList fetch_active;
	EDList fetch_queue;

	/* used to try and make some sense with progress reporting */
	int fetch_total;
	int fetch_total_done;

	/* bit hackish, 'fake' an async message and processing,
	   so we can use that to get cancel and report progress */
	struct _mail_msg *fetch_msg ;
	GIOChannel *fetch_cancel_channel;
	guint fetch_cancel_watch;
};

/* max number of connections to download images */
#define FETCH_MAX_CONNECTIONS (4)

/* path to http cache in fetch_cache */
#define FETCH_HTTP_CACHE "http"

/* for asynchronously downloading remote content */
struct _remote_data {
	struct _remote_data *next;
	struct _remote_data *prev;

	MailDisplay *md;	/* not ref'd */

	SoupMessage *msg;
	char *uri;
	GtkHTML *html;
	GtkHTMLStream *stream;
	CamelStream *cstream;	/* cache stream */
	size_t length;
	size_t total;
};

static void fetch_remote(MailDisplay *md, const char *uri, GtkHTML *html, GtkHTMLStream *stream);
static void fetch_cancel(MailDisplay *md);
static void fetch_next(MailDisplay *md);
static void fetch_data(SoupMessage *req, void *data);
static void fetch_free(struct _remote_data *rd);
static void fetch_done(SoupMessage *req, void *data);

/* global http cache, relies on external evolution_dir as well */
static CamelDataCache *fetch_cache;

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *mail_display_parent_class;

struct _PixbufLoader {
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
static GHashTable *thumbnail_cache = NULL;

/* Drag & Drop types */
#define TEXT_URI_LIST_TYPE       "text/uri-list"

enum DndTargetType {
	DND_TARGET_TYPE_TEXT_URI_LIST,
	DND_TARGET_TYPE_PART_MIME_TYPE
};

static GtkTargetEntry drag_types[] = {
	{ TEXT_URI_LIST_TYPE, 0, DND_TARGET_TYPE_TEXT_URI_LIST },
	{ NULL, 0, DND_TARGET_TYPE_PART_MIME_TYPE }
};

static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

/*----------------------------------------------------------------------*
 *                        Callbacks
 *----------------------------------------------------------------------*/

static void
write_data_written(CamelMimePart *part, char *name, int done, void *data)
{
	int *ret = data;
	
	/* should we popup a dialogue to say its done too? */
	*ret = done;
}

static gboolean
write_data_to_file (CamelMimePart *part, const char *name, gboolean unique)
{
	int fd;
	int ret = FALSE;
	
	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), FALSE);
	
	fd = open (name, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd == -1 && errno == EEXIST && !unique) {
		GtkWidget *dlg;
		GtkWidget *text;
		char *msg;
		
		dlg = gnome_dialog_new (_("Overwrite file?"),
					GNOME_STOCK_BUTTON_YES, 
					GNOME_STOCK_BUTTON_NO,
					NULL);
		
		msg = g_strdup_printf (_("File `%s' already exists.\nOverwrite it?"), name);
		text = gtk_label_new (msg);
		g_free (msg);
		
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox), text, TRUE, TRUE, 4);
		gtk_window_set_policy (GTK_WINDOW (dlg), FALSE, TRUE, FALSE);
		gtk_widget_show (text);
		
		if (gnome_dialog_run_and_close (GNOME_DIALOG (dlg)) != 0)
			return FALSE;
	}
	
	if (fd != -1)
		close (fd);
	
	/* should this have progress of what its doing? */
	mail_msg_wait (mail_save_part (part, name, write_data_written, &ret));
	
	return ret;
}

static char *
make_safe_filename (const char *prefix,CamelMimePart *part)
{
	const char *name = NULL;
	char *safe, *p;
	
	if (part) {
		name = camel_mime_part_get_filename (part);
	}
		
	if (!name) {
		/* This is a filename. Translators take note. */
		name = _("attachment");
	}
	
	p = strrchr (name, '/');
	if (p)
		safe = g_strdup_printf ("%s%s", prefix, p);
	else
		safe = g_strdup_printf ("%s/%s", prefix, name);
	
	p = strrchr (safe, '/');
	if (p)
		e_filename_make_safe (p + 1);
	
	return safe;
}

static void
save_data_cb (GtkWidget *widget, gpointer user_data)
{
	GtkFileSelection *file_select = (GtkFileSelection *)
		gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_SELECTION);
	char *dir;
	
	/* uh, this doesn't really feel right, but i dont know what to do better */
	gtk_widget_hide (GTK_WIDGET (file_select));
	write_data_to_file (user_data, gtk_file_selection_get_filename (file_select),
			    FALSE);
	
	/* preserve the pathname */
	dir = g_dirname (gtk_file_selection_get_filename (file_select));
	mail_config_set_last_filesel_dir (dir);
	g_free (dir);
	
	gtk_widget_destroy (GTK_WIDGET (file_select));
}

static void
save_destroy_cb (GtkWidget *widget, CamelMimePart *part) 
{
	camel_object_unref (CAMEL_OBJECT (part));
}

static gboolean
idle_redisplay (gpointer data)
{
	MailDisplay *md = data;
	
	md->idle_id = 0;
	mail_display_redisplay (md, FALSE);
	
	return FALSE;
}

void
mail_display_queue_redisplay (MailDisplay *md)
{
	if (!md->idle_id) {
		md->idle_id = g_idle_add_full (G_PRIORITY_LOW, idle_redisplay,
					       md, NULL);
	}
}

static void
mail_display_jump_to_anchor (MailDisplay *md, const char *url)
{
	char *anchor = strstr (url, "#");
	
	g_return_if_fail (anchor != NULL);

	if (anchor)
		gtk_html_jump_to_anchor (md->html, anchor + 1);
}

static void
on_link_clicked (GtkHTML *html, const char *url, MailDisplay *md)
{
	if (!g_strncasecmp (url, "news:", 5) || !g_strncasecmp (url, "nntp:", 5)) {
		g_warning ("Can't handle news URLs yet.");
	} else if (!g_strncasecmp (url, "mailto:", 7)) {
		send_to_url (url, NULL);
	} else if (*url == '#') {
		mail_display_jump_to_anchor (md, url);
	} else {
		gnome_url_show (url);
	}
}

static void 
save_part (CamelMimePart *part)
{
	GtkFileSelection *file_select;
	char *filename;
	
	g_return_if_fail (part != NULL);
	camel_object_ref (CAMEL_OBJECT (part));
	
	filename = make_safe_filename (mail_config_get_last_filesel_dir (), part);
	
	file_select = GTK_FILE_SELECTION (
		gtk_file_selection_new (_("Save Attachment")));
	gtk_file_selection_set_filename (file_select, filename);
	/* set the GtkEntry with the locale filename by breaking abstraction */
	e_utf8_gtk_entry_set_text (GTK_ENTRY (file_select->selection_entry), g_basename (filename));
	g_free (filename);
	
	gtk_signal_connect (GTK_OBJECT (file_select->ok_button), "clicked", 
			    GTK_SIGNAL_FUNC (save_data_cb), part);
	gtk_signal_connect_object (GTK_OBJECT (file_select->cancel_button),
				   "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (file_select));
	
	gtk_signal_connect (GTK_OBJECT (file_select), "destroy",
			    GTK_SIGNAL_FUNC (save_destroy_cb), part);

	gtk_widget_show (GTK_WIDGET (file_select));
}

static void
save_cb (GtkWidget *widget, gpointer user_data)
{
	CamelMimePart *part = gtk_object_get_data (GTK_OBJECT (user_data), "CamelMimePart");
	
	save_part (part);
}

static void
launch_cb (GtkWidget *widget, gpointer user_data)
{
	CamelMimePart *part = gtk_object_get_data (user_data, "CamelMimePart");
	MailMimeHandler *handler;
	GList *apps, *children, *c;
	GnomeVFSMimeApplication *app;
	char *command, *filename;
	const char *tmpdir;
	
	handler = mail_lookup_handler (gtk_object_get_data (user_data, "mime_type"));
	g_return_if_fail (handler != NULL && handler->applications != NULL);
	
	/* Yum. Too bad EPopupMenu doesn't allow per-item closures. */
	children = gtk_container_children (GTK_CONTAINER (widget->parent));
	g_return_if_fail (children != NULL && children->next != NULL && children->next->next != NULL);
	
	for (c = children->next->next, apps = handler->applications; c && apps; c = c->next, apps = apps->next) {
		if (c->data == widget)
			break;
	}
	g_list_free (children);
	g_return_if_fail (c != NULL && apps != NULL);
	app = apps->data;
	
	tmpdir = e_mkdtemp ("evolution.XXXXXX");
	
	if (!tmpdir) {
		char *msg = g_strdup_printf (_("Could not create temporary directory: %s"),
					     g_strerror (errno));
		gnome_error_dialog (msg);
		g_free (msg);
		return;
	}
	
	filename = make_safe_filename (tmpdir, part);
	
	if (!write_data_to_file (part, filename, TRUE)) {
		char *msg = g_strdup_printf (_("Could not create temporary file '%s': %s"),
					     filename, g_strerror (errno));
		gnome_error_dialog (msg);
		g_free (filename);
		g_free (msg);
		return;
	}
	
	command = g_strdup_printf ("%s %s%s &", app->command,
				   app->expects_uris == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS ? "file://" : "",
				   filename);
	g_free (filename);
	
	system (command);
	g_free (command);
}

static void
inline_cb (GtkWidget *widget, gpointer user_data)
{
	MailDisplay *md = gtk_object_get_data (user_data, "MailDisplay");
	CamelMimePart *part = gtk_object_get_data (user_data, "CamelMimePart");

	mail_part_toggle_displayed (part, md);
	mail_display_queue_redisplay (md);
}

static void
button_press (GtkWidget *widget, CamelMimePart *part)
{
	MailDisplay *md;

	md = gtk_object_get_data (GTK_OBJECT (widget), "MailDisplay");
	if (md == NULL) {
		g_warning ("No MailDisplay on button!");
		return;
	}

	mail_part_toggle_displayed (part, md);
	mail_display_queue_redisplay (md);
}

static gboolean
pixmap_press (GtkWidget *widget, GdkEventButton *event, EScrollFrame *user_data)
{
	EPopupMenu *menu;
	EPopupMenu save_item = E_POPUP_ITEM (N_("Save Attachment..."), GTK_SIGNAL_FUNC (save_cb), 0);
	EPopupMenu view_item = E_POPUP_ITEM (N_("View Inline"), GTK_SIGNAL_FUNC (inline_cb), 2);
	EPopupMenu open_item = E_POPUP_ITEM (N_("Open in %s..."), GTK_SIGNAL_FUNC (launch_cb), 1);
	MailDisplay *md;
	CamelMimePart *part;
	MailMimeHandler *handler;
	int mask = 0, i, nitems;
	
#ifdef USE_OLD_DISPLAY_STYLE
	if (event->button != 3) {
		gtk_propagate_event (GTK_WIDGET (user_data),
				     (GdkEvent *)event);
		return TRUE;
	}
#endif
	
	if (event->button != 1 && event->button != 3) {
		gtk_propagate_event (GTK_WIDGET (user_data),
				     (GdkEvent *)event);
		return TRUE;
	}
	
	/* Stop the signal, since we don't want the button's class method to
	   mess up our popup. */
	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "button_press_event");
	
	part = gtk_object_get_data (GTK_OBJECT (widget), "CamelMimePart");
	handler = mail_lookup_handler (gtk_object_get_data (GTK_OBJECT (widget),
							    "mime_type"));
	
	if (handler && handler->applications)
		nitems = g_list_length (handler->applications) + 2;
	else
		nitems = 3;
	menu = g_new0 (EPopupMenu, nitems + 1);
	
	/* Save item */
	memcpy (&menu[0], &save_item, sizeof (menu[0]));
	menu[0].name = _(menu[0].name);

	/* Inline view item */
	memcpy (&menu[1], &view_item, sizeof (menu[1]));
	if (handler && handler->builtin) {
		md = gtk_object_get_data (GTK_OBJECT (widget), "MailDisplay");

		if (!mail_part_is_displayed_inline (part, md)) {
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
				menu[1].name = g_strdup_printf (
					_("View Inline (via %s)"), name);
			} else
				menu[1].name = g_strdup (_(menu[1].name));
		} else
			menu[1].name = g_strdup (_("Hide"));
	} else {
		menu[1].name = g_strdup (_(menu[1].name));
		mask |= 2;
	}

	/* External views */
	if (handler && handler->applications) {
		GnomeVFSMimeApplication *app;
		GList *apps;
		int i;

		apps = handler->applications;
		for (i = 2; i < nitems; i++, apps = apps->next) {
			app = apps->data;
			memcpy (&menu[i], &open_item, sizeof (menu[i]));
			menu[i].name = g_strdup_printf (_(menu[i].name), app->name);
		}
	} else {
		memcpy (&menu[2], &open_item, sizeof (menu[2]));
		menu[2].name = g_strdup_printf (_(menu[2].name),
						_("External Viewer"));
		mask |= 1;
	}

	e_popup_menu_run (menu, (GdkEvent *)event, mask, 0, widget);

	for (i = 1; i < nitems; i++)
		g_free (menu[i].name);
	g_free (menu);
	return TRUE;
}	

static GdkPixbuf *
pixbuf_for_mime_type (const char *mime_type)
{
	const char *icon_name;
	char *filename = NULL;
	GdkPixbuf *pixbuf = NULL;
	
	/* GnomeVFS changed the key from icon-filename to
	   icon_filename, so check icon_filename first and if that
	   fails, fall back to the old key name */
	if (!(icon_name = gnome_vfs_mime_get_value (mime_type, "icon_filename")))
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
				g_free (fm_icon);
				fm_icon = g_strdup_printf ("mc/%s", icon_name);
				filename = gnome_pixmap_file (fm_icon);
			}
			g_free (fm_icon);
		}
		
		if (filename) {
			pixbuf = gdk_pixbuf_new_from_file (filename);
			g_free (filename);
		}
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

static gboolean
pixbuf_uncache (gpointer key)
{
	GdkPixbuf *pixbuf;

	pixbuf = g_hash_table_lookup (thumbnail_cache, key);
	gdk_pixbuf_unref (pixbuf);
	g_hash_table_remove (thumbnail_cache, key);
	g_free (key);
	return FALSE;
}

static gint
pixbuf_gen_idle (struct _PixbufLoader *pbl)
{
	GdkPixbuf *pixbuf, *mini;
	gboolean error = FALSE;
	char tmp[4096];
	int len, width, height, ratio;
	gpointer orig_key;

	/* Get the pixbuf from the cache */
	if (g_hash_table_lookup_extended (thumbnail_cache, pbl->cid,
					  &orig_key, (gpointer *)&mini)) {
		width = gdk_pixbuf_get_width (mini);
		height = gdk_pixbuf_get_height (mini);

		bonobo_ui_toolbar_icon_set_pixbuf (
		        BONOBO_UI_TOOLBAR_ICON (pbl->pixmap), mini);
		gtk_widget_set_usize (pbl->pixmap, width, height);
		
		/* Restart the cache-cleaning timer */
		g_source_remove_by_user_data (orig_key);
		g_timeout_add (5 * 60 * 1000, pixbuf_uncache, orig_key);

		if (pbl->loader) {
			gdk_pixbuf_loader_close (pbl->loader);
			gtk_object_destroy (GTK_OBJECT (pbl->loader));
			camel_object_unref (CAMEL_OBJECT (pbl->mstream));
		}
		gtk_signal_disconnect (GTK_OBJECT (pbl->eb), pbl->destroy_id);
		g_free (pbl->type);
		g_free (pbl->cid);
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
		g_free (pbl->cid);
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

	if (error || !pbl->mstream) {
		if (pbl->type)
			pixbuf = pixbuf_for_mime_type (pbl->type);
		else
			pixbuf = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/pgp-signature-nokey.png");
	} else
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
	g_hash_table_insert (thumbnail_cache, pbl->cid, mini);
	g_timeout_add (5 * 60 * 1000, pixbuf_uncache, pbl->cid);

	gtk_signal_disconnect (GTK_OBJECT (pbl->eb), pbl->destroy_id);
	if (pbl->loader) {
		gdk_pixbuf_loader_close (pbl->loader);
		gtk_object_unref (GTK_OBJECT (pbl->loader));
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
	g_free (pbl->cid);
	g_free (pbl);
};

static GtkWidget *
get_embedded_for_component (const char *iid, MailDisplay *md)
{
	GtkWidget *embedded;
	BonoboControlFrame *control_frame;
	Bonobo_PropertyBag prop_bag;
	
	/*
	 * First try a control.
	 */
	embedded = bonobo_widget_new_control (iid, NULL);
	if (embedded == NULL) {
		/*
		 * No control, try an embeddable instead.
		 */
		embedded = bonobo_widget_new_subdoc (iid, NULL);
		if (embedded != NULL) {
			/* FIXME: as of bonobo 0.18, there's an extra
			 * client_site dereference in the BonoboWidget
			 * destruction path that we have to balance out to
			 * prevent problems.
			 */
			bonobo_object_ref (BONOBO_OBJECT(bonobo_widget_get_client_site (
				BONOBO_WIDGET (embedded))));

			return embedded;
		}
	}

	if (embedded == NULL)
		return NULL;

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (embedded));
		
	prop_bag = bonobo_control_frame_get_control_property_bag ( control_frame, NULL );
		
	if (prop_bag != CORBA_OBJECT_NIL){
		CORBA_Environment ev;
		/*
		 * Now we can take care of business. Currently, the only control
		 * that needs something passed to it through a property bag is
		 * the iTip control, and it needs only the From email address,
		 * but perhaps in the future we can generalize this section of code
		 * to pass a bunch of useful things to all embedded controls.
		 */
		const CamelInternetAddress *from;
		char *from_address;

		CORBA_exception_init (&ev);
		
		from = camel_mime_message_get_from (md->current_message);
		from_address = camel_address_encode((CamelAddress *)from);
		bonobo_property_bag_client_set_value_string (
			prop_bag, "from_address", 
			from_address, &ev);
		g_free(from_address);

		Bonobo_Unknown_unref (prop_bag, &ev);
		CORBA_exception_free (&ev);
	}

	return embedded;
}

static void *
save_url (MailDisplay *md, const char *url)
{
	GHashTable *urls;
	CamelMimePart *part;

	urls = g_datalist_get_data (md->data, "part_urls");
	g_return_val_if_fail (urls != NULL, NULL);
	
	part = g_hash_table_lookup (urls, url);
	if (part == NULL) {
		GByteArray *ba;
		
		urls = g_datalist_get_data (md->data, "data_urls");
		g_return_val_if_fail (urls != NULL, NULL);
		
		/* See if it's some piece of cached data if it is then pretend it
		 * is a mime part so that we can use the mime part saveing routines.
		 * It is gross but it keeps duplicated code to a minimum and helps
		 * out with ref counting and the like.
		 */
		ba = g_hash_table_lookup (urls, url);
		if (ba) {
			CamelStream *memstream;
			CamelDataWrapper *wrapper;
			const char *name;
			
			name = strrchr (url, '/');
			name = name ? name : url;
			
			/* we have to copy the data here since the ba may be long gone
			 * by the time the user actually saves the file
			 */
			memstream = camel_stream_mem_new_with_buffer (ba->data, ba->len);			
			wrapper = camel_data_wrapper_new ();
			camel_data_wrapper_construct_from_stream (wrapper, memstream);
			camel_object_unref (CAMEL_OBJECT (memstream));
			part = camel_mime_part_new ();
			camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
			camel_object_unref (CAMEL_OBJECT (wrapper));
			camel_mime_part_set_filename (part, name);
		}
	} else {
		camel_object_ref (CAMEL_OBJECT (part));
	}

	if (part) {
		CamelDataWrapper *data;

		g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);

		data = camel_medium_get_content_object ((CamelMedium *)part);
		if (!mail_content_loaded (data, md, TRUE, NULL, NULL, NULL)) {
			return NULL;
		}

		save_part (part);
		camel_object_unref (CAMEL_OBJECT (part));
		return NULL;
	}
	
	g_warning ("part not found");
	return NULL;
}

static void
drag_data_get_cb (GtkWidget *widget,
		  GdkDragContext *drag_context,
		  GtkSelectionData *selection_data,
		  guint info,
		  guint time,
		  gpointer user_data)
{
	CamelMimePart *part = user_data;
	const char *filename, *tmpdir;
	char *uri_list;

	switch (info) {
	case DND_TARGET_TYPE_TEXT_URI_LIST:
		/* Kludge around Nautilus requesting the same data many times */
		uri_list = gtk_object_get_data (GTK_OBJECT (widget), "uri-list");
		if (uri_list) {
			gtk_selection_data_set (selection_data, selection_data->target, 8,
						uri_list, strlen (uri_list));
			return;
		}
		
		tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX");
		if (!tmpdir) {
			char *msg;
			
			msg = g_strdup_printf (_("Could not create temporary directory: %s"),
					       g_strerror (errno));
			gnome_error_dialog (msg);
			g_free (msg);
		}
		
		filename = camel_mime_part_get_filename (part);
		if (!filename)
			filename = _("Unknown");
		
		uri_list = g_strdup_printf ("file://%s/%s", tmpdir, filename);
		
		if (!write_data_to_file (part, uri_list + 7, TRUE)) {
			g_free (uri_list);
			return;
		}
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					uri_list, strlen (uri_list));
		
		gtk_object_set_data_full (GTK_OBJECT (widget), "uri-list", uri_list, g_free);		
		break;
	case DND_TARGET_TYPE_PART_MIME_TYPE:
		if (header_content_type_is (part->content_type, "text", "*")) {
		        GByteArray *ba;

			ba = mail_format_get_data_wrapper_text ((CamelDataWrapper *)part, NULL);
			if (ba) {
				gtk_selection_data_set (selection_data, selection_data->target, 8,
							ba->data, ba->len);
				g_byte_array_free (ba, TRUE);
			}
		} else {
			CamelDataWrapper *wrapper;
			CamelStream *cstream;
			GByteArray *ba;
		
			ba = g_byte_array_new ();

			cstream = camel_stream_mem_new_with_byte_array (ba);
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
			camel_data_wrapper_write_to_stream (wrapper, cstream);
			
			gtk_selection_data_set (selection_data, selection_data->target, 8,
						ba->data, ba->len);
			
			camel_object_unref (CAMEL_OBJECT (cstream));
		}
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
drag_data_delete_cb (GtkWidget *widget,
		     GdkDragContext *drag_context,
		     gpointer user_data)
{
	char *uri_list;
	
	uri_list = gtk_object_get_data (GTK_OBJECT (widget), "uri-list");
	if (uri_list) {
		unlink (uri_list + 7);
		gtk_object_set_data (GTK_OBJECT (widget), "uri-list", NULL);
	}
}

static gboolean
do_attachment_header (GtkHTML *html, GtkHTMLEmbedded *eb,
		      CamelMimePart *part, MailDisplay *md)
{
	GtkWidget *button, *mainbox, *hbox, *arrow, *popup;
	MailMimeHandler *handler;
	struct _PixbufLoader *pbl;
	
	pbl = g_new0 (struct _PixbufLoader, 1);
	if (g_strncasecmp (eb->type, "image/", 6) == 0) {
		CamelDataWrapper *content;
		
		content = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		if (!camel_data_wrapper_is_offline (content)) {
			pbl->mstream = camel_stream_mem_new ();
			camel_data_wrapper_write_to_stream (content, pbl->mstream);
			camel_stream_reset (pbl->mstream);
		}
	}
	pbl->type = g_strdup (eb->type);
	pbl->cid = g_strdup (eb->classid + 6);
	pbl->pixmap = bonobo_ui_toolbar_icon_new ();
  	gtk_widget_set_usize (pbl->pixmap, 24, 24);
	pbl->eb = eb;
	pbl->destroy_id = gtk_signal_connect (GTK_OBJECT (eb), "destroy",
					      embeddable_destroy_cb, pbl);
	
	g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)pixbuf_gen_idle, 
			 pbl, NULL);
	
	mainbox = gtk_hbox_new (FALSE, 0);
	
	button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
	gtk_object_set_data (GTK_OBJECT (button), "MailDisplay", md);
	
	handler = mail_lookup_handler (eb->type);
	if (handler && handler->builtin)
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
				    GTK_SIGNAL_FUNC (button_press), part);
	
	/* Drag & Drop */
	drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target = header_content_type_simple(part->content_type);
	g_strdown (drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target);

	gtk_drag_source_set (button, GDK_BUTTON1_MASK,
			     drag_types, num_drag_types,
			     GDK_ACTION_COPY);
	gtk_signal_connect (GTK_OBJECT (button), "drag-data-get",
			    drag_data_get_cb, part);
	gtk_signal_connect (GTK_OBJECT (button), "drag-data-delete",
			    drag_data_delete_cb, part);
	
	g_free (drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target);
	drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target = NULL;

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
	
	if (handler && mail_part_is_displayed_inline (part, md))
		arrow = gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_DOWN);
	else
		arrow = gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_FORWARD);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), pbl->pixmap, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (button), hbox);
	
	popup = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (popup, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (popup),
			   gtk_arrow_new (GTK_ARROW_DOWN,
					  GTK_SHADOW_ETCHED_IN));
	
	gtk_object_set_data (GTK_OBJECT (popup), "MailDisplay", md);
	gtk_object_set_data (GTK_OBJECT (popup), "CamelMimePart", part);
	gtk_object_set_data_full (GTK_OBJECT (popup), "mime_type",
				  g_strdup (eb->type), (GDestroyNotify)g_free);
	
	gtk_signal_connect (GTK_OBJECT (popup), "button_press_event",
			    GTK_SIGNAL_FUNC (pixmap_press), md->scroll);
	
	gtk_box_pack_start (GTK_BOX (mainbox), button, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainbox), popup, TRUE, TRUE, 0);
	gtk_widget_show_all (mainbox);
	
	gtk_container_add (GTK_CONTAINER (eb), mainbox);
	
	return TRUE;
}

static gboolean
do_external_viewer (GtkHTML *html, GtkHTMLEmbedded *eb,
		    CamelMimePart *part, MailDisplay *md)
{
	CamelDataWrapper *wrapper;
	OAF_ServerInfo *component;
	GtkWidget *embedded;
	BonoboObjectClient *server;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	GByteArray *ba;
	CamelStream *cstream;
	BonoboStream *bstream;
	MailMimeHandler *handler;
	
	handler = mail_lookup_handler (eb->type);
	if (!handler || !handler->is_bonobo)
		return FALSE;
	
	component = gnome_vfs_mime_get_default_component (eb->type);
	if (!component)
		return FALSE;

	embedded = get_embedded_for_component (component->iid, md);
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
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
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

static gboolean
do_signature (GtkHTML *html, GtkHTMLEmbedded *eb,
	      CamelMimePart *part, MailDisplay *md)
{
	GtkWidget *button;
	struct _PixbufLoader *pbl;

	pbl = g_new0 (struct _PixbufLoader, 1);
	pbl->type = NULL;
	pbl->cid = g_strdup (eb->classid);
	pbl->pixmap = bonobo_ui_toolbar_icon_new ();
  	gtk_widget_set_usize (pbl->pixmap, 24, 24);
	pbl->eb = eb;
	pbl->destroy_id = gtk_signal_connect (GTK_OBJECT (eb), "destroy",
					      embeddable_destroy_cb, pbl);

	g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)pixbuf_gen_idle, 
			 pbl, NULL);

	button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
	gtk_object_set_data (GTK_OBJECT (button), "MailDisplay", md);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (button_press), part);
	gtk_container_add (GTK_CONTAINER (button), pbl->pixmap);
	gtk_widget_show_all (button);
	gtk_container_add (GTK_CONTAINER (eb), button);

	return TRUE;
}

static gboolean
on_object_requested (GtkHTML *html, GtkHTMLEmbedded *eb, gpointer data)
{
	MailDisplay *md = data;
	GHashTable *urls;
	CamelMimePart *part;

	if (!eb->classid)
		return FALSE;

	urls = g_datalist_get_data (md->data, "part_urls");
	if (!urls)
		return FALSE;
	
	if (!strncmp (eb->classid, "popup:", 6) && eb->type) {
		part = g_hash_table_lookup (urls, eb->classid + 6);
		if (!CAMEL_IS_MIME_PART (part))
			return FALSE;
		return do_attachment_header (html, eb, part, md);
	} else if (!strncmp (eb->classid, "signature:", 10)) {
		part = g_hash_table_lookup (urls, eb->classid);
		if (!CAMEL_IS_MIME_PART (part))
			return FALSE;
		return do_signature (html, eb, part, md);
	} else if (!strncmp (eb->classid, "cid:", 4) && eb->type) {
		part = g_hash_table_lookup (urls, eb->classid);
		if (!CAMEL_IS_MIME_PART (part))
			return FALSE;
		return do_external_viewer (html, eb, part, md);
	}

	return FALSE;
}

static void
ebook_callback (EBook *book, const gchar *addr, ECard *card, gpointer data)
{
	MailDisplay *md = data;

	if (card && md->current_message) {
		const CamelInternetAddress *from = camel_mime_message_get_from (md->current_message);
		const char *md_name = NULL, *md_addr = NULL;

		/* We are extra anal, in case we are dealing with some sort of pathological message
		   w/o a From: header. */
		if (from != NULL && camel_internet_address_get (from, 0, &md_name, &md_addr)) {
			if (md_addr != NULL && !strcmp (addr, md_addr))
				mail_display_load_images (md);
		}
	}
}

static void
on_url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle,
		  gpointer user_data)
{
	MailDisplay *md = user_data;
	GHashTable *urls;
	CamelMedium *medium;
	GByteArray *ba;
	
	urls = g_datalist_get_data (md->data, "part_urls");
	g_return_if_fail (urls != NULL);
	
	/* See if it refers to a MIME part (cid: or http:) */
	medium = g_hash_table_lookup (urls, url);
	if (medium) {
		CamelContentType *content_type;
		CamelDataWrapper *data;
		
		g_return_if_fail (CAMEL_IS_MEDIUM (medium));
		
		if (md->related)
			g_hash_table_remove(md->related, medium);
		
		data = camel_medium_get_content_object (medium);
		if (!mail_content_loaded (data, md, FALSE, url, html, handle))
			return;
		
		content_type = camel_data_wrapper_get_mime_type_field (data);
		
		if (header_content_type_is (content_type, "text", "*")) {
			ba = mail_format_get_data_wrapper_text (data, md);
			if (ba) {
				gtk_html_write (html, handle, ba->data, ba->len);
				
				g_byte_array_free (ba, TRUE);
			}
		} else {
			CamelStream *html_stream;
			
			html_stream = mail_stream_gtkhtml_new (html, handle);
			camel_data_wrapper_write_to_stream (data, html_stream);
			camel_object_unref (CAMEL_OBJECT (html_stream));
		}
		
		gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
		return;
	}
	
	urls = g_datalist_get_data (md->data, "data_urls");
	g_return_if_fail (urls != NULL);
	
	/* See if it's some piece of cached data */
	ba = g_hash_table_lookup (urls, url);
	if (ba) {
		if (ba->len) {
			gtk_html_write (html, handle, ba->data, ba->len);
			/* printf ("-- begin --\n");
			   printf (ba->data);
			   printf ("-- end --\n"); */
		}
		gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
		return;
	}
	
	/* See if it's something we can load. */
	if (strncmp (url, "http:", 5) == 0 || strncmp (url, "https:", 6) == 0) {
		if (mail_config_get_http_mode () == MAIL_CONFIG_HTTP_ALWAYS ||
		    g_datalist_get_data (md->data, "load_images")) {
			fetch_remote(md, url, html, handle);
		} else if (mail_config_get_http_mode () == MAIL_CONFIG_HTTP_SOMETIMES &&
			   !g_datalist_get_data (md->data, "checking_from")) {
			const CamelInternetAddress *from = camel_mime_message_get_from (md->current_message);
			const char *name, *addr;
			
			g_datalist_set_data (md->data, "checking_from",
					     GINT_TO_POINTER (1));
			
			/* Make sure we aren't deal w/ some sort of a pathological message w/o a From: header */
			if (from != NULL && camel_internet_address_get (from, 0, &name, &addr))
				e_book_query_address_default (addr, ebook_callback, md);
			else
				gtk_html_end (html, handle, GTK_HTML_STREAM_ERROR);
		}
	}
}

/* for processing asynchronous url fetch cancels */
static struct _mail_msg_op fetch_fake_op = {
	NULL, NULL, NULL, NULL,
};

static gboolean fetch_cancelled(GIOChannel *source, GIOCondition cond, void *data)
{
	fetch_cancel((MailDisplay *)data);

	return FALSE;
}

static void fetch_next(MailDisplay *md)
{
	struct _remote_data *rd;
	struct _MailDisplayPrivate *p = md->priv;
	SoupMessage *msg;
	SoupContext *ctx;

	/* if we're called and no more work to do, clean up, otherwise, setup */
	if (e_dlist_empty(&p->fetch_active) && e_dlist_empty(&p->fetch_queue)) {
		if (p->fetch_msg) {
			p->fetch_total = 0;
			mail_disable_stop();
			camel_operation_end(p->fetch_msg->cancel);
			camel_operation_unregister(p->fetch_msg->cancel);
			mail_msg_free(p->fetch_msg);
			p->fetch_msg = NULL;
			g_source_remove(p->fetch_cancel_watch);
			g_io_channel_unref(p->fetch_cancel_channel);
		}
	} else {
		if (p->fetch_msg == NULL) {
			p->fetch_total_done = 0;
			p->fetch_msg = mail_msg_new(&fetch_fake_op, NULL, sizeof(*p->fetch_msg));
			camel_operation_register(p->fetch_msg->cancel);
			camel_operation_start(p->fetch_msg->cancel, _("Downloading images"));
			p->fetch_cancel_channel = g_io_channel_unix_new(camel_operation_cancel_fd(p->fetch_msg->cancel));
			p->fetch_cancel_watch = g_io_add_watch(p->fetch_cancel_channel, G_IO_IN, fetch_cancelled, md);
			mail_enable_stop();
		}
	}	

	while (e_dlist_length(&p->fetch_active) < FETCH_MAX_CONNECTIONS
	       && (rd = (struct _remote_data *)e_dlist_remhead(&p->fetch_queue))) {

		ctx = soup_context_get(rd->uri);
		rd->msg = msg =  soup_message_new(ctx, SOUP_METHOD_GET);

		if (ctx)
			soup_context_unref(ctx);

		soup_message_set_flags(msg, SOUP_MESSAGE_OVERWRITE_CHUNKS);
		soup_message_add_handler(msg, SOUP_HANDLER_BODY_CHUNK, fetch_data, rd);
		e_dlist_addtail(&p->fetch_active, (EDListNode *)rd);
		soup_message_queue(msg, fetch_done, rd);
	}
}

static void fetch_remote(MailDisplay *md, const char *uri, GtkHTML *html, GtkHTMLStream *stream)
{
	struct _remote_data *rd;
	CamelStream *cstream = NULL;

	if (fetch_cache) {
		cstream = camel_data_cache_get(fetch_cache, FETCH_HTTP_CACHE, uri, NULL);
		if (cstream) {
			char buf[1024];
			ssize_t len;

			/* need to verify header? */

			while (!camel_stream_eos(cstream)) {
				len = camel_stream_read(cstream, buf, 1024);
				if (len > 0) {
					gtk_html_write(html, stream, buf, len);
				} else if (len < 0) {
					gtk_html_end(html, stream, GTK_HTML_STREAM_ERROR);
					camel_object_unref(cstream);
					return;
				}
			}
			gtk_html_end(html, stream, GTK_HTML_STREAM_OK);
			camel_object_unref(cstream);
			return;
		}
		cstream = camel_data_cache_add(fetch_cache, FETCH_HTTP_CACHE, uri, NULL);
	}

	rd = g_malloc0(sizeof(*rd));
	rd->md = md;		/* dont ref */
	rd->uri = g_strdup(uri);
	rd->html = html;
	gtk_object_ref((GtkObject *)html);
	rd->stream = stream;
	rd->cstream = cstream;

	md->priv->fetch_total++;
	e_dlist_addtail(&md->priv->fetch_queue, (EDListNode *)rd);

	fetch_next(md);
}

static void fetch_data(SoupMessage *req, void *data)
{
	struct _remote_data *rd = data, *wd;
	struct _MailDisplayPrivate *p = rd->md->priv;
	int count;
	double complete;

	/* we could just hook into the header function for this, but i'm lazy today */
	if (rd->total == 0) {
		const char *cl = soup_message_get_header(req->response_headers, "content-length");
		if (cl)
			rd->total = strtoul(cl, 0, 10);
		else
			rd->total = 0;
	}
	rd->length += req->response.length;

	gtk_html_write(rd->html, rd->stream, req->response.body, req->response.length);

	/* copy to cache, clear cache if we get a cache failure */
	if (rd->cstream) {
		if (camel_stream_write(rd->cstream, req->response.body, req->response.length) == -1) {
			camel_data_cache_remove(fetch_cache, FETCH_HTTP_CACHE, rd->uri, NULL);
			camel_object_unref(rd->cstream);
			rd->cstream = NULL;
		}
	}

	/* update based on total active + finished totals */
	complete = 0.0;
	wd = (struct _remote_data *)p->fetch_active.head;
	count = e_dlist_length(&p->fetch_active);
	while (wd->next) {
		if (wd->total)
			complete += (double)wd->length / wd->total / count;
		wd = wd->next;
	}

	d(printf("%s: %f total %f (%d,%d)\n", rd->uri, complete, (p->fetch_total_done + complete ) * 100.0 / p->fetch_total, p->fetch_total, p->fetch_total_done));

	camel_operation_progress(p->fetch_msg->cancel, (p->fetch_total_done + complete ) * 100.0 / p->fetch_total);
}

static void fetch_free(struct _remote_data *rd)
{
	gtk_object_unref((GtkObject *)rd->html);
	if (rd->cstream)
		camel_object_unref(rd->cstream);
	g_free(rd->uri);
	g_free(rd);
}

static void fetch_done(SoupMessage *req, void *data)
{
	struct _remote_data *rd = data;
	MailDisplay *md = rd->md;

	if (SOUP_MESSAGE_IS_ERROR(req)) {
		d(printf("Loading '%s' failed!\n", rd->uri));
		gtk_html_end(rd->html, rd->stream, GTK_HTML_STREAM_ERROR);
		if (fetch_cache)
			camel_data_cache_remove(fetch_cache, FETCH_HTTP_CACHE, rd->uri, NULL);
	} else {
		d(printf("Loading '%s' complete!\n", rd->uri));
		gtk_html_end(rd->html, rd->stream, GTK_HTML_STREAM_OK);
	}

	e_dlist_remove((EDListNode *)rd);
	fetch_free(rd);
	md->priv->fetch_total_done++;

	fetch_next(md);
}

static void fetch_cancel(MailDisplay *md)
{
	struct _remote_data *rd;

	/* first, clean up all the ones we haven't finished yet */
	while ((rd = (struct _remote_data *)e_dlist_remhead(&md->priv->fetch_queue))) {
		gtk_html_end(rd->html, rd->stream, GTK_HTML_STREAM_ERROR);
		if (fetch_cache)
			camel_data_cache_remove(fetch_cache, FETCH_HTTP_CACHE, rd->uri, NULL);
		fetch_free(rd);
	}

	/* cancel the rest, cancellation will free it/etc */
	while (!e_dlist_empty(&md->priv->fetch_active)) {
		rd = (struct _remote_data *)md->priv->fetch_active.head;
		soup_message_cancel(rd->msg);
	}
}

struct _load_content_msg {
	struct _mail_msg msg;
	
	MailDisplay *display;
	GtkHTML *html;
	
	GtkHTMLStream *handle;
	int redisplay_counter;
	char *url;
	CamelMimeMessage *message;
	void (*callback)(MailDisplay *, gpointer);
	gpointer data;
};

static char *
load_content_desc (struct _mail_msg *mm, int done)
{
	return g_strdup (_("Loading message content"));
}

static void
load_content_load (struct _mail_msg *mm)
{
	struct _load_content_msg *m = (struct _load_content_msg *)mm;

	m->callback (m->display, m->data);
}

static gboolean
try_part_urls (struct _load_content_msg *m)
{
	GHashTable *urls;
	CamelMedium *medium;
	
	urls = g_datalist_get_data (m->display->data, "part_urls");
	g_return_val_if_fail (urls != NULL, FALSE);
	
	/* See if it refers to a MIME part (cid: or http:) */
	medium = g_hash_table_lookup (urls, m->url);
	if (medium) {
		CamelDataWrapper *data;
		CamelStream *html_stream;
		
		g_return_val_if_fail (CAMEL_IS_MEDIUM (medium), FALSE);
		
		data = camel_medium_get_content_object (medium);
		if (!mail_content_loaded (data, m->display, FALSE, m->url, m->html, m->handle)) {
			g_warning ("This code should not be reached\n");
			return TRUE;
		}
		
		html_stream = mail_stream_gtkhtml_new (m->html, m->handle);
		camel_data_wrapper_write_to_stream (data, html_stream);
		camel_object_unref (CAMEL_OBJECT (html_stream));
		
		gtk_html_end (m->html, m->handle, GTK_HTML_STREAM_OK);
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
try_data_urls (struct _load_content_msg *m)
{
	GHashTable *urls;
	GByteArray *ba;
	
	urls = g_datalist_get_data (m->display->data, "data_urls");
	ba   = g_hash_table_lookup (urls, m->url);
	
	printf ("url: %s data: %p len: %d\n", m->url, ba, ba ? ba->len : -1);
	if (ba) {
		if (ba->len) {
			printf ("writing ...\n");
			gtk_html_write (m->html, m->handle, ba->data, ba->len);
		}
		gtk_html_end (m->html, m->handle, GTK_HTML_STREAM_OK);
		return TRUE;
	}
	
	return FALSE;
}

static void
load_content_loaded (struct _mail_msg *mm)
{
	struct _load_content_msg *m = (struct _load_content_msg *)mm;
	
	if (GTK_OBJECT_DESTROYED (m->display))
		return;
	
	if (m->display->current_message == m->message) {
		if (m->handle) {
			printf ("handle: %p orig: %d actual: %d\n", m->handle,
				m->redisplay_counter,
				m->display->redisplay_counter);
			if (m->redisplay_counter == m->display->redisplay_counter) {
				if (!try_part_urls (m) && !try_data_urls (m))
					gtk_html_end (m->html, m->handle, GTK_HTML_STREAM_ERROR);
			}
		} else
			mail_display_redisplay (m->display, FALSE);
	}
}

static void
load_content_free (struct _mail_msg *mm)
{
	struct _load_content_msg *m = (struct _load_content_msg *)mm;
	
	g_free (m->url);
	gtk_object_unref (GTK_OBJECT (m->html));
	gtk_object_unref (GTK_OBJECT (m->display));
	camel_object_unref (CAMEL_OBJECT (m->message));
}

static struct _mail_msg_op load_content_op = {
	load_content_desc,
	load_content_load,
	load_content_loaded,
	load_content_free,
};

static void
stream_write_or_redisplay_when_loaded (MailDisplay *md,
				       GtkHTML *html,
				       gconstpointer key,
				       const gchar *url,
				       void (*callback)(MailDisplay *, gpointer),
				       GtkHTMLStream *handle,
				       gpointer data)
{
	struct _load_content_msg *m;
	GHashTable *loading;
	
	if (GTK_OBJECT_DESTROYED (md))
		return;
	
	loading = g_datalist_get_data (md->data, "loading");
	if (loading) {
		if (g_hash_table_lookup (loading, key))
			return;
	} else {
		loading = g_hash_table_new (NULL, NULL);
		g_datalist_set_data_full (md->data, "loading", loading,
					  (GDestroyNotify)g_hash_table_destroy);
	}
	g_hash_table_insert (loading, (gpointer)key, GINT_TO_POINTER (1));
	
	m = mail_msg_new (&load_content_op, NULL, sizeof (*m));
	m->display = md;
	gtk_object_ref (GTK_OBJECT (m->display));
	m->html = html;
	gtk_object_ref (GTK_OBJECT (html));
	m->handle = handle;
	m->url = g_strdup (url);
	m->redisplay_counter = md->redisplay_counter;
	m->message = md->current_message;
	camel_object_ref (CAMEL_OBJECT (m->message));
	m->callback = callback;
	m->data = data;
	
	e_thread_put (mail_thread_queued, (EMsg *)m);
	return;
}

void
mail_display_stream_write_when_loaded (MailDisplay *md,
				       gconstpointer key,
				       const char *url,
				       void (*callback)(MailDisplay *, gpointer),
				       GtkHTML *html,
				       GtkHTMLStream *handle,
				       gpointer data)
{
	stream_write_or_redisplay_when_loaded (md, html, key, url, callback, handle, data);
}

void
mail_display_redisplay_when_loaded (MailDisplay *md,
				    gconstpointer key,
				    void (*callback)(MailDisplay *, gpointer),
				    GtkHTML *html,
				    gpointer data)
{
	stream_write_or_redisplay_when_loaded (md, html, key, NULL, callback, NULL, data);
}

void
mail_text_write (GtkHTML *html, GtkHTMLStream *stream, gboolean printing, const char *text)
{
	guint flags;
	char *htmltext;
	
	flags = E_TEXT_TO_HTML_CONVERT_NL | E_TEXT_TO_HTML_CONVERT_SPACES;
	
	if (!printing)
		flags |= E_TEXT_TO_HTML_CONVERT_URLS | E_TEXT_TO_HTML_CONVERT_ADDRESSES;
	
	if (mail_config_get_citation_highlight () && ! printing)
		flags |= E_TEXT_TO_HTML_MARK_CITATION;
	
	htmltext = e_text_to_html_full (text, flags, mail_config_get_citation_color ());
	
	gtk_html_write (html, stream, "<tt>", 4);
	gtk_html_write (html, stream, htmltext, strlen (htmltext));
	gtk_html_write (html, stream, "</tt>", 5);
	g_free (htmltext);
}

void
mail_error_printf (GtkHTML *html, GtkHTMLStream *stream,
		   const char *format, ...)
{
	char *buf, *htmltext;
	va_list ap;
	
	va_start (ap, format);
	buf = g_strdup_vprintf (format, ap);
	va_end (ap);
	
	htmltext = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL | E_TEXT_TO_HTML_CONVERT_URLS);
	g_free (buf);
	
	gtk_html_stream_printf (stream, "<em><font color=red>");
	gtk_html_stream_write (stream, htmltext, strlen (htmltext));
	gtk_html_stream_printf (stream, "</font></em>");
	
	g_free (htmltext);
}


#define COLOR_IS_LIGHT(r, g, b)  ((r + g + b) > (128 * 3))

void
mail_display_render (MailDisplay *md, GtkHTML *html, gboolean reset_scroll)
{
	const char *flag, *completed;
	GtkHTMLStream *stream;
	
	g_return_if_fail (IS_MAIL_DISPLAY (md));
	g_return_if_fail (GTK_IS_HTML (html));
	
	stream = gtk_html_begin (html);
	if (!reset_scroll) {
		/* This is a hack until there's a clean way to do this. */
		GTK_HTML (md->html)->engine->newPage = FALSE;
	}
	
	mail_html_write (html, stream,
			 "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n"
			 "<html>\n"
			 "<head>\n<meta name=\"generator\" content=\"Evolution Mail Component\">\n</head>\n");
	mail_html_write (html, stream, "<body marginwidth=0 marginheight=0>\n");
	
	flag = md->info ? camel_tag_get (&md->info->user_tags, "follow-up") : NULL;
	completed = md->info ? camel_tag_get (&md->info->user_tags, "completed-on") : NULL;
	if ((flag && *flag) && !(completed && *completed)) {
		const char *due_by, *overdue = "";
		char bgcolor[7], fontcolor[7];
		time_t target_date, now;
		GtkStyle *style = NULL;
		char due_date[256];
		struct tm due;
		int offset;
		
		/* my favorite thing to do... muck around with colors so we respect people's stupid themes. */
		style = gtk_widget_get_style (GTK_WIDGET (html));
		if (style && !md->printing) {
			int state = GTK_WIDGET_STATE (GTK_WIDGET (html));
			gushort r, g, b;
			
			r = style->base[state].red / 256;
			g = style->base[state].green / 256;
			b = style->base[state].blue / 256;
			
			if (COLOR_IS_LIGHT (r, g, b)) {
				r *= 1.0;
				g *= 0.97;
				b *= 0.75;
			} else {
				r = 255 - (1.0 * (255 - r));
				g = 255 - (0.97 * (255 - g));
				b = 255 - (0.75 * (255 - b));
			}
			
			sprintf (bgcolor, "%.2X%.2X%.2X", r, g, b);
			
			r = style->text[state].red / 256;
			g = style->text[state].green / 256;
			b = style->text[state].blue / 256;
			
			sprintf (fontcolor, "%.2X%.2X%.2X", r, g, b);
		} else {
			strcpy (bgcolor, "EEEEEE");
			strcpy (fontcolor, "000000");
		}
		
		due_by = camel_tag_get (&md->info->user_tags, "due-by");
		if (due_by && *due_by) {
			target_date = header_decode_date (due_by, &offset);
			now = time (NULL);
			if (now >= target_date)
				overdue = U_("Overdue:");
			
			localtime_r (&target_date, &due);
			
			e_strftime_fix_am_pm (due_date, sizeof (due_date), _("by %B %d, %Y, %l:%M %P"), &due);
		} else {
			due_date[0] = '\0';
		}
		
		gtk_html_stream_printf (stream, "<font color=\"#%s\">"
					"<table width=\"100%%\" cellpadding=0 cellspacing=0><tr><td colspan=3 height=10></td></tr>"
					"<tr><td width=10></td><td>"
					"<table cellspacing=1 cellpadding=1 bgcolor=\"#000000\" width=\"100%%\"><tr><td>"
					"<table cellspacing=0 bgcolor=\"#%s\" cellpadding=2 cellspacing=2 width=\"100%%\">"
					"<tr><td align=\"left\" width=20><img src=\"%s\" align=\"middle\"></td>"
					"<td>%s%s%s%s %s</td></table></td></tr></table>"
					"</td><td width=10></td></tr></table></font>", fontcolor, bgcolor,
					mail_display_get_url_for_icon (md, EVOLUTION_IMAGES "/flag-for-followup-16.png"),
					overdue ? "<b>" : "", overdue, overdue ? "</b>&nbsp;" : "",
					flag, due_date);
	}
	
	if (md->current_message) {
		if (md->display_style == MAIL_CONFIG_DISPLAY_SOURCE)
			mail_format_raw_message (md->current_message, md, html, stream);
		else
			mail_format_mime_message (md->current_message, md, html, stream);
	}
	
	mail_html_write (html, stream, "</body></html>\n");
	gtk_html_end (html, stream, GTK_HTML_STREAM_OK);
}

/**
 * mail_display_redisplay:
 * @mail_display: the mail display object
 * @reset_scroll: specifies whether or not to reset current scroll
 *
 * Force a redraw of the message display.
 **/
void
mail_display_redisplay (MailDisplay *md, gboolean reset_scroll)
{
	if (GTK_OBJECT_DESTROYED (md))
		return;

	/* we're in effect stealing the queued redisplay */
	if (md->idle_id) {
		g_source_remove(md->idle_id);
		md->idle_id = 0;
	}

	fetch_cancel(md);
	
	md->last_active = NULL;
	md->redisplay_counter++;
	/* printf ("md %p redisplay %d\n", md, md->redisplay_counter); */
	
	mail_display_render (md, md->html, reset_scroll);
}


/**
 * mail_display_set_message:
 * @mail_display: the mail display object
 * @medium: the input camel medium, or %NULL
 * @folder: CamelFolder
 * @info: message info
 *
 * Makes the mail_display object show the contents of the medium
 * param.
 **/
void 
mail_display_set_message (MailDisplay *md, CamelMedium *medium, CamelFolder *folder, CamelMessageInfo *info)
{
	/* For the moment, we deal only with CamelMimeMessage, but in
	 * the future, we should be able to deal with any medium.
	 */
	if (medium && !CAMEL_IS_MIME_MESSAGE (medium))
		return;
	
	/* Clean up from previous message. */
	if (md->current_message) {
		fetch_cancel (md);
		camel_object_unref (CAMEL_OBJECT (md->current_message));
		g_datalist_clear (md->data);
	}
	
	if (medium) {
		camel_object_ref (medium);
		md->current_message = (CamelMimeMessage *) medium;
	} else
		md->current_message = NULL;
	
	if (md->folder && md->info) {
		camel_folder_free_message_info (md->folder, md->info);
		camel_object_unref (md->folder);
	}
	
	if (folder && info) {
		md->info = info;
		md->folder = folder;
		camel_object_ref (folder);
		camel_folder_ref_message_info (folder, info);
	} else {
		md->info = NULL;
		md->folder = NULL;
	}
	
	g_datalist_init (md->data);
	mail_display_redisplay (md, TRUE);
}

/**
 * mail_display_set_charset:
 * @mail_display: the mail display object
 * @charset: charset or %NULL
 *
 * Makes the mail_display object show the contents of the medium
 * param.
 **/
void
mail_display_set_charset (MailDisplay *mail_display, const char *charset)
{
	g_free (mail_display->charset);
	mail_display->charset = g_strdup (charset);
	
	mail_display_queue_redisplay (mail_display);
}

/**
 * mail_display_load_images:
 * @md: the mail display object
 *
 * Load all HTTP images in the current message
 **/
void
mail_display_load_images (MailDisplay *md)
{
	g_datalist_set_data (md->data, "load_images", GINT_TO_POINTER (1));
	mail_display_redisplay (md, FALSE);
}

/*----------------------------------------------------------------------*
 *                     Standard Gtk+ Class functions
 *----------------------------------------------------------------------*/

static void
mail_display_init (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	mail_display->scroll            = NULL;
	mail_display->html              = NULL;
	mail_display->redisplay_counter = 0;
	mail_display->last_active       = NULL;
	mail_display->idle_id           = 0;
	mail_display->selection         = NULL;
	mail_display->charset           = NULL;
	mail_display->current_message   = NULL;
	mail_display->folder            = NULL;
	mail_display->info              = NULL;
	mail_display->data              = NULL;
	
	mail_display->invisible         = gtk_invisible_new ();

	mail_display->display_style     = mail_config_get_message_display_style ();

	mail_display->printing          = FALSE;

	mail_display->priv = g_malloc0(sizeof(*mail_display->priv));
	e_dlist_init(&mail_display->priv->fetch_active);
	e_dlist_init(&mail_display->priv->fetch_queue);
}

static void
mail_display_destroy (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	gtk_object_unref (GTK_OBJECT (mail_display->html));
	
	if (mail_display->current_message) {
		camel_object_unref (mail_display->current_message);
		g_datalist_clear (mail_display->data);
		fetch_cancel(mail_display);
	}
	
	g_free (mail_display->charset);
	g_free (mail_display->selection);
	
	if (mail_display->folder) {
		if (mail_display->info)
			camel_folder_free_message_info (mail_display->folder, mail_display->info);
		camel_object_unref (mail_display->folder);
	}
	
	g_free (mail_display->data);
	mail_display->data = NULL;
	
	if (mail_display->idle_id)
		gtk_timeout_remove (mail_display->idle_id);
	
	gtk_widget_unref (mail_display->invisible);

	g_free(mail_display->priv);
	
	mail_display_parent_class->destroy (object);
}

static void
invisible_selection_get_callback (GtkWidget *widget,
				  GtkSelectionData *selection_data,
				  guint info,
				  guint time,
				  void *data)
{
	MailDisplay *display;

	display = MAIL_DISPLAY (data);
	
	if (!display->selection)
		return;
	
	g_assert (info == 1);

	gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING, 8,
				display->selection, strlen (display->selection));
}

static gint
invisible_selection_clear_event_callback (GtkWidget *widget,
					  GdkEventSelection *event,
					  void *data)
{
	MailDisplay *display;

	display = MAIL_DISPLAY (data);

	g_free (display->selection);
	display->selection = NULL;

	return TRUE;
}

static void
mail_display_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = mail_display_destroy;

	if (mail_display_parent_class == NULL) {
		/* blah, this is an unecessary dependency ... */
		extern char *evolution_dir;
		char *path = alloca(strlen(evolution_dir)+16);

		sprintf(path, "%s/cache", evolution_dir);
		/* cache expiry - 2 hour access, 1 day max */
		fetch_cache = camel_data_cache_new(path, 0, NULL);
		camel_data_cache_set_expire_age(fetch_cache, 24*60*60);
		camel_data_cache_set_expire_access(fetch_cache, 2*60*60);

		mail_display_parent_class = gtk_type_class (PARENT_TYPE);
		thumbnail_cache = g_hash_table_new (g_str_hash, g_str_equal);

	}
}

static void
link_open_in_browser (GtkWidget *w, MailDisplay *mail_display)
{
	if (!mail_display->html->pointer_url)
		return;
	
	on_link_clicked (mail_display->html, mail_display->html->pointer_url,
			 mail_display);
}

#if 0
static void
link_save_as (GtkWidget *w, MailDisplay *mail_display)
{
	g_print ("FIXME save %s\n", mail_display->html->pointer_url);
}
#endif

static void
link_copy_location (GtkWidget *w, MailDisplay *mail_display)
{
	GdkAtom clipboard_atom;

	g_free (mail_display->selection);
	mail_display->selection = g_strdup (mail_display->html->pointer_url);

	clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	if (clipboard_atom == GDK_NONE)
		return; /* failed */

	/* We don't check the return values of the following since there is not
	 * much we can do if we cannot assert the selection.
	 */

	gtk_selection_owner_set (GTK_WIDGET (mail_display->invisible),
				 GDK_SELECTION_PRIMARY,
				 GDK_CURRENT_TIME);
	gtk_selection_owner_set (GTK_WIDGET (mail_display->invisible),
				 clipboard_atom,
				 GDK_CURRENT_TIME);
}

static void
image_save_as (GtkWidget *w, MailDisplay *mail_display)
{
	const char *src;

	src = gtk_object_get_data (GTK_OBJECT (mail_display), "current_src_uri");

	g_warning ("loading uri=%s", src);
	
	save_url (mail_display, src);
}

enum {
	/* 
	 * This is used to mask the link specific menu items.
	 */
	MASK_URL = 1,

	/*
	 * This is used to mask src specific menu items.
	 */
	MASK_SRC = 2
};

#define SEPARATOR  { "", NULL, (NULL), NULL,  0 }
#define TERMINATOR { NULL, NULL, (NULL), NULL,  0 }

static EPopupMenu link_menu [] = {
	E_POPUP_ITEM (N_("Open Link in Browser"), GTK_SIGNAL_FUNC (link_open_in_browser),   MASK_URL),
	E_POPUP_ITEM (N_("Copy Link Location"), GTK_SIGNAL_FUNC (link_copy_location),  MASK_URL),
#if 0
	E_POPUP_ITEM (N_("Save Link as (FIXME)"), GTK_SIGNAL_FUNC (link_save_as),  MASK_URL),
#endif
	E_POPUP_ITEM (N_("Save Image as..."), GTK_SIGNAL_FUNC (image_save_as), MASK_SRC), 
     
	TERMINATOR
};


/*
 *  Create a window and popup our widget, with reasonable semantics for the popup
 *  disappearing, etc.
 */

typedef struct _PopupInfo PopupInfo;
struct _PopupInfo {
	GtkWidget *w;
	GtkWidget *win;
	guint destroy_timeout;
	guint widget_destroy_handle;
	Bonobo_EventSource_ListenerId listener_id;
	gboolean hidden;
};

/* Aiieee!  Global Data! */
static GtkWidget *the_popup = NULL;

static void
popup_info_free (PopupInfo *pop)
{
	if (pop) {
		if (pop->destroy_timeout)
			gtk_timeout_remove (pop->destroy_timeout);

		bonobo_event_source_client_remove_listener (bonobo_widget_get_objref (BONOBO_WIDGET (pop->w)),
							    pop->listener_id,
							    NULL);

		g_free (pop);
	}
}

static void
popup_window_destroy_cb (GtkWidget *w, gpointer user_data)
{
	PopupInfo *pop = (PopupInfo *) user_data;

	the_popup = NULL;

	popup_info_free (pop);
}

static gint
popup_timeout_cb (gpointer user_data)
{
	PopupInfo *pop = (PopupInfo *) user_data;

	pop->destroy_timeout = 0;
	gtk_widget_destroy (pop->win);

	return 0;
}

static gint
popup_enter_cb (GtkWidget *w, GdkEventCrossing *ev, gpointer user_data)
{
	PopupInfo *pop = (PopupInfo *) user_data;

	if (pop->destroy_timeout)
		gtk_timeout_remove (pop->destroy_timeout);
	pop->destroy_timeout = 0;

	return 0;
}

static gint
popup_leave_cb (GtkWidget *w, GdkEventCrossing *ev, gpointer user_data)
{
	PopupInfo *pop = (PopupInfo *) user_data;

	if (pop->destroy_timeout)
		gtk_timeout_remove (pop->destroy_timeout);

	if (!pop->hidden)
		pop->destroy_timeout = gtk_timeout_add (500, popup_timeout_cb, pop);

	return 0;
}

static void
popup_realize_cb (GtkWidget *widget, gpointer user_data)
{
	PopupInfo *pop = (PopupInfo *) user_data;

	gtk_widget_add_events (pop->win, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	if (pop->destroy_timeout == 0) {
		if (!pop->hidden) {
			pop->destroy_timeout = gtk_timeout_add (5000, popup_timeout_cb, pop);
		} else {
			pop->destroy_timeout = 0;
		}
	}
}

static void
popup_size_allocate_cb (GtkWidget *widget, GtkAllocation *alloc, gpointer user_data)
{
	gint x, y, w, h, xmax, ymax;

	xmax = gdk_screen_width ();
	ymax = gdk_screen_height ();

	gdk_window_get_pointer (NULL, &x, &y, NULL);
	w = alloc->width;
	h = alloc->height;
	x = CLAMP (x - w/2, 0, xmax - w);
	y = CLAMP (y - h/2, 0, ymax - h);
	gtk_widget_set_uposition (widget, x, y);

}

static PopupInfo *
make_popup_window (GtkWidget *w)
{
	PopupInfo *pop = g_new0 (PopupInfo, 1);
	GtkWidget *fr;

	/* Only allow for one popup at a time.  Ugly. */
	if (the_popup)
		gtk_widget_destroy (the_popup);

	pop->w = w;
	the_popup = pop->win = gtk_window_new (GTK_WINDOW_POPUP);
	fr = gtk_frame_new (NULL);

	gtk_container_add (GTK_CONTAINER (pop->win), fr);
	gtk_container_add (GTK_CONTAINER (fr), w);

	gtk_window_set_policy (GTK_WINDOW (pop->win), FALSE, FALSE, FALSE);

	gtk_signal_connect (GTK_OBJECT (pop->win),
			    "destroy",
			    GTK_SIGNAL_FUNC (popup_window_destroy_cb),
			    pop);
	gtk_signal_connect (GTK_OBJECT (pop->win),
			    "enter_notify_event",
			    GTK_SIGNAL_FUNC (popup_enter_cb),
			    pop);
	gtk_signal_connect (GTK_OBJECT (pop->win),
			    "leave_notify_event",
			    GTK_SIGNAL_FUNC (popup_leave_cb),
			    pop);
	gtk_signal_connect_after (GTK_OBJECT (pop->win),
				  "realize",
				  GTK_SIGNAL_FUNC (popup_realize_cb),
				  pop);
	gtk_signal_connect (GTK_OBJECT (pop->win),
			    "size_allocate",
			    GTK_SIGNAL_FUNC (popup_size_allocate_cb),
			    pop);

	gtk_widget_show (w);
	gtk_widget_show (fr);
	gtk_widget_show (pop->win);

	return pop;
}

static void
listener_cb (BonoboListener    *listener,
	     char              *event_name,
	     CORBA_any         *any,
	     CORBA_Environment *ev,
	     gpointer           user_data)
{
	PopupInfo *pop;
	char *type;

	pop = user_data;

	if (pop->destroy_timeout)
		gtk_timeout_remove (pop->destroy_timeout);
	pop->destroy_timeout = 0;
	
	type = bonobo_event_subtype (event_name);

	if (!strcmp (type, "Destroy")) {
		gtk_widget_destroy (GTK_WIDGET (pop->win));
	} else if (!strcmp (type, "Hide")) {
		pop->hidden = TRUE;
		gtk_widget_hide (GTK_WIDGET (pop->win));
	}

	g_free (type);
}

static int
html_button_press_event (GtkWidget *widget, GdkEventButton *event, MailDisplay *mail_display)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 3) {
			HTMLEngine *e;
			HTMLPoint *point;
			GtkWidget *popup_thing;

			e     = GTK_HTML (widget)->engine;
			point = html_engine_get_point_at (e, event->x + e->x_offset, event->y + e->y_offset, FALSE);
			
			if (point) {
				const gchar *url;
				const gchar *src;

				url = html_object_get_url (point->object);
				src = html_object_get_src (point->object);

				if (url && !g_strncasecmp (url, "mailto:", 7)) {
					PopupInfo *pop;
					gchar *url_decoded;

					url_decoded = gtk_html_get_url_object_relative (GTK_HTML (widget),
											point->object,
											url);
					camel_url_decode (url_decoded);

					popup_thing = bonobo_widget_new_control ("OAFIID:GNOME_Evolution_Addressbook_AddressPopup",
										 CORBA_OBJECT_NIL);

					bonobo_widget_set_property (BONOBO_WIDGET (popup_thing),
								    "email", url_decoded+7,
								    NULL);
					g_free (url_decoded);
					
					pop = make_popup_window (popup_thing);

					pop->listener_id =
						bonobo_event_source_client_add_listener (bonobo_widget_get_objref (BONOBO_WIDGET (popup_thing)),
											 listener_cb, NULL, NULL, pop);

				} else if (url || src) {
				        gint hide_mask = 0;

					if (!url)
						hide_mask |= MASK_URL;

					if (!src)
						hide_mask |= MASK_SRC;

					g_free (gtk_object_get_data (GTK_OBJECT (mail_display), "current_src_uri"));
					gtk_object_set_data (GTK_OBJECT (mail_display), "current_src_uri", 
							     gtk_html_get_url_object_relative (GTK_HTML (widget),
											       point->object,
											       src));
					
					e_popup_menu_run (link_menu, (GdkEvent *) event, 0, hide_mask, mail_display);

				}

				html_point_destroy (point);
			}

			return TRUE;
		}
	}
		
	return FALSE;
}

static inline void
set_underline (HTMLEngine *e, HTMLObject *o, gboolean underline)
{
	HTMLText *text = HTML_TEXT (o);

	html_text_set_font_style (text, e, underline
				  ? html_text_get_font_style (text) | GTK_HTML_FONT_STYLE_UNDERLINE
				  : html_text_get_font_style (text) & ~GTK_HTML_FONT_STYLE_UNDERLINE);
	html_engine_queue_draw (e, o);
}

static void
update_active (GtkWidget *widget, gint x, gint y, MailDisplay *mail_display)
{
	HTMLEngine *e;
	HTMLPoint *point;
	const gchar *email;

	e = GTK_HTML (widget)->engine;

	point = html_engine_get_point_at (e, x + e->x_offset, y + e->y_offset, FALSE);
	if (mail_display->last_active && (!point || mail_display->last_active != point->object)) {
		set_underline (e, HTML_OBJECT (mail_display->last_active), FALSE);
		mail_display->last_active = NULL;
	}
	if (point) {
		email = (const gchar *) html_object_get_data (point->object, "email");
		if (email && html_object_is_text (point->object)) {
			set_underline (e, point->object, TRUE);
			mail_display->last_active = point->object;
		}
		html_point_destroy (point);
	}
}

static gint
html_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event, MailDisplay *mail_display)
{
	update_active (widget, event->x, event->y, mail_display);

	return TRUE;
}

static gint
html_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, MailDisplay *mail_display)
{
	gint x, y;

	g_return_val_if_fail (widget != NULL, 0);
	g_return_val_if_fail (GTK_IS_HTML (widget), 0);
	g_return_val_if_fail (event != NULL, 0);

	if (event->is_hint)
		gdk_window_get_pointer (GTK_LAYOUT (widget)->bin_window, &x, &y, NULL);
	else {
		x = event->x;
		y = event->y;
	}

	update_active (widget, x, y, mail_display);

	return TRUE;
}

static void
html_iframe_created (GtkWidget *w, GtkHTML *iframe, MailDisplay *mail_display)
{
	gtk_signal_connect (GTK_OBJECT (iframe), "button_press_event",
			    GTK_SIGNAL_FUNC (html_button_press_event), mail_display);
	gtk_signal_connect (GTK_OBJECT (iframe), "motion_notify_event",
			    GTK_SIGNAL_FUNC (html_motion_notify_event), mail_display);
	gtk_signal_connect (GTK_OBJECT (iframe), "enter_notify_event",
			    GTK_SIGNAL_FUNC (html_enter_notify_event), mail_display);
}

static GNOME_Evolution_ShellView
retrieve_shell_view_interface_from_control (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;

	control_frame = bonobo_control_get_control_frame (control);

	if (control_frame == NULL)
		return CORBA_OBJECT_NIL;

	CORBA_exception_init (&ev);
	shell_view_interface = Bonobo_Unknown_queryInterface (control_frame,
							      "IDL:GNOME/Evolution/ShellView:1.0",
							      &ev);
	CORBA_exception_free (&ev);

	if (shell_view_interface != CORBA_OBJECT_NIL)
		gtk_object_set_data (GTK_OBJECT (control),
				     "mail_threads_shell_view_interface",
				     shell_view_interface);
	else
		g_warning ("Control frame doesn't have Evolution/ShellView.");

	return shell_view_interface;
}

static void
set_status_message (const char *message, int busy)
{
	EList *controls;
	EIterator *it;
	
	controls = folder_browser_factory_get_control_list ();
	for (it = e_list_get_iterator (controls); e_iterator_is_valid (it); e_iterator_next (it)) {
		BonoboControl *control;
		GNOME_Evolution_ShellView shell_view_interface;
		CORBA_Environment ev;
		
		control = BONOBO_CONTROL (e_iterator_get (it));
		
		shell_view_interface = gtk_object_get_data (GTK_OBJECT (control), "mail_threads_shell_view_interface");
		
		if (shell_view_interface == CORBA_OBJECT_NIL)
			shell_view_interface = retrieve_shell_view_interface_from_control (control);
		
		CORBA_exception_init (&ev);
		
		if (shell_view_interface != CORBA_OBJECT_NIL) {
			if (message != NULL)
				GNOME_Evolution_ShellView_setMessage (shell_view_interface,
								      message[0] ? message: "",
								      busy,
								      &ev);
		}
		
		CORBA_exception_free (&ev);
		
		/* yeah we only set the first one.  Why?  Because it seems to leave
		   random ones lying around otherwise.  Shrug. */
		break;
	}
	gtk_object_unref (GTK_OBJECT(it));
}

/* For now show every url but possibly limit it to showing only http:
   or ftp: urls */
static void
html_on_url (GtkHTML *html,
	     const char *url,
	     MailDisplay *mail_display)
{
	static char *previous_url = NULL;

	/* This all looks silly but yes, this is the proper way to mix
           GtkHTML's on_url with BonoboUIComponent statusbar */
	if (!url || (previous_url && (strcmp (url, previous_url) != 0)))
		set_status_message ("", FALSE);
	if (url) {
		set_status_message (url, FALSE);
		g_free (previous_url);
		previous_url = g_strdup (url);
	}
}

GtkWidget *
mail_display_new (void)
{
	MailDisplay *mail_display = gtk_type_new (mail_display_get_type ());
	GtkWidget *scroll, *html;
	GdkAtom clipboard_atom;
	HTMLTokenizer *tok;
	
	gtk_box_set_homogeneous (GTK_BOX (mail_display), FALSE);
	gtk_widget_show (GTK_WIDGET (mail_display));
	
	scroll = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scroll),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (scroll), GTK_SHADOW_IN);
	gtk_box_pack_start_defaults (GTK_BOX (mail_display), GTK_WIDGET (scroll));
	gtk_widget_show (GTK_WIDGET (scroll));
	
	html = gtk_html_new ();
	tok = e_searching_tokenizer_new ();
	html_engine_set_tokenizer (GTK_HTML (html)->engine, tok);
	gtk_object_unref (GTK_OBJECT (tok));
	
	mail_display_initialize_gtkhtml (mail_display, GTK_HTML (html));
	
	gtk_container_add (GTK_CONTAINER (scroll), html);
	gtk_widget_show (GTK_WIDGET (html));
	
	gtk_signal_connect (GTK_OBJECT (mail_display->invisible), "selection_get",
			    GTK_SIGNAL_FUNC (invisible_selection_get_callback), mail_display);
	gtk_signal_connect (GTK_OBJECT (mail_display->invisible), "selection_clear_event",
			    GTK_SIGNAL_FUNC (invisible_selection_clear_event_callback), mail_display);
	
	gtk_selection_add_target (mail_display->invisible,
				  GDK_SELECTION_PRIMARY, GDK_SELECTION_TYPE_STRING, 1);
	
	clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	if (clipboard_atom != GDK_NONE)
		gtk_selection_add_target (mail_display->invisible,
					  clipboard_atom, GDK_SELECTION_TYPE_STRING, 1);
	
	mail_display->scroll = E_SCROLL_FRAME (scroll);
	mail_display->html = GTK_HTML (html);
	gtk_object_ref (GTK_OBJECT (mail_display->html));
	mail_display->last_active = NULL;
	mail_display->data = g_new0 (GData *, 1);
	g_datalist_init (mail_display->data);
	
	return GTK_WIDGET (mail_display);
}

void
mail_display_initialize_gtkhtml (MailDisplay *mail_display, GtkHTML *html)
{
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
	gtk_signal_connect (GTK_OBJECT (html), "button_press_event",
			    GTK_SIGNAL_FUNC (html_button_press_event), mail_display);
	gtk_signal_connect (GTK_OBJECT (html), "motion_notify_event",
			    GTK_SIGNAL_FUNC (html_motion_notify_event), mail_display);
	gtk_signal_connect (GTK_OBJECT (html), "enter_notify_event",
			    GTK_SIGNAL_FUNC (html_enter_notify_event), mail_display);
	gtk_signal_connect (GTK_OBJECT (html), "iframe_created",
			    GTK_SIGNAL_FUNC (html_iframe_created), mail_display);
	gtk_signal_connect (GTK_OBJECT (html), "on_url",
			    GTK_SIGNAL_FUNC (html_on_url), mail_display);
}

static void
free_url (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
	if (data)
		g_byte_array_free (value, TRUE);
}

static void
free_data_urls (gpointer urls)
{
	g_hash_table_foreach (urls, free_url, GINT_TO_POINTER (1));
	g_hash_table_destroy (urls);
}

char *
mail_display_add_url (MailDisplay *md, const char *kind, char *url, gpointer data)
{
	GHashTable *urls;
	gpointer old_key, old_value;
	
	urls = g_datalist_get_data (md->data, kind);
	if (!urls) {
		urls =  g_hash_table_new (g_str_hash, g_str_equal);
		g_datalist_set_data_full (md->data, "data_urls", urls,
					  free_data_urls);
	}
	
	if (g_hash_table_lookup_extended (urls, url, &old_key, &old_value)) {
		g_free (url);
		url = old_key;
	}
	
	g_hash_table_insert (urls, url, data);
	
	return url;
}

const char *
mail_display_get_url_for_icon (MailDisplay *md, const char *icon_name)
{
	char *icon_path, buf[1024], *url;
	int fd, nread;
	GByteArray *ba;
	
	/* FIXME: cache */
	
	if (*icon_name == '/')
		icon_path = g_strdup (icon_name);
	else {
		icon_path = gnome_pixmap_file (icon_name);
		if (!icon_path)
			return "file:///dev/null";
	}
	
	fd = open (icon_path, O_RDONLY);
	g_free (icon_path);
	if (fd == -1)
		return "file:///dev/null";
	
	ba = g_byte_array_new ();
	while (1) {
		nread = read (fd, buf, sizeof (buf));
		if (nread < 1)
			break;
		g_byte_array_append (ba, buf, nread);
	}
	close (fd);
	
	url = g_strdup_printf ("x-evolution-data:%p", ba);
	
	return mail_display_add_url (md, "data_urls", url, ba);
}


struct _location_url_stack {
	struct _location_url_stack *parent;
	CamelURL *url;
};

void
mail_display_push_content_location (MailDisplay *md, const char *location)
{
	struct _location_url_stack *node;
	CamelURL *url;
	
	url = camel_url_new (location, NULL);
	node = g_new (struct _location_url_stack, 1);
	node->parent = md->urls;
	node->url = url;
	md->urls = node;
}

CamelURL *
mail_display_get_content_location (MailDisplay *md)
{
	return md->urls ? md->urls->url : NULL;
}

void
mail_display_pop_content_location (MailDisplay *md)
{
	struct _location_url_stack *node;
	
	if (!md->urls) {
		g_warning ("content-location stack underflow!");
		return;
	}
	
	node = md->urls;
	md->urls = node->parent;
	
	if (node->url)
		camel_url_free (node->url);
	
	g_free (node);
}

E_MAKE_TYPE (mail_display, "MailDisplay", MailDisplay, mail_display_class_init, mail_display_init, PARENT_TYPE);
