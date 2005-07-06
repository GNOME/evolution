/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Vivek Jain <jvivek@novell.com>
 *
 *  Copyright 2005 Novell, Inc. (www.novell.com)
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
#include <stdlib.h>
#include <glib.h>
#include "em-junk-hook.h"
#include "mail-session.h"
#include <e-util/e-icon-factory.h>
#include <camel/camel-junk-plugin.h>
#include <glib/gi18n.h>

static GHashTable *emjh_types;
static GObjectClass *parent_class = NULL;

static void *emjh_parent_class;
static GObjectClass *emj_parent;
#define emjh ((EMJunkHook *)eph)

#define d(x)

static const EPluginHookTargetKey emjh_flag_map[] = {
	{ 0 }
};

/* ********************************************************************** */

/* Mail junk plugin */

/*
  <hook class="org.gnome.evolution.mail.junk:1.0">
  <group id="EMJunk">
     <item check_junk="sa_check_junk"
     	   report_junk="sa_report_junk"
	   report_non_junk="sa_report_non_junk"
	   commit_reports = "sa_commit_reports"/>
  </group>
  </hook>

*/

static void 
em_junk_init(CamelJunkPlugin *csp)
{
}

static const char *
em_junk_get_name (CamelJunkPlugin *csp)
{
	struct _EMJunkHookItem *item = (EMJunkHookItem *)csp;

	if (item->hook && item->hook->hook.plugin->enabled) {
		return (item->hook->hook.plugin->name);
	} else
		return 	_("None");

}

static gboolean 
em_junk_check_junk(CamelJunkPlugin *csp, CamelMimeMessage *m)
{
	struct _EMJunkHookItem *item = (EMJunkHookItem *)csp;

	if (item->hook && item->hook->hook.plugin->enabled) {
		EMJunkHookTarget target = {
			  m
		};

		return (gboolean)(e_plugin_invoke(item->hook->hook.plugin, item->check_junk, &target));
	}

	return FALSE;
}

static void 
em_junk_report_junk(CamelJunkPlugin *csp, CamelMimeMessage *m)
{
	struct _EMJunkHookItem *item = (EMJunkHookItem *)csp;

	if (item->hook && item->hook->hook.plugin->enabled) {
		EMJunkHookTarget target = {
			  m
		};

		e_plugin_invoke(item->hook->hook.plugin, item->report_junk, &target);
	}
}

static void 
em_junk_report_non_junk(CamelJunkPlugin *csp, CamelMimeMessage *m)
{
	struct _EMJunkHookItem *item = (EMJunkHookItem *)csp;

	if (item->hook && item->hook->hook.plugin->enabled) {
		EMJunkHookTarget target = {
			 m
		};
		e_plugin_invoke(item->hook->hook.plugin, item->report_non_junk, &target);
	}
}

static void 
em_junk_commit_reports(CamelJunkPlugin *csp)
{
	struct _EMJunkHookItem *item = (EMJunkHookItem *)csp;

	if (item->hook && item->hook->hook.plugin->enabled) 
		e_plugin_invoke(item->hook->hook.plugin, item->commit_reports, NULL);

}

static void 
emj_dispose (GObject *object)
{
	if (parent_class->dispose)
		parent_class->dispose (object);
}

static void 
emj_finalize (GObject *object)
{
	if (parent_class->finalize)
		parent_class->finalize (object);
}

static void
emjh_free_item(EMJunkHookItem *item)
{
	g_free (item->check_junk);
	g_free (item->report_junk);
	g_free (item->report_non_junk);
	g_free (item->commit_reports);
	g_free(item);
}

static void
emjh_free_group(EMJunkHookGroup *group)
{
	g_slist_foreach(group->items, (GFunc)emjh_free_item, NULL);
	g_slist_free(group->items);

	g_free(group->id);
	g_free(group);
}

static struct _EMJunkHookItem *
emjh_construct_item(EPluginHook *eph, EMJunkHookGroup *group, xmlNodePtr root)
{
	struct _EMJunkHookItem *item; 
	EMJunk junk_plugin = {
		{
			em_junk_get_name,
			1,
			em_junk_check_junk,
			em_junk_report_junk,
			em_junk_report_non_junk,
			em_junk_commit_reports,
			em_junk_init,
		}
	};

	d(printf("  loading group item\n"));
	item = g_malloc0(sizeof(*item));
	item->csp =  junk_plugin.csp;
	item->check_junk = e_plugin_xml_prop(root, "check_junk");
	item->report_junk = e_plugin_xml_prop(root, "report_junk");
	item->report_non_junk = e_plugin_xml_prop(root, "report_non_junk");
	item->commit_reports = e_plugin_xml_prop(root, "commit_reports");
	item->hook = emjh;
	
	if (item->check_junk == NULL || item->report_junk == NULL || item->report_non_junk == NULL || item->commit_reports == NULL)
		goto error;
	
	/* assign the plugin to the session*/
	session->junk_plugin = CAMEL_JUNK_PLUGIN (&(item->csp));
	
	return item;
error:
	printf ("ERROR");
	emjh_free_item (item);
	return NULL;
}

static struct _EMJunkHookGroup *
emjh_construct_group(EPluginHook *eph, xmlNodePtr root)
{
	struct _EMJunkHookGroup *group;
	xmlNodePtr node;

	d(printf(" loading group\n"));
	group = g_malloc0(sizeof(*group));

	group->id = e_plugin_xml_prop(root, "id");
	if (group->id == NULL)
		goto error;

	node = root->children;
	
	/* We'll processs only  the first item from xml file*/
	while (node) {
		if (0 == strcmp(node->name, "item")) {
			struct _EMJunkHookItem *item;

			item = emjh_construct_item(eph, group, node);
			if (item)
				group->items = g_slist_append(group->items, item);
			break;
		}
		
		node = node->next;
	}

	return group;
error:
	emjh_free_group(group);
	
	return NULL;
}

static int
emjh_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	static gboolean loaded = FALSE;
		
	d(printf("loading junk hook\n"));

	if (((EPluginHookClass *)emjh_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	if (loaded) {
		g_warning ("Can't load multiple plugins to this hook:ignored");
		return -1;
	}

	node = root->children;
	while (node) {
		if (strcmp(node->name, "group") == 0) {
			struct _EMJunkHookGroup *group;

			group = emjh_construct_group(eph, node);
			if (group) {
				emjh->groups = g_slist_append(emjh->groups, group);
			}
		}
		node = node->next;
	}
	
	eph->plugin = ep;
	loaded = TRUE;

	return 0;
}

/*XXX: don't think we need here*/
static void
emjh_enable(EPluginHook *eph, int state)
{
	GSList *g;
	
	g = emjh->groups;
	if (emjh_types == NULL)
		return;

}

static void
emjh_finalise(GObject *o)
{
	EPluginHook *eph = (EPluginHook *)o;

	g_slist_foreach(emjh->groups, (GFunc)emjh_free_group, NULL);
	g_slist_free(emjh->groups);

	((GObjectClass *)emjh_parent_class)->finalize(o);
}

static void
emjh_class_init(EPluginHookClass *klass)
{
	((GObjectClass *)klass)->finalize = emjh_finalise;
	klass->construct = emjh_construct;
	klass->enable = emjh_enable;
	klass->id = "org.gnome.evolution.mail.junk:1.0";
}

GType
em_junk_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMJunkHookClass), NULL, NULL, (GClassInitFunc) emjh_class_init, NULL, NULL,
			sizeof(EMJunkHook), 0, (GInstanceInitFunc) NULL,
		};

		emjh_parent_class = g_type_class_ref(e_plugin_hook_get_type());
		type = g_type_register_static(e_plugin_hook_get_type(), "EMJunkHook", &info, 0);
	}
	
	return type;
}

static void
emj_class_init (EMJunkClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
	object_class->dispose = emj_dispose;
	object_class->finalize = emj_finalize;
}

GType
emj_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMJunkClass), NULL, NULL, (GClassInitFunc) emj_class_init, NULL, NULL,
			sizeof(EMJunk), 0, (GInstanceInitFunc) NULL,
		};

		emj_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EMJunk", &info, 0);
	}
	
	return type;
}

void 
em_junk_hook_register_type(GType type)
{
	EMJunk *klass;

	if (emjh_types == NULL)
		emjh_types = g_hash_table_new(g_str_hash, g_str_equal);

	d(printf("registering junk plugin type '%s'\n", g_type_name(type)));

	klass = g_type_class_ref(type);
	g_hash_table_insert(emjh_types, (void *)g_type_name(type), klass);
}
