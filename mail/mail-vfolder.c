/*
  Copyright 2000, 2001 Ximian Inc.

  Author: Michael Zucchi <notzed@ximian.com>

  code for managing vfolders

  NOTE: dont run this through fucking indent.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomeui/gnome-stock.h>

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"
#include "folder-browser.h"
#include "mail-vfolder.h"
#include "mail-tools.h"
#include "mail-autofilter.h"
#include "mail-folder-cache.h"
#include "mail.h"
#include "mail-ops.h"
#include "mail-mt.h"

#include "camel/camel.h"
#include "camel/camel-remote-store.h"
#include "camel/camel-vee-folder.h"

#include "filter/vfolder-context.h"
#include "filter/vfolder-editor.h"

#include "e-util/e-unicode-i18n.h"

#define d(x) x

struct _vfolder_info {
	char *name;
	char *query;
	FilterRule *rule;
	CamelVeeFolder *folder;
};

/* list of vfolders available */
static VfolderContext *context;
static CamelStore *vfolder_store;
static GList *source_folders;	/* list of source folders */

static GHashTable *vfolder_hash;

/* Ditto below */
EvolutionStorage *vfolder_storage;

extern EvolutionShellClient *global_shell_client;

/* more globals ... */
extern char *evolution_dir;
extern CamelSession *session;

/* ********************************************************************** */

/* return true if this folder should be added to this rule */
static int
check_source(FilterRule *rule, CamelFolder *folder)
{
	if (rule->source) {
		int remote = (((CamelService *)folder->parent_store)->provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0;
		
		if (!strcmp(rule->source, "local")) {
			if (!remote) {
				return TRUE;
			}
		} else if (!strcmp(rule->source, "remote_active")) {
			if (remote) {
				return TRUE;
			}
		} else if (!strcmp(rule->source, "local_remote_active")) {
			return TRUE;
		}
	}
	
	return FALSE;
}

static void
register_source(char *key, CamelVeeFolder *vfolder, CamelFolder *folder)
{
	FilterRule *rule;
	
	rule = rule_context_find_rule((RuleContext *)context, key, NULL);
	if (rule && check_source(rule, folder))
		camel_vee_folder_add_folder(vfolder, folder);
}

/* the source will never be finalised while a vfolder has it */
static void
source_finalise(CamelFolder *folder, void *event_data, void *data)
{
	source_folders = g_list_remove(source_folders, folder);
}

/* for registering potential vfolder sources */
void
vfolder_register_source (CamelFolder *folder)
{
	if (CAMEL_IS_VEE_FOLDER(folder))
		return;
	
	if (g_list_find(source_folders, folder))
		return;
	
	/* note that once we register a source, it will be ref'd
	   by our vfolder ... and wont go away with this, but we
	   do this so our source_folders list doesn't get stale */
	camel_object_hook_event((CamelObject *)folder, "finalize", (CamelObjectEventHookFunc)source_finalise, folder);
	
	source_folders = g_list_append(source_folders, folder);
	g_hash_table_foreach(vfolder_hash, (GHFunc)register_source, folder);
}

/* ********************************************************************** */

struct _setup_msg {
	struct _mail_msg msg;

	CamelFolder *folder;
	char *query;
	GList *sources_uri;
	GList *sources_folder;
};

static void
vfolder_setup_do(struct _mail_msg *mm)
{
	struct _setup_msg *m = (struct _setup_msg *)mm;
	GList *l, *list = NULL;
	CamelFolder *folder;

	camel_vee_folder_set_expression((CamelVeeFolder *)m->folder, m->query);

	l = m->sources_uri;
	while (l) {
		folder = mail_tool_uri_to_folder(l->data, &mm->ex);
		if (folder) {
			list = g_list_append(list, folder);
		} else {
			g_warning("Could not open vfolder source: %s", (char *)l->data);
			camel_exception_clear(&mm->ex);
		}
		l = l->next;
	}

	l = m->sources_folder;
	while (l) {
		camel_object_ref((CamelObject *)l->data);
		list = g_list_append(list, l->data);
		l = l->next;
	}

	camel_vee_folder_set_folders((CamelVeeFolder *)m->folder, list);

	l = list;
	while (l) {
		camel_object_unref((CamelObject *)l->data);
		l = l->next;
	}
	g_list_free(list);
}

static void
vfolder_setup_done(struct _mail_msg *mm)
{
	struct _setup_msg *m = (struct _setup_msg *)mm;

	m = m;
}

static void
vfolder_setup_free (struct _mail_msg *mm)
{
	struct _setup_msg *m = (struct _setup_msg *)mm;
	GList *l;

	camel_object_unref((CamelObject *)m->folder);
	g_free(m->query);

	l = m->sources_uri;
	while (l) {
		g_free(l->data);
		l = l->next;
	}
	g_list_free(m->sources_uri);

	l = m->sources_folder;
	while (l) {
		camel_object_unref(l->data);
		l = l->next;
	}
	g_list_free(m->sources_folder);
}

static struct _mail_msg_op vfolder_setup_op = {
	NULL,
	vfolder_setup_do,
	vfolder_setup_done,
	vfolder_setup_free,
};

static int
vfolder_setup(CamelFolder *folder, const char *query, GList *sources_uri, GList *sources_folder)
{
	struct _setup_msg *m;
	int id;
	
	m = mail_msg_new(&vfolder_setup_op, NULL, sizeof (*m));
	m->folder = folder;
	camel_object_ref((CamelObject *)folder);
	m->query = g_strdup(query);
	m->sources_uri = sources_uri;
	m->sources_folder = sources_folder;
	
	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);

	return id;
}

/* ********************************************************************** */

static void context_rule_added(RuleContext *ctx, FilterRule *rule);

static void
rule_changed(FilterRule *rule, CamelFolder *folder)
{
	const char *sourceuri;
	GList *l;
	GList *sources_uri = NULL, *sources_folder = NULL;
	GString *query;

	/* if the folder has changed name, then add it, then remove the old manually */
	if (strcmp(folder->full_name, rule->name) != 0) {
		char *path, *key;
		CamelFolder *old;

		gtk_signal_disconnect_by_func((GtkObject *)rule, rule_changed, folder);

		context_rule_added((RuleContext *)context, rule);

		/* TODO: remove folder from folder info cache? */

		path = g_strdup_printf("/%s", folder->full_name);
		evolution_storage_removed_folder(mail_lookup_storage(vfolder_store), path);
		g_free(path);

		if (g_hash_table_lookup_extended(vfolder_hash, folder->full_name, (void **)&key, (void **)&old)) {
			g_hash_table_remove(vfolder_hash, key);
			g_free(key);
			camel_object_unref((CamelObject *)folder);
		} else {
			g_warning("couldn't find a vfolder rule in our table? %s", folder->full_name);
		}

		return;
	}

	printf("Filter rule changed? for folder '%s'!!\n", folder->name);

	/* work out the work to do, then do it in another thread */
	sourceuri = NULL;
	while ( (sourceuri = vfolder_rule_next_source((VfolderRule *)rule, sourceuri)) ) {
		sources_uri = g_list_append(sources_uri, g_strdup(sourceuri));
	}
	
	l = source_folders;
	while (l) {
		if (check_source(rule, l->data)) {
			camel_object_ref(l->data);
			sources_folder = g_list_append(sources_folder, l->data);
		}
		l = l->next;
	}

	query = g_string_new("");
	filter_rule_build_code(rule, query);

	vfolder_setup(folder, query->str, sources_uri, sources_folder);

	g_string_free(query, TRUE);
}

static void context_rule_added(RuleContext *ctx, FilterRule *rule)
{
	CamelFolder *folder;

	printf("rule added: %s\n", rule->name);

	/* this always runs quickly */
	folder = camel_store_get_folder(vfolder_store, rule->name, 0, NULL);
	if (folder) {
		gtk_signal_connect((GtkObject *)rule, "changed", rule_changed, folder);

		g_hash_table_insert(vfolder_hash, g_strdup(rule->name), folder);

		mail_note_folder(folder, NULL);
		rule_changed(rule, folder);
	}
}

static void context_rule_removed(RuleContext *ctx, FilterRule *rule)
{
	char *key, *path;
	CamelFolder *folder;

	printf("rule removed; %s\n", rule->name);

	/* TODO: remove from folder info cache? */

	path = g_strdup_printf("/%s", rule->name);
	evolution_storage_removed_folder(mail_lookup_storage(vfolder_store), path);
	g_free(path);

	if (g_hash_table_lookup_extended(vfolder_hash, rule->name, (void **)&key, (void **)&folder)) {
		g_hash_table_remove(vfolder_hash, key);
		g_free(key);
		camel_object_unref((CamelObject *)folder);
	}

	camel_store_delete_folder(vfolder_store, rule->name, NULL);
}

static void
store_folder_created(CamelObject *o, void *event_data, void *data)
{
	CamelStore *store = (CamelStore *)o;
	CamelFolderInfo *info = event_data;

	store = store;
	info = info;
}

static void
store_folder_deleted(CamelObject *o, void *event_data, void *data)
{
	CamelStore *store = (CamelStore *)o;
	CamelFolderInfo *info = event_data;
	FilterRule *rule;
	char *user;

	printf("Folder deleted: %s\n", info->name);
	store = store;

	/* delete it from our list */
	rule = rule_context_find_rule((RuleContext *)context, info->name, NULL);
	if (rule) {
		/* We need to stop listening to removed events, otherwise we'll try and remove it again */
		gtk_signal_disconnect_by_func((GtkObject *)context, context_rule_removed, context);
		rule_context_remove_rule((RuleContext *)context, rule);
		gtk_object_unref((GtkObject *)rule);
		gtk_signal_connect((GtkObject *)context, "rule_removed", context_rule_removed, context);

		user = g_strdup_printf("%s/vfolders.xml", evolution_dir);
		rule_context_save((RuleContext *)context, user);
		g_free(rule);
	} else {
		g_warning("Cannot find rule for deleted vfolder '%s'", info->name);
	}
}

void
vfolder_load_storage(GNOME_Evolution_Shell shell)
{
	char *user, *storeuri;
	FilterRule *rule;

	vfolder_hash = g_hash_table_new(g_str_hash, g_str_equal);

	/* first, create the vfolder store, and set it up */
	storeuri = g_strdup_printf("vfolder:%s/vfolder", evolution_dir);
	vfolder_store = camel_session_get_store(session, storeuri, NULL);
	if (vfolder_store == NULL) {
		g_warning("Cannot open vfolder store - no vfolders available");
		return;
	}

	camel_object_hook_event((CamelObject *)vfolder_store, "folder_created",
				(CamelObjectEventHookFunc)store_folder_created, NULL);
	camel_object_hook_event((CamelObject *)vfolder_store, "folder_deleted",
				(CamelObjectEventHookFunc)store_folder_deleted, NULL);

	printf("got store '%s' = %p\n", storeuri, vfolder_store);
	mail_load_storage_by_uri(shell, storeuri, U_("VFolders"));

	/* load our rules */
	user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
	context = vfolder_context_new ();
	if (rule_context_load ((RuleContext *)context, EVOLUTION_DATADIR "/evolution/vfoldertypes.xml", user) != 0) {
		g_warning("cannot load vfolders: %s\n", ((RuleContext *)context)->error);
	}
	g_free (user);
	
	gtk_signal_connect((GtkObject *)context, "rule_added", context_rule_added, context);
	gtk_signal_connect((GtkObject *)context, "rule_removed", context_rule_removed, context);

	/* and setup the rules we have */
	rule = NULL;
	while ( (rule = rule_context_next_rule((RuleContext *)context, rule, NULL)) ) {
		context_rule_added((RuleContext *)context, rule);
	}

	g_free(storeuri);
}

static GtkWidget *vfolder_editor = NULL;

static void
vfolder_editor_destroy (GtkWidget *widget, gpointer user_data)
{
	vfolder_editor = NULL;
}

static void
vfolder_editor_clicked (GtkWidget *dialog, int button, void *data)
{
	if (button == 0) {
		char *user;

		user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
		rule_context_save ((RuleContext *)context, user);
		g_free (user);
	}
	if (button != -1) {
		gnome_dialog_close (GNOME_DIALOG (dialog));
	}
}

void
vfolder_edit (void)
{
	if (vfolder_editor) {
		gdk_window_raise (GTK_WIDGET (vfolder_editor)->window);
		return;
	}
	
	vfolder_editor = GTK_WIDGET (vfolder_editor_new (context));
	gtk_signal_connect (GTK_OBJECT (vfolder_editor), "clicked", vfolder_editor_clicked, NULL);
	gtk_signal_connect (GTK_OBJECT (vfolder_editor), "destroy", vfolder_editor_destroy, NULL);
	gtk_widget_show (vfolder_editor);
}

static void
new_rule_clicked(GtkWidget *w, int button, void *data)
{
	if (button == 0) {
		char *user;
		FilterRule *rule = gtk_object_get_data((GtkObject *)w, "rule");

		gtk_object_ref((GtkObject *)rule);
		rule_context_add_rule((RuleContext *)context, rule);
		user = g_strdup_printf("%s/vfolders.xml", evolution_dir);
		rule_context_save((RuleContext *)context, user);
		g_free(user);
	}
	if (button != -1) {
		gnome_dialog_close((GnomeDialog *)w);
	}
}

FilterPart *
vfolder_create_part(const char *name)
{
	return rule_context_create_part((RuleContext *)context, name);
}

/* clones a filter/search rule into a matching vfolder rule (assuming the same system definitions) */
FilterRule *
vfolder_clone_rule(FilterRule *in)
{
	FilterRule *rule = (FilterRule *)vfolder_rule_new();
	xmlNodePtr xml;

	xml = filter_rule_xml_encode(in);
	filter_rule_xml_decode(rule, xml, (RuleContext *)context);
	xmlFreeNodeList(xml);

	return rule;
}

/* adds a rule with a gui */
void
vfolder_gui_add_rule(VfolderRule *rule)
{
	GtkWidget *w;
	GnomeDialog *gd;

	w = filter_rule_get_widget((FilterRule *)rule, (RuleContext *)context);

	gd = (GnomeDialog *)gnome_dialog_new(_("New VFolder"),
					     GNOME_STOCK_BUTTON_OK,
					     GNOME_STOCK_BUTTON_CANCEL,
					     NULL);
	gnome_dialog_set_default (gd, 0);

	gtk_window_set_policy(GTK_WINDOW(gd), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (gd), 500, 500);
	gtk_box_pack_start((GtkBox *)gd->vbox, w, TRUE, TRUE, 0);
	gtk_widget_show((GtkWidget *)gd);
	gtk_object_set_data_full((GtkObject *)gd, "rule", rule, (GtkDestroyNotify)gtk_object_unref);
	gtk_signal_connect((GtkObject *)gd, "clicked", new_rule_clicked, NULL);
	gtk_widget_show((GtkWidget *)gd);
}

void
vfolder_gui_add_from_message(CamelMimeMessage *msg, int flags, const char *source)
{
	VfolderRule *rule;

	g_return_if_fail (msg != NULL);

	rule = (VfolderRule*)vfolder_rule_from_message(context, msg, flags, source);
	vfolder_gui_add_rule(rule);
}

void
vfolder_gui_add_from_mlist(CamelMimeMessage *msg, const char *mlist, const char *source)
{
	VfolderRule *rule;

	g_return_if_fail (msg != NULL);

	rule = (VfolderRule*)vfolder_rule_from_mlist(context, mlist, source);
	vfolder_gui_add_rule(rule);
}

EvolutionStorage *
mail_vfolder_get_vfolder_storage (void)
{
	return mail_lookup_storage(vfolder_store);
}
