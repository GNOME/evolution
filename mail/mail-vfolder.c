/*
  Copyright 2000 Helix Code Inc.

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

#include "filter/vfolder-context.h"
#include "filter/filter-rule.h"
#include "filter/vfolder-editor.h"

struct _vfolder_info {
	char *name;
	char *query;
};

/* list of vfolders available */
static GList *available_vfolders = NULL;
static VfolderContext *context;
static EvolutionStorage *vfolder_storage;

/* GROSS HACK: for passing to other parts of the program */
EvolutionShellClient *global_shell_client = NULL;
extern char *evolution_dir;

static struct _vfolder_info *
vfolder_find(char *name)
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
	while ( (rule = rule_context_next_rule((RuleContext *)context, rule)) ) {
		info = vfolder_find(rule->name);
		g_string_truncate(expr, 0);
		filter_rule_build_code(rule, expr);
		if (info) {
			available_vfolders = g_list_remove(available_vfolders, info);

			/* check if the rule has changed ... otherwise, leave it */
			if (strcmp(expr->str, info->query)) {
				printf("Must reconfigure vfolder with new rule?\n");
				g_free(info->query);
				info->query = g_strdup(expr->str);

				uri = g_strdup_printf("vfolder:%s/vfolder/%s?%s", evolution_dir, info->name, info->query);
				path = g_strdup_printf("/%s", info->name);
				evolution_storage_removed_folder(vfolder_storage, path);
				evolution_storage_new_folder (vfolder_storage, path,
							      "mail",
							      uri,
							      info->name);
				g_free(uri);
				g_free(path);
			}
		} else {
			info = g_malloc(sizeof(*info));
			info->name = g_strdup(rule->name);
			info->query = g_strdup(expr->str);
			printf("Adding new vfolder: %s %s\n", rule->name, expr->str);
			
			uri = g_strdup_printf("vfolder:%s/vfolder/%s?%s", evolution_dir, info->name, info->query);
			path = g_strdup_printf("/%s", info->name);
			evolution_storage_new_folder (vfolder_storage, path,
						      "mail",
						      uri,
						      info->name);
			g_free(uri);
			g_free(path);
		}
		head = g_list_append(head, info);
	}
	/* everything in available_vfolders are to be removed ... */
	l = available_vfolders;
	while (l) {
		info = l->data;
		printf("removing vfolders %s %s\n", info->name, info->query);
		path = g_strdup_printf("/%s", info->name);
		evolution_storage_removed_folder(vfolder_storage, path);
		g_free(path);
		g_free(info->name);
		g_free(info->query);
		l = g_list_next(l);
	}
	g_list_free(available_vfolders);
	available_vfolders = head;
	g_string_free(expr, TRUE);
}

void
vfolder_create_storage(EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	Evolution_Shell corba_shell;
	EvolutionStorage *storage;
	char *user, *system;

	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == NULL) {
		g_warning ("We have no shell!?");
		return;
	}
	global_shell_client = shell_client;

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
    
	storage = evolution_storage_new ("VFolders");
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

void
vfolder_edit(void)
{
	GtkWidget *w;
	char *user;

	w = vfolder_editor_construct(context);
	if (gnome_dialog_run_and_close((GnomeDialog *)w) == 0) {
		user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
		rule_context_save(context, user);
		g_free(user);
		vfolder_refresh();
	}
}
