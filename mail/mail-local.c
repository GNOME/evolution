/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-local.c: Local mailbox support. */

/* 
 * Author: 
 *  Michael Zucchi <NotZed@helixcode.com>
 *  Peter Williams <peterw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


/*
  code for handling local mail boxes
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <bonobo.h>
#include <gnome.h>
#include <glade/glade.h>

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"
#include "folder-browser.h"

#include "camel/camel.h"

#include "filter/vfolder-context.h"
#include "filter/vfolder-rule.h"
#include "filter/vfolder-editor.h"

#include "mail.h"
#include "mail-local.h"
#include "mail-tools.h"
#include "mail-threads.h"

#define d(x)

struct _local_meta {
	char *path;		/* path of metainfo file */

	char *format;		/* format of mailbox */
	char *name;		/* name of mbox itself */
};

static struct _local_meta *
load_metainfo(const char *path)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	struct _local_meta *meta;

	meta = g_malloc0(sizeof(*meta));
	meta->path = g_strdup(path);

	printf("Loading folder metainfo from : %s\n", meta->path);

	doc = xmlParseFile(meta->path);
	if (doc == NULL) {
		goto dodefault;
	}
	node = doc->root;
	if (strcmp(node->name, "folderinfo")) {
		goto dodefault;
	}
	node = node->childs;
	while (node) {
		if (!strcmp(node->name, "folder")) {
			meta->format = xmlGetProp(node, "type");
			meta->name = xmlGetProp(node, "name");
		}
		node = node->next;
	}
	xmlFreeDoc(doc);
	return meta;

dodefault:
	meta->format = g_strdup("mbox"); /* defaults */
	meta->name = g_strdup("mbox");
	if (doc)
		xmlFreeDoc(doc);
	return meta;
}

static void
free_metainfo(struct _local_meta *meta)
{
	g_free(meta->path);
	g_free(meta->format);
	g_free(meta->name);
	g_free(meta);
}

static int
save_metainfo(struct _local_meta *meta)
{
	xmlDocPtr doc;
	xmlNodePtr root, node;
	int ret;

	printf("Saving folder metainfo to : %s\n", meta->path);

	doc = xmlNewDoc("1.0");
	root = xmlNewDocNode(doc, NULL, "folderinfo", NULL);
	xmlDocSetRootElement(doc, root);

	node  = xmlNewChild(root, NULL, "folder", NULL);
	xmlSetProp(node, "type", meta->format);
	xmlSetProp(node, "name", meta->name);

	ret = xmlSaveFile(meta->path, doc);
	xmlFreeDoc(doc);
	return ret;
}

/* maps a local uri to the real type */
char *
mail_local_map_uri(const char *uri)
{
	CamelURL *url;
	char *metapath;
	char *storename;
	struct _local_meta *meta;
	CamelException *ex;

	if (strncmp(uri, "file:", 5)) {
		g_warning("Trying to map non-local uri: %s", uri);
		return g_strdup(uri);
	}

	ex = camel_exception_new();
	url = camel_url_new(uri, ex);
	if (camel_exception_is_set(ex)) {
		camel_exception_free(ex);
		return g_strdup(uri);
	}
	camel_exception_free(ex);

	metapath = g_strdup_printf("%s/local-metadata.xml", url->path);
	meta = load_metainfo(metapath);
	g_free(metapath);

	/* change file: to format: */
	camel_url_set_protocol(url, meta->format);
	storename = camel_url_to_string(url, TRUE);
	camel_url_free(url);

	return storename;
}

CamelFolder *
mail_tool_local_uri_to_folder(const char *uri, CamelException *ex)
{
	CamelURL *url;
	char *metapath;
	char *storename;
	CamelFolder *folder = NULL;
	struct _local_meta *meta;

	if (strncmp(uri, "file:", 5)) {
		return NULL;
	}

	printf("opening local folder %s\n", uri);

	/* get the actual location of the mailbox */
	url = camel_url_new(uri, ex);
	if (camel_exception_is_set(ex)) {
		return NULL;
	}

	metapath = g_strdup_printf("%s/local-metadata.xml", url->path);
	meta = load_metainfo(metapath);
	g_free(metapath);

	/* change file: to format: */
	camel_url_set_protocol(url, meta->format);
	storename = camel_url_to_string(url, TRUE);

	printf("store name is  %s\n", storename);

	folder = mail_tool_get_folder_from_urlname (storename, meta->name, FALSE, ex);
	camel_url_free(url);
	free_metainfo(meta);

	return folder;
}

/*
   open new
   copy old->new
   close old
   rename old oldsave
   rename new old
   open oldsave
   delete oldsave

   close old
   rename oldtmp
   open new
   open oldtmp
   copy oldtmp new
   close oldtmp
   close oldnew

*/

static void update_progress(char *fmt, float percent)
{
	if (fmt)
		mail_op_set_message ("%s", fmt);
	/*mail_op_set_percentage (percent);*/
}

/* ******************** */

typedef struct reconfigure_folder_input_s {
	FolderBrowser *fb;
	gchar *newtype;
	GtkWidget *frame;
	GtkWidget *apply;
	GtkWidget *cancel;
	GtkOptionMenu *optionlist;
} reconfigure_folder_input_t;

static gchar *describe_reconfigure_folder (gpointer in_data, gboolean gerund);
static void setup_reconfigure_folder   (gpointer in_data, gpointer op_data, CamelException *ex);
static void do_reconfigure_folder      (gpointer in_data, gpointer op_data, CamelException *ex);
static void cleanup_reconfigure_folder (gpointer in_data, gpointer op_data, CamelException *ex);

static gchar *
describe_reconfigure_folder (gpointer in_data, gboolean gerund)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	if (gerund)
		return g_strdup_printf (_("Changing folder \"%s\" to \"%s\" format"),
					input->fb->uri,
					input->newtype);
	else
		return g_strdup_printf (_("Change folder \"%s\" to \"%s\" format"),
					input->fb->uri,
					input->newtype);
}

static void
setup_reconfigure_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	if (!IS_FOLDER_BROWSER (input->fb)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Input has a bad FolderBrowser in reconfigure_folder");
		return;
	}

	if (!input->newtype) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No new folder type in reconfigure_folder");
		return;
	}

	gtk_object_ref (GTK_OBJECT (input->fb));
}

static void
do_reconfigure_folder(gpointer in_data, gpointer op_data, CamelException *ex)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	CamelStore *fromstore = NULL, *tostore = NULL;
	char *fromurl = NULL, *tourl = NULL;
	CamelFolder *fromfolder = NULL, *tofolder = NULL;

	char *metapath;
	char *tmpname;
	char *uri;
	CamelURL *url = NULL;
	struct _local_meta *meta;

	printf("reconfiguring folder: %s to type %s\n", input->fb->uri, input->newtype);

	/* get the actual location of the mailbox */
	url = camel_url_new(input->fb->uri, ex);
	if (camel_exception_is_set(ex)) {
		g_warning("%s is not a workable url!", input->fb->uri);
		goto cleanup;
	}

	metapath = g_strdup_printf("%s/local-metadata.xml", url->path);
	meta = load_metainfo(metapath);
	g_free(metapath);

	/* first, 'close' the old folder */
	if (input->fb->folder != NULL) {
		update_progress("Closing current folder", 0.0);

		mail_tool_camel_lock_up ();
		camel_folder_sync(input->fb->folder, FALSE, ex);
		mail_tool_camel_lock_down ();
		camel_object_unref (CAMEL_OBJECT (input->fb->folder));
		input->fb->folder = NULL;
	}

	camel_url_set_protocol(url, meta->format);
	fromurl = camel_url_to_string(url, TRUE);
	camel_url_set_protocol(url, input->newtype);
	tourl = camel_url_to_string(url, TRUE);

	printf("opening stores %s and %s\n", fromurl, tourl);

	mail_tool_camel_lock_up ();
	fromstore = camel_session_get_store(session, fromurl, ex);
	mail_tool_camel_lock_down ();

	if (camel_exception_is_set(ex))
		goto cleanup;

	mail_tool_camel_lock_up ();
	tostore = camel_session_get_store(session, tourl, ex);
	mail_tool_camel_lock_down ();
	if (camel_exception_is_set(ex))
		goto cleanup;

	/* rename the old mbox and open it again */
	tmpname = g_strdup_printf("%s_reconfig", meta->name);
	printf("renaming %s to %s, and opening it\n", meta->name, tmpname);
	update_progress("Renaming old folder and opening", 0.0);

	mail_tool_camel_lock_up ();
	camel_store_rename_folder(fromstore, meta->name, tmpname, ex);
	if (camel_exception_is_set(ex)) {
		mail_tool_camel_lock_down ();
		goto cleanup;
	}

	fromfolder = camel_store_get_folder(fromstore, tmpname, TRUE, ex);
	if (fromfolder == NULL || camel_exception_is_set(ex)) {
		/* try and recover ... */
		camel_exception_clear (ex);
		camel_store_rename_folder(fromstore, tmpname, meta->name, ex);
		mail_tool_camel_lock_down ();
		goto cleanup;
	}

	/* create a new mbox */
	printf("Creating the destination mbox\n");
	update_progress("Creating new folder", 0.0);

	tofolder = camel_store_get_folder(tostore, meta->name, TRUE, ex);
	if (tofolder == NULL || camel_exception_is_set(ex)) {
		printf("cannot open destination folder\n");
		/* try and recover ... */
		camel_exception_clear (ex);
		camel_store_rename_folder(fromstore, tmpname, meta->name, ex);
		mail_tool_camel_lock_down ();
		goto cleanup;
	}

	update_progress("Copying messages", 0.0);
	mail_tool_move_folder_contents (fromfolder, tofolder, FALSE, ex);

	printf("delete old mbox ...\n");
	camel_store_delete_folder(fromstore, tmpname, ex);
	mail_tool_camel_lock_down ();

	/* switch format */
	g_free(meta->format);
	meta->format = g_strdup(input->newtype);
	if (save_metainfo(meta) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "Cannot save folder metainfo; "
				      "you'll probably find you can't\n"
				      "open this folder anymore: %s", tourl);
	}
	free_metainfo(meta);

	/* force a reload of the newly formatted folder */
	printf("opening new source\n");
	uri = g_strdup(input->fb->uri);
	folder_browser_set_uri(input->fb, uri);
	g_free(uri);

	/* and unref our copy of the new folder ... */
 cleanup:
	if (tofolder)
		camel_object_unref (CAMEL_OBJECT (tofolder));
	if (fromfolder)
		camel_object_unref (CAMEL_OBJECT (fromfolder));
	if (fromstore)
		camel_object_unref (CAMEL_OBJECT (fromstore));
	if (tostore)
		camel_object_unref (CAMEL_OBJECT (tostore));
	g_free(fromurl);
	g_free(tourl);
	if (url)
		camel_url_free (url);
}

static void
cleanup_reconfigure_folder  (gpointer in_data, gpointer op_data, CamelException *ex)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	if (camel_exception_is_set(ex)) {
		GtkWidget *win = gtk_widget_get_ancestor((GtkWidget *)input->frame, GTK_TYPE_WINDOW);
		gnome_error_dialog_parented ("If you can no longer open this mailbox, then\n"
					     "you may need to repair it manually.", GTK_WINDOW (win));
	}

	gtk_object_unref (GTK_OBJECT (input->fb));
	g_free (input->newtype);
}

static const mail_operation_spec op_reconfigure_folder =
{
	describe_reconfigure_folder,
	0,
	setup_reconfigure_folder,
	do_reconfigure_folder,
	cleanup_reconfigure_folder
};

static void
reconfigure_clicked(GnomeDialog *d, int button, reconfigure_folder_input_t *data)
{
	if (button == 0) {
		GtkMenu *menu;
		int type;
		char *types[] = { "mh", "mbox" };

		menu = (GtkMenu *)gtk_option_menu_get_menu(data->optionlist);
		type = g_list_index(GTK_MENU_SHELL(menu)->children, gtk_menu_get_active(menu));
		if (type < 0 || type > 1)
			type = 1;

		gtk_widget_set_sensitive(data->frame, FALSE);
		gtk_widget_set_sensitive(data->apply, FALSE);
		gtk_widget_set_sensitive(data->cancel, FALSE);

		data->newtype = g_strdup (types[type]);
		mail_operation_queue (&op_reconfigure_folder, data, TRUE);
	}

	if (button != -1)
		gnome_dialog_close(d);
}

void
local_reconfigure_folder(FolderBrowser *fb)
{
	CamelStore *store;
	GladeXML *gui;
	GnomeDialog *gd;
	reconfigure_folder_input_t *data;

	if (fb->folder == NULL) {
		g_warning("Trying to reconfigure nonexistant folder");
		return;
	}

	data = g_new (reconfigure_folder_input_t, 1);

	store = camel_folder_get_parent_store(fb->folder);

	gui = glade_xml_new(EVOLUTION_GLADEDIR "/local-config.glade", "dialog_format");
	gd = (GnomeDialog *)glade_xml_get_widget (gui, "dialog_format");

	data->frame = glade_xml_get_widget (gui, "frame_format");
	data->apply = glade_xml_get_widget (gui, "apply_format");
	data->cancel = glade_xml_get_widget (gui, "cancel_format");
	data->optionlist = (GtkOptionMenu *)glade_xml_get_widget (gui, "option_format");
	data->newtype = NULL;
	data->fb = fb;

	gtk_label_set_text((GtkLabel *)glade_xml_get_widget (gui, "label_format"),
			   ((CamelService *)store)->url->protocol);

	gtk_signal_connect((GtkObject *)gd, "clicked", reconfigure_clicked, data);
	gtk_object_unref((GtkObject *)gui);

	GDK_THREADS_ENTER ();
	gnome_dialog_run_and_close (GNOME_DIALOG (gd));
	GDK_THREADS_LEAVE ();
}
