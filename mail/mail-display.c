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

#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include <gtk/gtkinvisible.h>
#include <libgnome/gnome-program.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/gnome-url.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-control-frame.h>
#include <bonobo/bonobo-stream-memory.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-socket.h>

#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-popup-menu.h>

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlobject.h>
#include <gtkhtml/htmltext.h>
#include <gtkhtml/htmlinterval.h>
#include <gtkhtml/gtkhtml-stream.h>

#include <libsoup/soup-message.h>

#include "e-util/e-gui-utils.h"
#include "e-util/e-mktemp.h"
#include "addressbook/backend/ebook/e-book-util.h"

#include "e-searching-tokenizer.h"
#include "folder-browser-factory.h"
#include "mail-display-stream.h"
#include "folder-browser.h"
#include "mail-config.h"
#include "mail-display.h"
#include "mail-format.h"
#include "mail-ops.h"
#include "mail-mt.h"
#include "mail.h"

#include "camel/camel-data-cache.h"

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

	guint display_notify_id;
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
	int fd, ret = FALSE;
	
	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), FALSE);
	
	fd = open (name, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd == -1 && errno == EEXIST && !unique) {
		GtkWidget *dialog;
		int button;
		
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
						 _("File `%s' already exists.\nOverwrite it?"),
						 name);
		
		g_object_set (dialog, "title", _("Overwrite file?"), "allow_grow", TRUE, NULL);
		button = gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		if (button != GTK_RESPONSE_YES)
			return FALSE;
	}
	
	if (fd != -1)
		close (fd);
	
	/* should this have progress of what its doing? */
	mail_msg_wait (mail_save_part (part, name, write_data_written, &ret));
	
	return ret;
}

static char *
make_safe_filename (const char *prefix, CamelMimePart *part)
{
	const char *name;
	char *safe, *p;
	
	name = part ? camel_mime_part_get_filename (part) : NULL;
	
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
	GtkFileSelection *file_select;
	GConfClient *gconf;
	char *dir;
	
	file_select = (GtkFileSelection *) gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_SELECTION);
	
	/* uh, this doesn't really feel right, but i dont know what to do better */
	gtk_widget_hide (GTK_WIDGET (file_select));
	write_data_to_file (user_data, gtk_file_selection_get_filename (file_select), FALSE);
	
	/* preserve the pathname */
	dir = g_path_get_dirname (gtk_file_selection_get_filename (file_select));
	gconf = mail_config_get_gconf_client ();
	gconf_client_set_string (gconf, "/apps/evolution/mail/save_dir", dir, NULL);
	g_free (dir);
	
	gtk_widget_destroy (GTK_WIDGET (file_select));
}

static void
save_destroy_cb (CamelMimePart *part, GObject *deadbeef) 
{
	camel_object_unref (part);
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
	if (!strncasecmp (url, "mailto:", 7)) {
		send_to_url (url, NULL);
	} else if (*url == '#') {
		mail_display_jump_to_anchor (md, url);
	} else {
		GError *err = NULL;
		
		gnome_url_show (url, &err);
		
		if (err) {
			g_warning ("gnome_url_show: %s", err->message);
			g_error_free (err);
		}
	}
}

static void 
save_part (CamelMimePart *part)
{
	char *filename, *dir, *home, *base;
	GtkFileSelection *file_select;
	GConfClient *gconf;
	
	camel_object_ref (part);
	
	home = getenv ("HOME");
	gconf = mail_config_get_gconf_client ();
	dir = gconf_client_get_string (gconf, "/apps/evolution/mail/save_dir", NULL);
	filename = make_safe_filename (dir ? dir : (home ? home : ""), part);
	g_free (dir);
	
	file_select = GTK_FILE_SELECTION (gtk_file_selection_new (_("Save Attachment")));
	gtk_file_selection_set_filename (file_select, filename);
	/* set the GtkEntry with the locale filename by breaking abstraction */
	base = g_path_get_basename (filename);
	gtk_entry_set_text (GTK_ENTRY (file_select->selection_entry), base);
	g_free (filename);
	g_free (base);
	
	g_signal_connect (file_select->ok_button, "clicked", 
			  G_CALLBACK (save_data_cb), part);
	
	g_signal_connect_swapped (file_select->cancel_button, "clicked",
				  G_CALLBACK (gtk_widget_destroy), file_select);
	
	g_object_weak_ref ((GObject *) file_select, (GWeakNotify) save_destroy_cb, part);
	
	gtk_widget_show (GTK_WIDGET (file_select));
}

static void
save_cb (GtkWidget *widget, gpointer user_data)
{
	CamelMimePart *part = g_object_get_data ((GObject *) user_data, "CamelMimePart");
	
	save_part (part);
}

static void
launch_cb (GtkWidget *widget, gpointer user_data)
{
	CamelMimePart *part = g_object_get_data(user_data, "CamelMimePart");
	MailMimeHandler *handler;
	GList *apps, *children, *c;
	GnomeVFSMimeApplication *app;
	char *command, *filename;
	const char *tmpdir;
	
	handler = mail_lookup_handler (g_object_get_data(user_data, "mime_type"));
	g_return_if_fail (handler != NULL && handler->applications != NULL);
	
	/* Yum. Too bad EPopupMenu doesn't allow per-item closures. */
	children = gtk_container_get_children (GTK_CONTAINER (widget->parent));
	/* We need to bypass the first 2 menu items */
	g_return_if_fail (children != NULL && children->next != NULL 
		&& children->next->next != NULL && children->next->next->next != NULL);

	for (c = children->next->next->next, apps = handler->applications; c && apps; c = c->next, apps = apps->next) {
		if (c->data == widget)
			break;
	}
	g_list_free (children);
	g_return_if_fail (c != NULL && apps != NULL);
	app = apps->data;
	
	tmpdir = e_mkdtemp ("app-launcher-XXXXXX");
	
	if (!tmpdir) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_RESPONSE_CLOSE,
						 _("Could not create temporary directory: %s"),
						 g_strerror (errno));
		
		/* FIXME: this should be async */
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		return;
	}
	
	filename = make_safe_filename (tmpdir, part);
	
	if (!write_data_to_file (part, filename, TRUE)) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_RESPONSE_CLOSE,
						 _("Could not create temporary file '%s': %s"),
						 filename, g_strerror (errno));
		
		/* FIXME: this should be async */
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		g_free (filename);
		return;
	}
	
	command = g_strdup_printf ("%s %s%s &", app->command,
				   app->expects_uris == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS ?
				   "file://" : "", filename);
	g_free (filename);
	
	system (command);
	g_free (command);
}

static void
inline_cb (GtkWidget *widget, gpointer user_data)
{
	MailDisplay *md = g_object_get_data (user_data, "MailDisplay");
	CamelMimePart *part = g_object_get_data (user_data, "CamelMimePart");
	
	mail_part_toggle_displayed (part, md);
	mail_display_queue_redisplay (md);
}

static void
save_all_parts_cb (GtkWidget *widget, gpointer user_data)
{
	GtkFileSelection *dir_select = (GtkFileSelection *) 
		gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_SELECTION);
	const char *filename;
	char *save_filename, *dir;
	struct stat st;
	int i;
	GPtrArray *attachment_array;
	CamelMimePart *part;
	GConfClient *gconf;

	gtk_widget_hide (GTK_WIDGET (dir_select));

	/* Get the selected directory name */
	filename = gtk_file_selection_get_filename (dir_select);
	if (stat (filename, &st) == -1 || !S_ISDIR (st.st_mode)) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			_("%s is not a valid directory name."), filename);

		/* FIXME: this should be async */
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		gtk_widget_destroy (GTK_WIDGET (dir_select));
		return;
	} else {
		dir = g_strdup (filename);
	}
						
	/* Now save the attachment one by one */
	attachment_array = (GPtrArray *)user_data;
	for (i = 0; i < attachment_array->len; i++) {
		part = g_ptr_array_index (attachment_array, i);
		save_filename = make_safe_filename (dir, part);
		write_data_to_file (part, save_filename, FALSE);
		g_free (save_filename);
	}
							
	/* preserve the pathname */
	gconf = mail_config_get_gconf_client ();
	gconf_client_set_string (gconf, "/apps/evolution/mail/save_dir", dir, NULL);
	g_free (dir);
							
	gtk_widget_destroy (GTK_WIDGET (dir_select));
}

static void 
save_all_parts (GPtrArray *attachment_array)
{
	GtkFileSelection *dir_select;
	char *dir, *home, *dir2;
	GConfClient *gconf;

	g_return_if_fail (attachment_array !=  NULL);

	home = getenv ("HOME");
	gconf = mail_config_get_gconf_client ();
	dir = gconf_client_get_string (gconf, "/apps/evolution/mail/save_dir", NULL);
	dir = dir ? dir : (home ? g_strdup (home) : g_strdup (""));

	/* Make sure dir2 has a '/' as its tail */
	dir2 = g_strdup_printf ("%s/", dir);
	g_free (dir);

	dir_select = GTK_FILE_SELECTION (
		gtk_file_selection_new (_("Select Directory for Attachments")));
	gtk_file_selection_set_filename (dir_select, dir2);
	gtk_widget_set_sensitive (dir_select->file_list, FALSE);
	gtk_widget_hide (dir_select->selection_entry);
	g_free (dir2);

	g_signal_connect (dir_select->ok_button, "clicked", 
		G_CALLBACK (save_all_parts_cb), attachment_array);
	g_signal_connect_swapped (dir_select->cancel_button,
		"clicked",
		G_CALLBACK (gtk_widget_destroy),
		dir_select);

	gtk_widget_show (GTK_WIDGET (dir_select));
}
		
static void
save_all_cb (GtkWidget *widget, gpointer user_data)
{
	MailDisplay *md = g_object_get_data (user_data, "MailDisplay");
	GPtrArray *attachment_array;
			
	if (md == NULL) {
		g_warning ("No MailDisplay!");
		return;
	}

	attachment_array = g_datalist_get_data (md->data, "attachment_array");
	save_all_parts (attachment_array);
}


static gboolean 
button_press (GtkWidget *widget, GdkEvent *event, CamelMimePart *part)
{
	MailDisplay *md;

	if (event->type == GDK_BUTTON_PRESS)
		g_signal_stop_emission_by_name (widget, "button_press_event");
	else if (event->type == GDK_KEY_PRESS && event->key.keyval != GDK_Return)
		return FALSE;

	md = g_object_get_data ((GObject *) widget, "MailDisplay");
	if (md == NULL) {
		g_warning ("No MailDisplay on button!");
		return TRUE;
	}
	
	mail_part_toggle_displayed (part, md);
	mail_display_queue_redisplay (md);

	return TRUE;
}

static void
popup_menu_placement_callback(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
	GtkWidget *widget = (GtkWidget*) user_data;

	gdk_window_get_origin (gtk_widget_get_parent_window (widget), x, y);
	*x += widget->allocation.x + widget->allocation.width;
	*y += widget->allocation.y;

	return;
}

static gboolean
pixmap_press (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	EPopupMenu *menu;
	GtkMenu *gtk_menu;
	EPopupMenu save_item = E_POPUP_ITEM (N_("Save Attachment..."), G_CALLBACK (save_cb), 0);
	EPopupMenu save_all_item = E_POPUP_ITEM (N_("Save all attachments..."), G_CALLBACK (save_all_cb), 0);
	EPopupMenu view_item = E_POPUP_ITEM (N_("View Inline"), G_CALLBACK (inline_cb), 2);
	EPopupMenu open_item = E_POPUP_ITEM (N_("Open in %s..."), G_CALLBACK (launch_cb), 1);
	MailDisplay *md;
	CamelMimePart *part;
	MailMimeHandler *handler;
	int mask = 0, i, nitems;
	int current_item = 0;
	
	if (event->type == GDK_BUTTON_PRESS) {
#ifdef USE_OLD_DISPLAY_STYLE
		if (event->button.button != 3) {
			gtk_propagate_event (GTK_WIDGET (user_data),
					     (GdkEvent *)event);
			return TRUE;
		}
#endif
		
		if (event->button.button != 1 && event->button.button != 3) {
			gtk_propagate_event (GTK_WIDGET (user_data),
					     (GdkEvent *)event);
			return TRUE;
		}
		/* Stop the signal, since we don't want the button's class method to
		   mess up our popup. */
		g_signal_stop_emission_by_name (widget, "button_press_event");
	} else {
		if (event->key.keyval != GDK_Return)
			return FALSE;
	}
	
	part = g_object_get_data ((GObject *) widget, "CamelMimePart");
	handler = mail_lookup_handler (g_object_get_data ((GObject *) widget, "mime_type"));
	
	if (handler && handler->applications)
		nitems = g_list_length (handler->applications) + 3;
	else
		nitems = 4;
	menu = g_new0 (EPopupMenu, nitems + 1);
	
	/* Save item */
	memcpy (&menu[current_item], &save_item, sizeof (menu[current_item]));
	menu[current_item].name = g_strdup (_(menu[current_item].name));
	current_item++;

	/* Save All item */
	memcpy (&menu[current_item], &save_all_item, sizeof (menu[current_item]));
	menu[current_item].name = g_strdup (_(menu[current_item].name));
	current_item++;
	 
	/* Inline view item */
	memcpy (&menu[current_item], &view_item, sizeof (menu[current_item]));
	if (handler && handler->builtin) {
		md = g_object_get_data ((GObject *) widget, "MailDisplay");
		
		if (!mail_part_is_displayed_inline (part, md)) {
			if (handler->component) {
				Bonobo_ActivationProperty *prop;
				char *name;
				
				prop = bonobo_server_info_prop_find (handler->component, "name");
				if (!prop) {
					prop = bonobo_server_info_prop_find (handler->component,
									     "description");
				}
				if (prop && prop->v._d == Bonobo_ACTIVATION_P_STRING)
					name = prop->v._u.value_string;
				else
					name = "bonobo";
				menu[current_item].name = g_strdup_printf (_("View Inline (via %s)"), name);
			} else
				menu[current_item].name = g_strdup (_(menu[current_item].name));
		} else
			menu[current_item].name = g_strdup (_("Hide"));
	} else {
		menu[current_item].name = g_strdup (_(menu[current_item].name));
		mask |= 2;
	}
	current_item++;
	
	/* External views */
	if (handler && handler->applications) {
		GnomeVFSMimeApplication *app;
		GList *apps;
		int i;
		
		apps = handler->applications;
		for (i = current_item; i < nitems; i++, apps = apps->next) {
			app = apps->data;
			memcpy (&menu[i], &open_item, sizeof (menu[i]));
			menu[i].name = g_strdup_printf (_(menu[i].name), app->name);
			current_item++;
		}
	} else {
		memcpy (&menu[current_item], &open_item, sizeof (menu[current_item]));
		menu[current_item].name = g_strdup_printf (_(menu[current_item].name), _("External Viewer"));
		mask |= 1;
	}
	
	gtk_menu = e_popup_menu_create (menu, mask, 0, widget);
	e_auto_kill_popup_menu_on_selection_done (gtk_menu);

	if (event->type == GDK_BUTTON_PRESS)
		gtk_menu_popup (gtk_menu, NULL, NULL, NULL, (gpointer)widget, event->button.button, event->button.time);
	else
		gtk_menu_popup (gtk_menu, NULL, NULL, popup_menu_placement_callback, (gpointer)widget, 0, event->key.time);
	
	for (i = 1; i < nitems; i++)
		g_free (menu[i].name);
	g_free (menu);
	
	return TRUE;
}	

static gboolean
pixbuf_uncache (gpointer key)
{
	GdkPixbuf *pixbuf;
	
	pixbuf = g_hash_table_lookup (thumbnail_cache, key);
	g_object_unref (pixbuf);
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
		
		gtk_image_set_from_pixbuf ((GtkImage *) pbl->pixmap, mini);
		gtk_widget_set_size_request (pbl->pixmap, width, height);
		
		/* Restart the cache-cleaning timer */
		g_source_remove_by_user_data (orig_key);
		g_timeout_add (5 * 60 * 1000, pixbuf_uncache, orig_key);
		
		if (pbl->loader) {
			gdk_pixbuf_loader_close (pbl->loader, NULL);
			g_object_unref (pbl->loader);
			camel_object_unref (pbl->mstream);
		}
		
		g_signal_handler_disconnect (pbl->eb, pbl->destroy_id);
		g_free (pbl->type);
		g_free (pbl->cid);
		g_free (pbl);
		
		return FALSE;
	}
	
	/* Not in cache, so get a pixbuf from the wrapper */
	
	if (!GTK_IS_WIDGET (pbl->pixmap)) {
		/* Widget has died */
		if (pbl->mstream)
			camel_object_unref (pbl->mstream);
		
		if (pbl->loader) {
			gdk_pixbuf_loader_close (pbl->loader, NULL);
			g_object_unref (pbl->loader);
		}
		
		g_signal_handler_disconnect (pbl->eb, pbl->destroy_id);
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
			error = !gdk_pixbuf_loader_write (pbl->loader, tmp, len, NULL);
			if (!error)
				return TRUE;
		} else if (!camel_stream_eos (pbl->mstream))
			error = TRUE;
	}
	
	if (error || !pbl->mstream) {
		if (pbl->type)
			pixbuf = e_icon_for_mime_type (pbl->type, 24);
		else
			pixbuf = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/pgp-signature-nokey.png", NULL);
	} else
		pixbuf = gdk_pixbuf_loader_get_pixbuf (pbl->loader);
	
	if (pixbuf == NULL) {
		/* pixbuf is non-existant */
		if (pbl->mstream)
			camel_object_unref (pbl->mstream);
		
		if (pbl->loader) {
			gdk_pixbuf_loader_close (pbl->loader, NULL);
			g_object_unref (pbl->loader);
		}
		
		g_signal_handler_disconnect (pbl->eb, pbl->destroy_id);
		g_free (pbl->type);
		g_free (pbl->cid);
		g_free (pbl);
		
		return FALSE;
	}
	
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
	
	mini = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
	if (error || !pbl->mstream)
		g_object_unref (pixbuf);
	
	gtk_image_set_from_pixbuf ((GtkImage *) pbl->pixmap, mini);
	
	/* Add the pixbuf to the cache */
	g_hash_table_insert (thumbnail_cache, pbl->cid, mini);
	g_timeout_add (5 * 60 * 1000, pixbuf_uncache, pbl->cid);
	
	g_signal_handler_disconnect (pbl->eb, pbl->destroy_id);
	if (pbl->loader) {
		gdk_pixbuf_loader_close (pbl->loader, NULL);
		g_object_unref (pbl->loader);
		camel_object_unref (pbl->mstream);
	}
	
	g_free (pbl->type);
	g_free (pbl);
	
	return FALSE;
}

/* Stop the idle function and free the pbl structure
   as the widget that the pixbuf was to be rendered to
   has died on us. */
static void
embeddable_destroy_cb (GtkObject *embeddable, struct _PixbufLoader *pbl)
{
	g_idle_remove_by_data (pbl);
	if (pbl->mstream)
		camel_object_unref (pbl->mstream);
	
	if (pbl->loader) {
		gdk_pixbuf_loader_close (pbl->loader, NULL);
		g_object_unref (pbl->loader);
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
#warning "what about bonobo_widget_new_subdoc?"
#if 0
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
			bonobo_object_ref (BONOBO_OBJECT (bonobo_widget_get_client_site (
				BONOBO_WIDGET (embedded))));
			
			return embedded;
		}
#endif
	}
	
	if (embedded == NULL)
		return NULL;
	
	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (embedded));
	
	prop_bag = bonobo_control_frame_get_control_property_bag (control_frame, NULL);
	
	if (prop_bag != CORBA_OBJECT_NIL) {
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
		from_address = camel_address_encode ((CamelAddress *) from);
		bonobo_property_bag_client_set_value_string (
			prop_bag, "from_address", 
			from_address, &ev);
		g_free (from_address);
		
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
	g_return_val_if_fail (url != NULL, NULL);
	g_return_val_if_fail (urls != NULL, NULL);
	
	part = g_hash_table_lookup (urls, url);
	if (part == NULL) {
		CamelDataWrapper *wrapper;
		CamelStream *stream = NULL;
		const char *name;
		
		/* See if it's some piece of cached data if it is then pretend it
		 * is a mime part so that we can use the mime part saving routines.
		 * It is gross but it keeps duplicated code to a minimum and helps
		 * out with ref counting and the like.
		 */
		name = strrchr (url, '/');
		name = name ? name : url;
		
		if (fetch_cache) {
			/* look in the soup cache */
			stream = camel_data_cache_get(fetch_cache, FETCH_HTTP_CACHE, url, NULL);
		} else {
			GByteArray *ba = NULL;			

			urls = g_datalist_get_data (md->data, "data_urls");
			g_return_val_if_fail (urls != NULL, NULL);
		
			ba = g_hash_table_lookup (urls, url);
			if (ba) {
				/* we have to copy the data here since the ba may be long gone
				 * by the time the user actually saves the file
				 */
				stream = camel_stream_mem_new_with_buffer (ba->data, ba->len);			
			}
		}

		if (stream) {
			wrapper = camel_data_wrapper_new ();
			camel_data_wrapper_construct_from_stream (wrapper, stream);			
			camel_object_unref (stream);
			part = camel_mime_part_new ();
			camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
			camel_object_unref (wrapper);
			camel_mime_part_set_filename (part, name);
		}
	} else {
		camel_object_ref (part);
	}
	
	if (part) {
		CamelDataWrapper *data;
		
		g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);
		
		data = camel_medium_get_content_object ((CamelMedium *)part);
		if (!mail_content_loaded (data, md, TRUE, NULL, NULL, NULL)) {
			return NULL;
		}
		
		save_part (part);
		camel_object_unref (part);
		return NULL;
	}
	
	g_warning ("Data for url: \"%s\" not found", url);
	
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
		uri_list = g_object_get_data ((GObject *) widget, "uri-list");
		if (uri_list) {
			gtk_selection_data_set (selection_data, selection_data->target, 8,
						uri_list, strlen (uri_list));
			return;
		}
		
		tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX");
		if (!tmpdir) {
			GtkWidget *dialog;
			
			dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_RESPONSE_CLOSE,
							 _("Could not create temporary directory: %s"),
							 g_strerror (errno));
			
			/* FIXME: this should be async */
			gtk_dialog_run ((GtkDialog *) dialog);
			gtk_widget_destroy (dialog);
		}
		
		filename = camel_mime_part_get_filename (part);
		/* This is the default filename used for dnd temporary target of attachment */
		if (!filename)
			filename = _("Unknown");
		
		uri_list = g_strdup_printf ("file://%s/%s", tmpdir, filename);
		
		if (!write_data_to_file (part, uri_list + 7, TRUE)) {
			g_free (uri_list);
			return;
		}
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					uri_list, strlen (uri_list));
		
		g_object_set_data_full ((GObject *) widget, "uri-list", uri_list, g_free);		
		break;
	case DND_TARGET_TYPE_PART_MIME_TYPE:
		if (header_content_type_is (part->content_type, "text", "*")) {
		        GByteArray *ba;
			
			ba = mail_format_get_data_wrapper_text ((CamelDataWrapper *) part, NULL);
			if (ba) {
				gtk_selection_data_set (selection_data, selection_data->target, 8,
							ba->data, ba->len);
				g_byte_array_free (ba, TRUE);
			}
		} else {
			CamelDataWrapper *wrapper;
			CamelStreamMem *cstream;
			
			cstream = (CamelStreamMem *) camel_stream_mem_new ();
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
			camel_data_wrapper_write_to_stream (wrapper, (CamelStream *)cstream);
			
			gtk_selection_data_set (selection_data, selection_data->target, 8,
						cstream->buffer->data, cstream->buffer->len);
			
			camel_object_unref (cstream);
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
	
	uri_list = g_object_get_data ((GObject *) widget, "uri-list");
	if (uri_list) {
		unlink (uri_list + 7);
		g_object_set_data ((GObject *) widget, "uri-list", NULL);
	}
}

/* This is a wrapper function */
void ptr_array_free_notify (gpointer array)
{
	g_ptr_array_free ((GPtrArray *) array, TRUE);
}

static gboolean
do_attachment_header (GtkHTML *html, GtkHTMLEmbedded *eb,
		      CamelMimePart *part, MailDisplay *md)
{
	GtkWidget *button, *mainbox, *hbox, *arrow, *popup;
	MailMimeHandler *handler;
	struct _PixbufLoader *pbl;
	GPtrArray *attachment_array;
	
	pbl = g_new0 (struct _PixbufLoader, 1);
	if (strncasecmp (eb->type, "image/", 6) == 0) {
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
	pbl->pixmap = gtk_image_new();
  	gtk_widget_set_size_request (pbl->pixmap, 24, 24);
	pbl->eb = eb;
	pbl->destroy_id = g_signal_connect (eb, "destroy", G_CALLBACK (embeddable_destroy_cb), pbl);
	
	g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc) pixbuf_gen_idle, pbl, NULL);
	
	mainbox = gtk_hbox_new (FALSE, 0);
	
	button = gtk_button_new ();
	g_object_set_data ((GObject *) button, "MailDisplay", md);
	
	handler = mail_lookup_handler (eb->type);
	if (handler && handler->builtin) {
		g_signal_connect (button, "button_press_event", G_CALLBACK (button_press), part);
		g_signal_connect (button, "key_press_event", G_CALLBACK (button_press), part);
	} else {
		gtk_widget_set_sensitive (button, FALSE);
		GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
	}
	
	/* Drag & Drop */
	drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target = header_content_type_simple (part->content_type);
	camel_strdown (drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target);
	
	gtk_drag_source_set (button, GDK_BUTTON1_MASK,
			     drag_types, num_drag_types,
			     GDK_ACTION_COPY);
	g_signal_connect (button, "drag-data-get", G_CALLBACK (drag_data_get_cb), part);
	g_signal_connect (button, "drag-data-delete", G_CALLBACK (drag_data_delete_cb), part);
	
	g_free (drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target);
	drag_types[DND_TARGET_TYPE_PART_MIME_TYPE].target = NULL;
	
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
	
	/* should this be a gtk_arrow? */
	if (handler && mail_part_is_displayed_inline (part, md))
		arrow = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON);
	else
		arrow = gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), pbl->pixmap, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (button), hbox);
	
	popup = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (popup),
			   gtk_arrow_new (GTK_ARROW_DOWN,
					  GTK_SHADOW_ETCHED_IN));
	
	g_object_set_data ((GObject *) popup, "MailDisplay", md);
	g_object_set_data ((GObject *) popup, "CamelMimePart", part);
	g_object_set_data_full ((GObject *) popup, "mime_type", g_strdup (eb->type), (GDestroyNotify) g_free);

	/* Save attachment pointer in an array for "save all attachment" use */	
	attachment_array = g_datalist_get_data (md->data, "attachment_array");
	if (!attachment_array) {
		attachment_array = g_ptr_array_new ();
		g_datalist_set_data_full (md->data, "attachment_array", 
			attachment_array, (GDestroyNotify) ptr_array_free_notify);
	}
	/* Since the attachment pointer might have been added to the array before,
	remove it first anyway to avoide duplication */
	g_ptr_array_remove (attachment_array, part);
	g_ptr_array_add (attachment_array, part);
		 
	
	g_signal_connect (popup, "button_press_event", G_CALLBACK (pixmap_press), md->scroll);
	g_signal_connect (popup, "key_press_event", G_CALLBACK (pixmap_press), md->scroll);
	
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
	Bonobo_ServerInfo *component;
	GtkWidget *embedded;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	CamelStreamMem *cstream;
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
	
	persist = (Bonobo_PersistStream) Bonobo_Unknown_queryInterface (
		bonobo_widget_get_objref (BONOBO_WIDGET (embedded)),
		"IDL:Bonobo/PersistStream:1.0", NULL);
	
	if (persist == CORBA_OBJECT_NIL) {
		gtk_object_sink (GTK_OBJECT (embedded));
		return FALSE;
	}
	
	/* Write the data to a CamelStreamMem... */
	cstream = (CamelStreamMem *) camel_stream_mem_new ();
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
 	camel_data_wrapper_write_to_stream (wrapper, (CamelStream *)cstream);
	
	/* ...convert the CamelStreamMem to a BonoboStreamMem... */
	bstream = bonobo_stream_mem_create (cstream->buffer->data, cstream->buffer->len, TRUE, FALSE);
	camel_object_unref (cstream);
	
	/* ...and hydrate the PersistStream from the BonoboStream. */
	CORBA_exception_init (&ev);
	Bonobo_PersistStream_load (persist,
				   bonobo_object_corba_objref (BONOBO_OBJECT (bstream)),
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
	pbl->pixmap = gtk_image_new ();
  	gtk_widget_set_size_request (pbl->pixmap, 24, 24);
	pbl->eb = eb;
	pbl->destroy_id = g_signal_connect (eb, "destroy", G_CALLBACK (embeddable_destroy_cb), pbl);
	
	g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc) pixbuf_gen_idle, pbl, NULL);
	
	button = gtk_button_new ();
	g_object_set_data ((GObject *) button, "MailDisplay", md);
	g_signal_connect (button, "button_press_event", G_CALLBACK (button_press), part);
	g_signal_connect (button, "key_press_event", G_CALLBACK (button_press), part);

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
	GConfClient *gconf;
	GHashTable *urls;
	CamelMedium *medium;
	GByteArray *ba;
	
	gconf = mail_config_get_gconf_client ();
	
	urls = g_datalist_get_data (md->data, "part_urls");
	g_return_if_fail (urls != NULL);
	
	/* See if it refers to a MIME part (cid: or http:) */
	medium = g_hash_table_lookup (urls, url);
	if (medium) {
		CamelContentType *content_type;
		CamelDataWrapper *wrapper;
		CamelStream *html_stream;
		
		g_return_if_fail (CAMEL_IS_MEDIUM (medium));
		
		if (md->related)
			g_hash_table_remove (md->related, medium);
		
		wrapper = camel_medium_get_content_object (medium);
		if (!mail_content_loaded (wrapper, md, FALSE, url, html, handle))
			return;
		
		content_type = camel_data_wrapper_get_mime_type_field (wrapper);
		
		html_stream = mail_display_stream_new (html, handle);
		
		if (header_content_type_is (content_type, "text", "*")) {
			mail_format_data_wrapper_write_to_stream (wrapper, md, html_stream);
		} else {
			camel_data_wrapper_write_to_stream (wrapper, html_stream);
		}
		
		camel_object_unref (html_stream);
		
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
		int http_mode;
		
		http_mode = gconf_client_get_int (gconf, "/apps/evolution/mail/display/load_http_images", NULL);
		if (http_mode == MAIL_CONFIG_HTTP_ALWAYS ||
		    g_datalist_get_data (md->data, "load_images")) {
			fetch_remote (md, url, html, handle);
		} else if (http_mode == MAIL_CONFIG_HTTP_SOMETIMES &&
			   !g_datalist_get_data (md->data, "checking_from")) {
			const CamelInternetAddress *from;
			const char *name, *addr;
			
			from = camel_mime_message_get_from (md->current_message);
			g_datalist_set_data (md->data, "checking_from", GINT_TO_POINTER (1));
			
			/* Make sure we aren't deal w/ some sort of a
			   pathological message w/o a From: header */
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

static gboolean
fetch_cancelled (GIOChannel *source, GIOCondition cond, void *user_data)
{
	fetch_cancel ((MailDisplay *) user_data);
	
	return FALSE;
}

static void
fetch_next (MailDisplay *md)
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
	g_object_ref(html);
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
	g_object_unref(rd->html);
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
		
		html_stream = mail_display_stream_new (m->html, m->handle);
		camel_data_wrapper_write_to_stream (data, html_stream);
		camel_object_unref (html_stream);
		
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
	
	if (ba) {
		if (ba->len)
			gtk_html_write (m->html, m->handle, ba->data, ba->len);
		gtk_html_end (m->html, m->handle, GTK_HTML_STREAM_OK);
		return TRUE;
	}
	
	return FALSE;
}

static void
load_content_loaded (struct _mail_msg *mm)
{
	struct _load_content_msg *m = (struct _load_content_msg *)mm;
	
	if (m->display->destroyed)
		return;
	
	if (m->display->current_message == m->message) {
		if (m->handle) {
			if (m->redisplay_counter == m->display->redisplay_counter) {
				if (!try_part_urls (m) && !try_data_urls (m))
					gtk_html_end (m->html, m->handle, GTK_HTML_STREAM_ERROR);
			}
		} else {
			mail_display_redisplay (m->display, FALSE);
		}
	}
}

static void
load_content_free (struct _mail_msg *mm)
{
	struct _load_content_msg *m = (struct _load_content_msg *)mm;
	
	g_free (m->url);
	g_object_unref (m->html);
	g_object_unref (m->display);
	camel_object_unref (m->message);
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
	
	if (md->destroyed)
		return;
	
	loading = g_datalist_get_data (md->data, "loading");
	if (loading) {
		if (g_hash_table_lookup (loading, key))
			return;
	} else {
		loading = g_hash_table_new (NULL, NULL);
		g_datalist_set_data_full (md->data, "loading", loading,
					  (GDestroyNotify) g_hash_table_destroy);
	}
	g_hash_table_insert (loading, (gpointer) key, GINT_TO_POINTER (1));
	
	m = mail_msg_new (&load_content_op, NULL, sizeof (*m));
	m->display = md;
	g_object_ref((m->display));
	m->html = html;
	g_object_ref((html));
	m->handle = handle;
	m->url = g_strdup (url);
	m->redisplay_counter = md->redisplay_counter;
	m->message = md->current_message;
	camel_object_ref (m->message);
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
mail_text_write (MailDisplayStream *stream, MailDisplay *md, CamelMimePart *part,
		 int idx, gboolean printing, const char *text)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *html_filter;
	GConfClient *gconf;
	guint32 flags, rgb;
	GdkColor colour;
	char *buf;
	
	gconf = mail_config_get_gconf_client ();
	
	flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_NL | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
	
	if (!printing)
		flags |= CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
	
	if (!printing && gconf_client_get_bool (gconf, "/apps/evolution/mail/display/mark_citations", NULL))
		flags |= CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	
	buf = gconf_client_get_string (gconf, "/apps/evolution/mail/display/citation_colour", NULL);
	gdk_color_parse (buf ? buf : "#737373", &colour);
	g_free (buf);
	
	rgb = ((colour.red & 0xff00) << 8) | (colour.green & 0xff00) | ((colour.blue & 0xff00) >> 8);
	html_filter = camel_mime_filter_tohtml_new (flags, rgb);
	filtered_stream = camel_stream_filter_new_with_stream ((CamelStream *) stream);
	camel_stream_filter_add (filtered_stream, html_filter);
	camel_object_unref (html_filter);
	
	camel_stream_write ((CamelStream *) stream, "<tt>\n", 5);
	camel_stream_write ((CamelStream *) filtered_stream, text, strlen (text));
	camel_stream_flush ((CamelStream *) filtered_stream);
	camel_stream_write ((CamelStream *) stream, "</tt>\n", 6);
	camel_object_unref (filtered_stream);
	
#if 0
	/* this was the old way of doing it, I don't understand why we need iframes... */
	GByteArray *ba;
	char *xed, *iframe;
	char *btt = "<tt>\n";
	char *ett = "</tt>\n";
	char *htmltext;
	guint32 flags;
	
	flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_NL | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
	
	if (!printing)
		flags |= CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
	
	if (!printing && mail_config_get_citation_highlight ())
		flags |= CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	
	htmltext = camel_text_to_html (text, flags, mail_config_get_citation_color ());
	
	ba = g_byte_array_new ();
	g_byte_array_append (ba, (const guint8 *) btt, strlen (btt) + 1);
	g_byte_array_append (ba, (const guint8 *) htmltext, strlen (htmltext) + 1);
	g_byte_array_append (ba, (const guint8 *) ett, strlen (ett) + 1);
	g_free (htmltext);
	
	xed = g_strdup_printf ("x-evolution-data:%p-%d", part, idx);
	iframe = g_strdup_printf ("<iframe src=\"%s\" frameborder=0 scrolling=no>could not get %s</iframe>", xed, xed);
	mail_display_add_url (md, "data_urls", xed, ba);
	camel_stream_write ((CamelStream *) stream, iframe, strlen (iframe));
	g_free (iframe);
#endif
}

void
mail_error_printf (MailDisplayStream *stream, const char *format, ...)
{
	/* FIXME: it'd be nice if camel-stream had a vprintf method... */
	char *buf, *htmltext;
	va_list ap;
	
	va_start (ap, format);
	buf = g_strdup_vprintf (format, ap);
	va_end (ap);
	
	htmltext = camel_text_to_html (buf, CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
				       CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	g_free (buf);
	
	camel_stream_printf ((CamelStream *) stream, "<em><font color=red>");
	camel_stream_write ((CamelStream *) stream, htmltext, strlen (htmltext));
	camel_stream_printf ((CamelStream *) stream, "</font></em>");
	
	g_free (htmltext);
}


#define COLOR_IS_LIGHT(r, g, b)  ((r + g + b) > (128 * 3))

#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n"  \
                    "<head>\n<meta name=\"generator\" content=\"Evolution Mail Component\">\n</head>\n"

void
mail_display_render (MailDisplay *md, GtkHTML *html, gboolean reset_scroll)
{
	const char *flag, *completed;
	GtkHTMLStream *html_stream;
	MailDisplayStream *stream;

	g_return_if_fail (IS_MAIL_DISPLAY (md));
	g_return_if_fail (GTK_IS_HTML (html));
	
	if (!md->html) {
		/* we've been destroyed */
		return;
	}
	
	html_stream = gtk_html_begin (html);
	if (!reset_scroll) {
		/* This is a hack until there's a clean way to do this. */
		GTK_HTML (md->html)->engine->newPage = FALSE;
	}

	gtk_html_stream_write (html_stream, HTML_HEADER, sizeof (HTML_HEADER) - 1);
	
	if (md->current_message && md->display_style == MAIL_CONFIG_DISPLAY_SOURCE)
		gtk_html_stream_write (html_stream, "<body>\n", 7);
	else
		gtk_html_stream_write (html_stream, "<body marginwidth=0 marginheight=0>\n", 36);
	
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
		/* FIXME: this is also in mail-format.c */
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
				overdue = _("Overdue:");
			
			localtime_r (&target_date, &due);
			
			e_utf8_strftime_fix_am_pm (due_date, sizeof (due_date), _("by %B %d, %Y, %l:%M %P"), &due);
		} else {
			due_date[0] = '\0';
		}
		
		gtk_html_stream_printf (html_stream, "<font color=\"#%s\">"
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
		stream = (MailDisplayStream *) mail_display_stream_new (html, html_stream);
		
		if (md->display_style == MAIL_CONFIG_DISPLAY_SOURCE)
			mail_format_raw_message (md->current_message, md, stream);
		else
			mail_format_mime_message (md->current_message, md, stream);
		
		camel_object_unref (stream);
	}
	
	gtk_html_stream_write (html_stream, "</body></html>\n", 15);
	gtk_html_end (html, html_stream, GTK_HTML_STREAM_OK);
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
	if (md->destroyed)
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
	if (md->destroyed
	    || (medium && !CAMEL_IS_MIME_MESSAGE (medium)))
		return;
	
	/* Clean up from previous message. */
	if (md->current_message) {
		fetch_cancel (md);
		camel_object_unref (md->current_message);
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
mail_display_init (GObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);
	GConfClient *gconf;
	int style;
	
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
	g_object_ref (mail_display->invisible);
	gtk_object_sink ((GtkObject *) mail_display->invisible);
	
	gconf = mail_config_get_gconf_client ();
	style = gconf_client_get_int (gconf, "/apps/evolution/mail/format/message_display_style", NULL);
	mail_display->display_style     = style;
	
	mail_display->printing          = FALSE;
	
	mail_display->priv = g_malloc0(sizeof(*mail_display->priv));
	e_dlist_init(&mail_display->priv->fetch_active);
	e_dlist_init(&mail_display->priv->fetch_queue);
}

static void
mail_display_destroy (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);
	
	if (mail_display->html) {
		g_object_unref (mail_display->html);
		mail_display->html = NULL;
	}
	
	if (mail_display->current_message) {
		camel_object_unref (mail_display->current_message);
		g_datalist_clear (mail_display->data);
		fetch_cancel(mail_display);
		mail_display->current_message = NULL;
	}
	
	g_free (mail_display->charset);
	mail_display->charset = NULL;
	g_free (mail_display->selection);
	mail_display->selection = NULL;
	
	if (mail_display->folder) {
		if (mail_display->info)
			camel_folder_free_message_info (mail_display->folder, mail_display->info);
		camel_object_unref (mail_display->folder);
		mail_display->folder = NULL;
	}
	
	g_free (mail_display->data);
	mail_display->data = NULL;
	
	if (mail_display->idle_id) {
		gtk_timeout_remove (mail_display->idle_id);
		mail_display->idle_id = 0;
	}
	
	if (mail_display->invisible) {
		g_object_unref (mail_display->invisible);
		mail_display->invisible = NULL;
	}
	
	if (mail_display->priv && mail_display->priv->display_notify_id) {
		GConfClient *gconf = mail_config_get_gconf_client ();
		gconf_client_notify_remove (gconf, mail_display->priv->display_notify_id);
		mail_display->priv->display_notify_id = 0;
	}
	
	g_free (mail_display->priv);
	mail_display->priv = NULL;
	
	mail_display->destroyed = TRUE;
	
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
		char *path;
		
		path = g_alloca (strlen (evolution_dir) + 16);
		sprintf (path, "%s/cache", evolution_dir);
		
		/* cache expiry - 2 hour access, 1 day max */
		fetch_cache = camel_data_cache_new(path, 0, NULL);
		camel_data_cache_set_expire_age(fetch_cache, 24*60*60);
		camel_data_cache_set_expire_access(fetch_cache, 2*60*60);
		
		mail_display_parent_class = g_type_class_ref (PARENT_TYPE);
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
	
	src = g_object_get_data ((GObject *) mail_display, "current_src_uri");
	
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
	E_POPUP_ITEM (N_("Open Link in Browser"), G_CALLBACK (link_open_in_browser),   MASK_URL),
	E_POPUP_ITEM (N_("Copy Link Location"), G_CALLBACK (link_copy_location),  MASK_URL),
#if 0
	E_POPUP_ITEM (N_("Save Link as (FIXME)"), G_CALLBACK (link_save_as),  MASK_URL),
#endif
	E_POPUP_ITEM (N_("Save Image as..."), G_CALLBACK (image_save_as), MASK_SRC), 
	
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
	Bonobo_Listener listener;
	gboolean hidden;
};

/* Aiieee!  Global Data! */
static GtkWidget *the_popup = NULL;

static void
popup_window_destroy_cb (PopupInfo *pop, GObject *deadbeef)
{
	the_popup = NULL;

	if (pop->destroy_timeout != 0)
		g_source_remove(pop->destroy_timeout);

	bonobo_event_source_client_remove_listener (bonobo_widget_get_objref (BONOBO_WIDGET (pop->w)),
						    pop->listener,
						    NULL);
	CORBA_Object_release (pop->listener, NULL);
	g_object_unref(pop->w);
	g_free (pop);
}

static int
popup_timeout_cb (gpointer user_data)
{
	PopupInfo *pop = (PopupInfo *) user_data;

	pop->destroy_timeout = 0;
	gtk_widget_destroy (pop->win);
	
	return 0;
}

static int
popup_enter_cb (GtkWidget *w, GdkEventCrossing *ev, gpointer user_data)
{
	PopupInfo *pop = (PopupInfo *) user_data;
	
	if (pop->destroy_timeout)
		gtk_timeout_remove (pop->destroy_timeout);
	pop->destroy_timeout = 0;
	
	return 0;
}

static int
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
	gtk_window_set_position (GTK_WINDOW (widget), GTK_WIN_POS_MOUSE);
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
	g_object_ref(w);
	the_popup = pop->win = gtk_window_new (GTK_WINDOW_POPUP);
	fr = gtk_frame_new (NULL);
	
	gtk_container_add (GTK_CONTAINER (pop->win), fr);
	gtk_container_add (GTK_CONTAINER (fr), w);
	
	gtk_window_set_resizable (GTK_WINDOW (pop->win), FALSE);
	
	g_signal_connect (pop->win, "enter_notify_event", G_CALLBACK (popup_enter_cb), pop);
	g_signal_connect (pop->win, "leave_notify_event", G_CALLBACK (popup_leave_cb), pop);
	g_signal_connect_after (pop->win, "realize", G_CALLBACK (popup_realize_cb), pop);
	g_signal_connect (pop->win, "size_allocate", G_CALLBACK (popup_size_allocate_cb), pop);
	
	g_object_weak_ref ((GObject *) pop->win, (GWeakNotify) popup_window_destroy_cb, pop);
	
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
			point = html_engine_get_point_at (e, event->x, event->y, FALSE);
			
			if (point) {
				const char *url, *src;
				
				url = html_object_get_url (point->object);
				src = html_object_get_src (point->object);
				
				if (url && !strncasecmp (url, "mailto:", 7)) {
					PopupInfo *pop;
					char *url_decoded;
					
					url_decoded = gtk_html_get_url_object_relative (GTK_HTML (widget),
											point->object,
											url);
					camel_url_decode (url_decoded);
					
					popup_thing = bonobo_widget_new_control ("OAFIID:GNOME_Evolution_Addressbook_AddressPopup",
										 CORBA_OBJECT_NIL);
					
					bonobo_widget_set_property (BONOBO_WIDGET (popup_thing),
								    "email", TC_CORBA_string, url_decoded+7,
								    NULL);
					g_free (url_decoded);
					
					pop = make_popup_window (popup_thing);
					
					pop->listener = bonobo_event_source_client_add_listener_full(
						bonobo_widget_get_objref (BONOBO_WIDGET (popup_thing)),
						g_cclosure_new (G_CALLBACK (listener_cb), pop, NULL),
						NULL, NULL);
				} else if (url || src) {
				        int hide_mask = 0;
					
					if (!url)
						hide_mask |= MASK_URL;
					
					if (!src)
						hide_mask |= MASK_SRC;
					
					g_free (g_object_get_data ((GObject *) mail_display, "current_src_uri"));
					g_object_set_data ((GObject *) mail_display, "current_src_uri", 
							  gtk_html_get_url_object_relative (GTK_HTML (widget),
											    point->object,
											    src));
					
					e_popup_menu_run (link_menu, (GdkEvent *) event, 0, hide_mask, mail_display);
				}
				
				html_point_destroy (point);
				return TRUE;
			}			
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
	
	point = html_engine_get_point_at (e, x, y, FALSE);
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

static int
html_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event, MailDisplay *mail_display)
{
	update_active (widget, event->x, event->y, mail_display);
	
	return FALSE;
}

static int
html_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, MailDisplay *mail_display)
{
	int x, y;
	
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
	
	return FALSE;
}

static void
html_iframe_created (GtkWidget *w, GtkHTML *iframe, MailDisplay *mail_display)
{
	g_signal_connect (iframe, "button_press_event",
			  G_CALLBACK (html_button_press_event), mail_display);
	g_signal_connect (iframe, "motion_notify_event",
			  G_CALLBACK (html_motion_notify_event), mail_display);
	g_signal_connect (iframe, "enter_notify_event",
			  G_CALLBACK (html_enter_notify_event), mail_display);
}

static GNOME_Evolution_ShellView
retrieve_shell_view_interface_from_control (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;
	
	control_frame = bonobo_control_get_control_frame (control, NULL);
	
	if (control_frame == NULL)
		return CORBA_OBJECT_NIL;
	
	CORBA_exception_init (&ev);
	shell_view_interface = Bonobo_Unknown_queryInterface (control_frame,
							      "IDL:GNOME/Evolution/ShellView:1.0",
							      &ev);

	if (BONOBO_EX (&ev))
		shell_view_interface = CORBA_OBJECT_NIL;
	
	CORBA_exception_free (&ev);
	
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

		bonobo_object_release_unref (shell_view_interface, NULL);
		
		/* yeah we only set the first one.  Why?  Because it seems to leave
		   random ones lying around otherwise.  Shrug. */
		break;
	}
	
	g_object_unref (it);
}

/* For now show every url but possibly limit it to showing only http:
   or ftp: urls */
static void
html_on_url (GtkHTML *html, const char *url, MailDisplay *mail_display)
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

/* If if a gconf setting for the mail display has changed redisplay to pick up the changes */
static void
display_notify (GConfClient *gconf, guint cnxn_id, GConfEntry *entry, gpointer data)
{
	MailDisplay *md = data;
	gchar *tkey;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (gconf_entry_get_key (entry) != NULL);
	g_return_if_fail (gconf_entry_get_value (entry) != NULL);
	
	tkey = strrchr (entry->key, '/');
	
	g_return_if_fail (tkey != NULL);

	if (!strcmp (tkey, "/animate_images")) {
		gtk_html_set_animate (md->html, gconf_value_get_bool (gconf_entry_get_value(entry)));
	} else if (!strcmp (tkey, "/citation_color") 
		   || !strcmp (tkey, "/mark_citations")) {
		mail_display_queue_redisplay (md);
	} else if (!strcmp (tkey, "/caret_mode")) {
		gtk_html_set_caret_mode(md->html, gconf_value_get_bool (gconf_entry_get_value(entry)));
	}
}

GtkWidget *
mail_display_new (void)
{
	MailDisplay *mail_display = g_object_new (mail_display_get_type (), NULL);
	GtkWidget *scroll, *html;
	GdkAtom clipboard_atom;
	HTMLTokenizer *tok;
	GConfClient *gconf;

	gtk_box_set_homogeneous (GTK_BOX (mail_display), FALSE);
	gtk_widget_show (GTK_WIDGET (mail_display));
	
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_box_pack_start_defaults (GTK_BOX (mail_display), scroll);
	gtk_widget_show (scroll);
	
	html = gtk_html_new ();
	tok = e_searching_tokenizer_new ();
	html_engine_set_tokenizer (GTK_HTML (html)->engine, tok);
	g_object_unref (tok);
	
	mail_display_initialize_gtkhtml (mail_display, GTK_HTML (html));
	
	gtk_container_add (GTK_CONTAINER (scroll), html);
	gtk_widget_show (GTK_WIDGET (html));
	
	g_signal_connect (mail_display->invisible, "selection_get",
			  G_CALLBACK (invisible_selection_get_callback), mail_display);
	g_signal_connect (mail_display->invisible, "selection_clear_event",
			  G_CALLBACK (invisible_selection_clear_event_callback), mail_display);
	
	gtk_selection_add_target (mail_display->invisible,
				  GDK_SELECTION_PRIMARY, GDK_SELECTION_TYPE_STRING, 1);
	
	clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	if (clipboard_atom != GDK_NONE)
		gtk_selection_add_target (mail_display->invisible,
					  clipboard_atom, GDK_SELECTION_TYPE_STRING, 1);
	
	gconf = mail_config_get_gconf_client ();
	gtk_html_set_animate (GTK_HTML (html), gconf_client_get_bool (gconf, "/apps/evolution/mail/display/animate_images", NULL));
	gtk_html_set_caret_mode (GTK_HTML (html), gconf_client_get_bool (gconf, "/apps/evolution/mail/display/caret_mode", NULL));
	
	gconf_client_add_dir (gconf, "/apps/evolution/mail/display",GCONF_CLIENT_PRELOAD_NONE, NULL);
	mail_display->priv->display_notify_id = gconf_client_notify_add (gconf, "/apps/evolution/mail/display",
									 display_notify, mail_display, NULL, NULL);
	
	mail_display->scroll = GTK_SCROLLED_WINDOW (scroll);
	mail_display->html = GTK_HTML (html);
	g_object_ref (mail_display->html);
	mail_display->last_active = NULL;
	mail_display->data = g_new0 (GData *, 1);
	g_datalist_init (mail_display->data);
	
	return GTK_WIDGET (mail_display);
}

void
mail_display_initialize_gtkhtml (MailDisplay *mail_display, GtkHTML *html)
{
	gtk_html_set_default_content_type (GTK_HTML (html), "text/html; charset=utf-8");
	
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	
	g_signal_connect (html, "url_requested",
			  G_CALLBACK (on_url_requested),
			  mail_display);
	g_signal_connect (html, "object_requested",
			  G_CALLBACK (on_object_requested),
			  mail_display);
	g_signal_connect (html, "link_clicked",
			  G_CALLBACK (on_link_clicked),
			  mail_display);
	g_signal_connect (html, "button_press_event",
			  G_CALLBACK (html_button_press_event), mail_display);
	g_signal_connect (html, "motion_notify_event",
			  G_CALLBACK (html_motion_notify_event), mail_display);
	g_signal_connect (html, "enter_notify_event",
			  G_CALLBACK (html_enter_notify_event), mail_display);
	g_signal_connect (html, "iframe_created",
			  G_CALLBACK (html_iframe_created), mail_display);
	g_signal_connect (html, "on_url",
			  G_CALLBACK (html_on_url), mail_display);
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
		icon_path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
						       icon_name, TRUE, NULL);
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
