
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

CamelFolder *
local_uri_to_folder(const char *uri, CamelException *ex)
{
	CamelURL *url;
	char *metapath;
	char *storename;
	CamelStore *store;
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

	store = camel_session_get_store(session, storename, ex);
	g_free(storename);
	if (store) {
		folder = camel_store_get_folder(store, meta->name, FALSE, ex);
		gtk_object_unref((GtkObject *)store);
	}
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

static void update_progress(GtkProgress *progress, char *fmt, float percent)
{
	if (fmt)
		gtk_progress_set_format_string(progress, fmt);
	gtk_progress_set_percentage(progress, percent);
	while( gtk_events_pending() )
		gtk_main_iteration();
}

static void
do_local_reconfigure_folder(FolderBrowser *fb, char *newtype, GtkProgress *progress, CamelException *ex)
{
	CamelStore *fromstore, *tostore;
	char *fromurl, *tourl, *uri;
	CamelFolder *fromfolder, *tofolder;
	GPtrArray *uids;
	int i;
	char *metapath;
	char *tmpname;
	CamelURL *url;
	struct _local_meta *meta;

	printf("reconfiguring folder: %s to type %s\n", fb->uri, newtype);

	/* get the actual location of the mailbox */
	url = camel_url_new(fb->uri, ex);
	if (url == NULL || camel_exception_is_set(ex)) {
		camel_exception_free(ex);
		g_warning("%s is not a workable url!", fb->uri);
		return;
	}

	metapath = g_strdup_printf("%s/local-metadata.xml", url->path);
	meta = load_metainfo(metapath);
	g_free(metapath);

	/* first, 'close' the old folder */
	if (fb->folder != NULL) {
		update_progress(progress, "Closing current folder", 0.0);
		printf("Closing old folder ...\n");
		camel_folder_sync(fb->folder, FALSE, ex);
		gtk_object_unref (GTK_OBJECT (fb->folder));
		fb->folder = NULL;
	}

	camel_url_set_protocol(url, meta->format);
	fromurl = camel_url_to_string(url, TRUE);
	camel_url_set_protocol(url, newtype);
	tourl = camel_url_to_string(url, TRUE);

	printf("opening stores %s and %s\n", fromurl, tourl);
	fromstore = camel_session_get_store(session, fromurl, ex);
	if (camel_exception_is_set(ex)) {
		return;
	}
	tostore = camel_session_get_store(session, tourl, ex);
	if (camel_exception_is_set(ex)) {
		return;
	}

	/* rename the old mbox and open it again */
	tmpname = g_strdup_printf("%s_reconfig", meta->name);
	printf("renaming mbox to mboxtmp, and opening it\n");
	update_progress(progress, "Renaming old folder and opening", 0.0);
	camel_store_rename_folder(fromstore, meta->name, tmpname, ex);
	if (camel_exception_is_set(ex)) {
		return;
	}
	fromfolder = camel_store_get_folder(fromstore, tmpname, TRUE, ex);
	if (fromfolder == NULL || camel_exception_is_set(ex)) {
		/* try and recover ... */
		camel_store_rename_folder(fromstore, tmpname, meta->name, ex);
		return;
	}

	/* create a new mbox */
	printf("Creating the destination mbox\n");
	update_progress(progress, "Creating new folder", 0.0);
	tofolder = camel_store_get_folder(tostore, meta->name, TRUE, ex);
	if (tofolder == NULL || camel_exception_is_set(ex)) {
		printf("cannot open destination folder\n");
		/* try and recover ... */
		camel_store_rename_folder(fromstore, tmpname, meta->name, ex);
		return;
	}

	/* copy the messages across */
	uids = camel_folder_get_uids (fromfolder);
	printf("got %d messages in source\n", uids->len);
	update_progress(progress, "Copying messages", 0.0);
	for (i = 0; i < uids->len; i++) {
		CamelMimeMessage *msg;
		char *uid = uids->pdata[i];

		update_progress(progress, NULL, i/uids->len);

		printf("copying message %s\n", uid);
		msg = camel_folder_get_message(fromfolder, uid, ex);
		if (camel_exception_is_set(ex)) {
			/* we're fucked a bit ... */
			/* need to: delete new folder
			   rename old back again */
			g_warning("cannot get message");
			return;
		}
		camel_folder_append_message(tofolder, msg,
					    camel_folder_get_message_flags(fromfolder, uid),
					    ex);
		if (camel_exception_is_set(ex)) {
			/* we're fucked a bit ... */
			/* need to: delete new folder
			   rename old back again */
			g_warning("cannot append message");
			return;
		}
		gtk_object_unref((GtkObject *)msg);
#warning "because flags were removed from the message"
#warning "we can't keep them when converting mail storage format"
	}
	update_progress(progress, "Synchronising", 0.0);

	/* sync while we're doing i/o, just to make sure */
	camel_folder_sync(tofolder, FALSE, ex);
	if (camel_exception_is_set(ex)) {
		/* same again */
	}

	/* delete everything in the old mailbox */
	printf("deleting old mbox contents\n");
	for (i = 0; i < uids->len; i++) {
		char *uid = uids->pdata[i];
		camel_folder_delete_message(fromfolder, uid);
	}
	camel_folder_sync(fromfolder, TRUE, ex);
	gtk_object_unref((GtkObject *)fromfolder);
	printf("and old mbox ...\n");
	camel_store_delete_folder(fromstore, tmpname, ex);

	/* switch format */
	g_free(meta->format);
	meta->format = g_strdup(newtype);
	if (save_metainfo(meta) == -1) {
		g_warning("Cannot save folder metainfo, you'll probably find you can't\n"
			  "open this folder anymore: %s", tourl);
	}
	free_metainfo(meta);

	/* force a reload of the newly formatted folder */
	printf("opening new source\n");
	uri = g_strdup(fb->uri);
	folder_browser_set_uri(fb, uri);
	g_free(uri);

	/* and unref our copy of the new folder ... */
	gtk_object_unref((GtkObject *)tofolder);
	g_free(fromurl);
	g_free(tourl);
}

struct _reconfig_data {
	FolderBrowser *fb;
	GtkProgress *progress;
	GtkWidget *frame;
	GtkWidget *apply;
	GtkWidget *cancel;
	GtkOptionMenu *optionlist;
};

static void
reconfigure_clicked(GnomeDialog *d, int button, struct _reconfig_data *data)
{
	if (button == 0) {
		GtkMenu *menu;
		int type;
		char *types[] = { "mh", "mbox" };
		CamelException *ex;

		ex = camel_exception_new();

		menu = (GtkMenu *)gtk_option_menu_get_menu(data->optionlist);
		type = g_list_index(GTK_MENU_SHELL(menu)->children, gtk_menu_get_active(menu));
		if (type < 0 || type > 1)
			type = 1;

		gtk_progress_set_percentage(data->progress, 0.0);
		gtk_widget_set_sensitive(data->frame, FALSE);
		gtk_widget_set_sensitive(data->apply, FALSE);
		gtk_widget_set_sensitive(data->cancel, FALSE);

		do_local_reconfigure_folder(data->fb, types[type], data->progress, ex);
		if (camel_exception_is_set(ex)) {
			GtkWidget *win = gtk_widget_get_ancestor((GtkWidget *)d, GTK_TYPE_WINDOW);
			char *error;

			error = g_strdup_printf("A failure occured:\n %s\n\n"
						"If you can no longer open this mailbox, then\n"
						"you may need to repair it manually.",
						camel_exception_get_description(ex));
			gnome_error_dialog_parented(error, GTK_WINDOW (win));
			g_free(error);
		}
		camel_exception_free(ex);
	}
	if (button != -1) {
		gnome_dialog_close(d);
	}
}

void
local_reconfigure_folder(FolderBrowser *fb)
{
	CamelStore *store;
	GladeXML *gui;
	GnomeDialog *gd;
	struct _reconfig_data *data;

	if (fb->folder == NULL) {
		g_warning("Trying to reconfigure nonexistant folder");
		return;
	}

	data = g_malloc0(sizeof(*data));

	store = camel_folder_get_parent_store(fb->folder);

	gui = glade_xml_new(EVOLUTION_GLADEDIR "/local-config.glade", "dialog_format");
	gd = (GnomeDialog *)glade_xml_get_widget (gui, "dialog_format");

	data->progress = (GtkProgress *)glade_xml_get_widget (gui, "progress_format");
	gtk_progress_set_show_text(data->progress, TRUE);
	data->frame = glade_xml_get_widget (gui, "frame_format");
	data->apply = glade_xml_get_widget (gui, "apply_format");
	data->cancel = glade_xml_get_widget (gui, "cancel_format");
	data->optionlist = (GtkOptionMenu *)glade_xml_get_widget (gui, "option_format");
	data->fb = fb;

	gtk_label_set_text((GtkLabel *)glade_xml_get_widget (gui, "label_format"),
			   ((CamelService *)store)->url->protocol);

	gtk_signal_connect((GtkObject *)gd, "clicked", reconfigure_clicked, data);
	gtk_object_set_data_full((GtkObject *)gd, "data", data, g_free);
	gtk_widget_show((GtkWidget *)gd);
	gtk_object_unref((GtkObject *)gui);
}
