/*
  Copyright 2000, 2001 Helix Code Inc.

  Author: Michael Zucchi <notzed@helixcode.com>

  code for managing vfolders

  NOTE: dont run this through fucking indent.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <bonobo.h>

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"
#include "folder-browser.h"
#include "mail-vfolder.h"
#include "mail-tools.h"
#include "mail-autofilter.h"
#include "mail.h"

#include "camel/camel.h"
#include "camel/camel-remote-store.h"
#include "camel/camel-vee-folder.h"

#include "filter/vfolder-context.h"
#include "filter/vfolder-editor.h"

#define d(x) x

struct _vfolder_info {
	char *name;
	char *query;
	FilterRule *rule;
	CamelVeeFolder *folder;
};

/* list of vfolders available */
static GList *available_vfolders = NULL;
static VfolderContext *context;
static GList *source_folders;	/* list of source folders */

/* Ditto below */
EvolutionStorage *vfolder_storage;

/* GROSS HACK: for passing to other parts of the program */
EvolutionShellClient *global_shell_client = NULL;

/* more globals ... */
extern char *evolution_dir;
extern CamelSession *session;

static struct _vfolder_info *
vfolder_find(const char *name)
{
	GList *l = available_vfolders;
	struct _vfolder_info *info;

	while (l) {
		info = l->data;
		if (!strcmp(info->name, name))
			return info;
		l = g_list_next(l);
	}
	return NULL;
}

static void
register_new_source(struct _vfolder_info *info, CamelFolder *folder)
{
	FilterRule *rule = info->rule;

	if (rule && info->folder && rule->source) {
		int remote = (((CamelService *)folder->parent_store)->provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0;

		if (!strcmp(rule->source, "local")) {
			if (!remote) {
				printf("adding local folder to vfolder %s\n", rule->name);
				camel_vee_folder_add_folder(info->folder, folder);
			}
		} else if (!strcmp(rule->source, "remote_active")) {
			if (remote) {
				printf("adding remote folder to vfolder %s\n", rule->name);
				camel_vee_folder_add_folder(info->folder, folder);
			}
		} else if (!strcmp(rule->source, "local_remote_active")) {
			printf("adding local or remote folder to vfolder %s\n", rule->name);
			camel_vee_folder_add_folder(info->folder, folder);
		}
	}
}

static void source_finalise(CamelFolder *sub, gpointer type, CamelFolder *vf)
{
	GList *l = available_vfolders;
	
	while (l) {
		struct _vfolder_info *info = l->data;

		if (info->folder)
			camel_vee_folder_remove_folder(info->folder, sub);

		l = l->next;
	}
}

/* for registering potential vfolder sources */
void vfolder_register_source(CamelFolder *folder)
{
	GList *l;

	if (CAMEL_IS_VEE_FOLDER(folder))
		return;

	if (g_list_find(source_folders, folder))
		return;

	/* FIXME: Hook to destroy event */
	camel_object_hook_event((CamelObject *)folder, "finalize", (CamelObjectEventHookFunc)source_finalise, folder);

	source_folders = g_list_append(source_folders, folder);
	l = available_vfolders;
	while (l) {
		register_new_source(l->data, folder);
		l = l->next;
	}
}

/* go through the list of what we have, what we want, and make
   them match, deleting/reconfiguring as required */
static void
vfolder_refresh(void)
{
	GList *l;
	GList *head = NULL;	/* processed list */
	struct _vfolder_info *info;
	FilterRule *rule;
	GString *expr = g_string_new("");
	char *uri, *path;

	rule = NULL;
	while ( (rule = rule_context_next_rule((RuleContext *)context, rule, NULL)) ) {
		info = vfolder_find(rule->name);
		g_string_truncate(expr, 0);
		filter_rule_build_code(rule, expr);
		if (info) {
			gtk_object_ref((GtkObject *)rule);
			if (info->rule)
				gtk_object_unref((GtkObject *)info->rule);
			info->rule = rule;

			available_vfolders = g_list_remove(available_vfolders, info);

			/* check if the rule has changed ... otherwise, leave it */
			if (strcmp(expr->str, info->query)) {
				d(printf("Must reconfigure vfolder with new rule?\n"));
				g_free(info->query);
				info->query = g_strdup(expr->str);

				uri = g_strdup_printf("vfolder:%s", info->name);
				path = g_strdup_printf("/%s", info->name);
				evolution_storage_removed_folder(vfolder_storage, path);
				evolution_storage_new_folder(vfolder_storage, path, g_basename(path),
							     "mail", uri, info->name, FALSE);
				g_free(uri);
				g_free(path);
			}
		} else {
			info = g_malloc(sizeof(*info));
			info->name = g_strdup(rule->name);
			info->query = g_strdup(expr->str);
			gtk_object_ref((GtkObject *)rule);
			info->rule = rule;
			info->folder = NULL;
			d(printf("Adding new vfolder: %s %s\n", rule->name, expr->str));
			
			uri = g_strdup_printf("vfolder:%s", info->name);
			path = g_strdup_printf("/%s", info->name);
			evolution_storage_new_folder(vfolder_storage, path, g_basename(path),
						     "mail", uri, info->name, FALSE);
			g_free(uri);
			g_free(path);
		}
		head = g_list_append(head, info);
	}
	/* everything in available_vfolders are to be removed ... */
	l = available_vfolders;
	while (l) {
		info = l->data;
		d(printf("removing vfolders %s %s\n", info->name, info->query));
		path = g_strdup_printf("/%s", info->name);
		evolution_storage_removed_folder(vfolder_storage, path);
		g_free(path);
		g_free(info->name);
		g_free(info->query);
		gtk_object_unref((GtkObject *)info->rule);
		g_free(info);
		l = g_list_next(l);
	}

	/* setup the virtual unmatched folder */
	info = vfolder_find("UNMATCHED");
	if (info == NULL) {
		char *uri, *path;

		info = g_malloc(sizeof(*info));
		info->name = g_strdup("UNMATCHED");
		info->query = g_strdup("UNMATCHED");
		info->rule = NULL;
		info->folder = NULL;
		d(printf("Adding new vfolder: %s %s\n", info->name, info->query));
		
		uri = g_strdup_printf("vfolder:%s", info->name);
		path = g_strdup_printf("/%s", info->name);
		evolution_storage_new_folder(vfolder_storage, path, g_basename(path),
					     "mail", uri, info->name, FALSE);
		g_free(uri);
		g_free(path);
	}
	head = g_list_append(head, info);

	g_list_free(available_vfolders);
	available_vfolders = head;
	g_string_free(expr, TRUE);
}

void
vfolder_create_storage(EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell corba_shell;
	EvolutionStorage *storage;
	char *user, *system;

	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == NULL) {
		g_warning ("We have no shell!?");
		return;
	}
	global_shell_client = shell_client;

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
    
	storage = evolution_storage_new (_("VFolders"), NULL, NULL);
	if (evolution_storage_register_on_shell (storage, corba_shell) != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage");
		return;
	}

	vfolder_storage = storage;

	user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
	system = g_strdup_printf("%s/evolution/vfoldertypes.xml", EVOLUTION_DATADIR);
	
	context = vfolder_context_new();
	printf("loading rules %s %s\n", system, user);
	if (rule_context_load((RuleContext *)context, system, user) != 0) {
		g_warning("cannot load vfolders: %s\n", ((RuleContext *)context)->error);
	}
	g_free(user);
	g_free(system);
	vfolder_refresh();
}

/* maps the shell's uri to the real vfolder uri and open the folder */
CamelFolder *
vfolder_uri_to_folder(const char *uri, CamelException *ex)
{
	struct _vfolder_info *info;
	char *storeuri, *foldername;
	VfolderRule *rule;
	CamelFolder *folder = NULL, *sourcefolder;
	const char *sourceuri;
	int sources;
	GList *l;

	if (strncmp (uri, "vfolder:", 8))
		return NULL;

	info = vfolder_find(uri+8);
	if (info == NULL) {
		g_warning("Shell trying to open unknown vFolder: %s", uri);
		return NULL;
	}

	if (info->folder) {
		camel_object_ref((CamelObject *)info->folder);
		return (CamelFolder *)info->folder;
	}

	d(printf("Opening vfolder: %s\n", uri));

	rule = (VfolderRule *)rule_context_find_rule((RuleContext *)context, info->name, NULL);

	storeuri = g_strdup_printf("vfolder:%s/vfolder/%s", evolution_dir, info->name);
	foldername = g_strdup_printf("%s?%s", info->name, info->query);

	/* we dont have indexing on vfolders */
	folder = mail_tool_get_folder_from_urlname (storeuri, foldername, CAMEL_STORE_FOLDER_CREATE, ex);
	info->folder = (CamelVeeFolder *)folder;

	bonobo_object_ref (BONOBO_OBJECT (vfolder_storage));
	mail_hash_storage ((CamelService *)folder->parent_store, vfolder_storage);

	if (strcmp(uri+8, "UNMATCHED") != 0) {
		sourceuri = NULL;
		sources = 0;
		while ( (sourceuri = vfolder_rule_next_source(rule, sourceuri)) ) {
			d(printf("adding vfolder source: %s\n", sourceuri));
			sourcefolder = mail_tool_uri_to_folder (sourceuri, ex);
			printf("source folder = %p\n", sourcefolder);
			if (sourcefolder) {
				sources++;
				camel_vee_folder_add_folder((CamelVeeFolder *)folder, sourcefolder);
			} else {
				/* we'll just silently ignore now-missing sources */
				camel_exception_clear(ex);
			}
		}

		l = source_folders;
		while (l) {
			register_new_source(info, l->data);
			l = l->next;
		}
#if 0
		/* if we didn't have any sources, just use Inbox as the default */
		if (sources == 0) {
			char *defaulturi;
			
			defaulturi = g_strdup_printf("file://%s/local/Inbox", evolution_dir);
			d(printf("No sources configured/found, using default: %s\n", defaulturi));
			sourcefolder = mail_tool_uri_to_folder (defaulturi, ex);
			g_free(defaulturi);
			if (sourcefolder) {
				camel_vee_folder_add_folder(folder, sourcefolder);
			}
		}
#endif
	}

	g_free(foldername);
	g_free(storeuri);

	return folder;
}

static void
vfolder_editor_clicked(GtkWidget *w, int button, void *data)
{
	if (button == 0) {
		char *user;

		user = g_strdup_printf("%s/vfolders.xml", evolution_dir);
		rule_context_save((RuleContext *)context, user);
		g_free(user);
		vfolder_refresh();
	}
	if (button != -1) {
		gnome_dialog_close((GnomeDialog *)w);
	}
}

void
vfolder_edit(void)
{
	GtkWidget *w;

	w = vfolder_editor_new(context);
	gtk_signal_connect((GtkObject *)w, "clicked", vfolder_editor_clicked, NULL);
	gtk_widget_show(w);
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
		vfolder_refresh();
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
	gtk_object_ref (GTK_OBJECT (vfolder_storage));
	return vfolder_storage;
}
