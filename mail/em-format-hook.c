/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
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

#include "em-format-hook.h"

#include <e-util/e-icon-factory.h>

#include <libgnome/gnome-i18n.h>


/* class name -> klass map for EMFormat and subclasses */
static GHashTable *emfh_types;

/* ********************************************************************** */

/* Mail formatter handler plugin */

/*
  <hook class="org.gnome.evolution.mail.format:1.0">
  <group id="EMFormatHTML">
     <item flags="inline,inline_disposition"
           mime_type="text/vcard"
	   format="format_vcard"/>
  </group>
  </hook>

*/

static void *emfh_parent_class;
#define emfh ((EMFormatHook *)eph)

static const EPluginHookTargetKey emfh_flag_map[] = {
	{ "inline", EM_FORMAT_HANDLER_INLINE },
	{ "inline_disposition", EM_FORMAT_HANDLER_INLINE_DISPOSITION },
	{ 0 }
};

static void
emfh_format_format(EMFormat *md, struct _CamelStream *stream, struct _CamelMimePart *part, const EMFormatHandler *info)
{
	struct _EMFormatHookItem *item = (EMFormatHookItem *)info;
	EMFormatHookTarget target = {
		md, stream, part, item
	};

	e_plugin_invoke(item->hook->hook.plugin, item->format, &target);
}

static void
emfh_free_item(struct _EMFormatHookItem *item)
{
	/* FIXME: remove from formatter class */

	g_free(item->handler.mime_type);
	g_free(item->format);
	g_free(item);
}

static void
emfh_free_group(struct _EMFormatHookGroup *group)
{
	g_slist_foreach(group->items, (GFunc)emfh_free_item, NULL);
	g_slist_free(group->items);

	g_free(group->id);
	g_free(group);
}

static struct _EMFormatHookItem *
emfh_construct_item(EPluginHook *eph, EMFormatHookGroup *group, xmlNodePtr root)
{
	struct _EMFormatHookItem *item;

	printf("  loading group item\n");
	item = g_malloc0(sizeof(*item));

	item->handler.mime_type = e_plugin_xml_prop(root, "mime_type");
	item->handler.flags = e_plugin_hook_mask(root, emfh_flag_map, "flags");
	item->format = e_plugin_xml_prop(root, "format");

	item->handler.handler = emfh_format_format;
	item->hook = emfh;

	if (item->handler.mime_type == NULL || item->format == NULL)
		goto error;

	printf("   type='%s' format='%s'\n", item->handler.mime_type, item->format);

	return item;
error:
	printf("error!\n");
	emfh_free_item(item);
	return NULL;
}

static struct _EMFormatHookGroup *
emfh_construct_group(EPluginHook *eph, xmlNodePtr root)
{
	struct _EMFormatHookGroup *group;
	xmlNodePtr node;

	printf(" loading group\n");
	group = g_malloc0(sizeof(*group));

	group->id = e_plugin_xml_prop(root, "id");
	if (group->id == NULL)
		goto error;

	node = root->children;
	while (node) {
		if (0 == strcmp(node->name, "item")) {
			struct _EMFormatHookItem *item;

			item = emfh_construct_item(eph, group, node);
			if (item)
				group->items = g_slist_append(group->items, item);
		}
		node = node->next;
	}

	return group;
error:
	emfh_free_group(group);
	return NULL;
}

static int
emfh_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;

	printf("loading format hook\n");

	if (((EPluginHookClass *)emfh_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	node = root->children;
	while (node) {
		if (strcmp(node->name, "group") == 0) {
			struct _EMFormatHookGroup *group;

			group = emfh_construct_group(eph, node);
			if (group) {
				EMFormatClass *klass;

				if (emfh_types
				    && (klass = g_hash_table_lookup(emfh_types, group->id))) {
					GSList *l = group->items;

					for (;l;l=g_slist_next(l)) {
						EMFormatHookItem *item = l->data;
						/* TODO: only add handlers if enabled? */
						em_format_class_add_handler(klass, &item->handler);
					}
				}
				/* We don't actually need to keep this around once its set on the class */
				emfh->groups = g_slist_append(emfh->groups, group);
			}
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
emfh_enable(EPluginHook *eph, int state)
{
	GSList *g, *l;
	EMFormatClass *klass;

	g = emfh->groups;
	if (emfh_types == NULL)
		return;

	for (;g;g=g_slist_next(g)) {
		struct _EMFormatHookGroup *group = g->data;

		klass = g_hash_table_lookup(emfh_types, group->id);
		for (l=group->items;l;l=g_slist_next(l)) {
			EMFormatHookItem *item = l->data;

			if (state)
				em_format_class_add_handler(klass, &item->handler);
			else
				em_format_class_remove_handler(klass, &item->handler);
		}
	}
}

static void
emfh_finalise(GObject *o)
{
	EPluginHook *eph = (EPluginHook *)o;

	g_slist_foreach(emfh->groups, (GFunc)emfh_free_group, NULL);
	g_slist_free(emfh->groups);

	((GObjectClass *)emfh_parent_class)->finalize(o);
}

static void
emfh_class_init(EPluginHookClass *klass)
{
	((GObjectClass *)klass)->finalize = emfh_finalise;
	klass->construct = emfh_construct;
	klass->enable = emfh_enable;
	klass->id = "org.gnome.evolution.mail.format:1.0";
}

GType
em_format_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMFormatHookClass), NULL, NULL, (GClassInitFunc) emfh_class_init, NULL, NULL,
			sizeof(EMFormatHook), 0, (GInstanceInitFunc) NULL,
		};

		emfh_parent_class = g_type_class_ref(e_plugin_hook_get_type());
		type = g_type_register_static(e_plugin_hook_get_type(), "EMFormatHook", &info, 0);
	}
	
	return type;
}

void em_format_hook_register_type(GType type)
{
	EMFormatClass *klass;

	if (emfh_types == NULL)
		emfh_types = g_hash_table_new(g_str_hash, g_str_equal);

	printf("registering formatter type '%s'\n", g_type_name(type));

	klass = g_type_class_ref(type);
	g_hash_table_insert(emfh_types, (void *)g_type_name(type), klass);
}
