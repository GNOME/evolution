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

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>

#include "e-event.h"

#include <e-util/e-icon-factory.h>

#include <libgnome/gnome-i18n.h>

#define d(x)

struct _EEventFactory {
	struct _EEventFactory *next, *prev;

	char *menuid;
	EEventFactoryFunc factory;
	void *factory_data;
};

struct _event_node {
	struct _event_node *next, *prev;

	GSList *events;
	void *data;
	EEventItemsFunc freefunc;
};

struct _event_info {
	struct _event_node *parent;
	EEventItem *item;
};

struct _EEventPrivate {
	EDList events;

	GSList *sorted;		/* sorted list of struct _event_info's */
};

static GObjectClass *ep_parent;

static void
ep_init(GObject *o)
{
	EEvent *emp = (EEvent *)o;
	struct _EEventPrivate *p;

	p = emp->priv = g_malloc0(sizeof(struct _EEventPrivate));

	e_dlist_init(&p->events);
}

static void
ep_finalise(GObject *o)
{
	EEvent *emp = (EEvent *)o;
	struct _EEventPrivate *p = emp->priv;
	struct _event_node *node;

	if (emp->target)
		e_event_target_free(emp, emp->target);

	g_free(emp->id);

	while ((node = (struct _event_node *)e_dlist_remhead(&p->events))) {
		if (node->freefunc)
			node->freefunc(emp, node->events, node->data);

		g_free(node);
	}

	g_slist_foreach(p->sorted, (GFunc)g_free, NULL);
	g_slist_free(p->sorted);

	g_free(p);

	((GObjectClass *)ep_parent)->finalize(o);
}

static void
ep_target_free(EEvent *ep, EEventTarget *t)
{
	g_free(t);
	g_object_unref(ep);
}

static void
ep_class_init(GObjectClass *klass)
{
	d(printf("EEvent class init %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type)));

	klass->finalize = ep_finalise;
	((EEventClass *)klass)->target_free = ep_target_free;
}

/**
 * e_event_get_type:
 * 
 * Standard GObject type function.  Used to subclass EEvent.
 * 
 * Return value: The EEvent type.
 **/
GType
e_event_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EEventClass),
			(GBaseInitFunc)NULL, NULL,
			(GClassInitFunc)ep_class_init, NULL, NULL,
			sizeof(EEvent), 0,
			(GInstanceInitFunc)ep_init
		};
		ep_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EEvent", &info, 0);
	}

	return type;
}

/**
 * e_event_construct:
 * @ep: An instantiated but uninitialised EEvent.
 * @id: Event manager id.
 * 
 * Construct the base event instance with standard parameters.
 * 
 * Return value: Returns @ep.
 **/
EEvent *e_event_construct(EEvent *ep, const char *id)
{
	ep->id = g_strdup(id);

	return ep;
}

/**
 * e_event_add_items:
 * @emp: An initialised EEvent structure.
 * @items: A list of EEventItems event listeners to register on this event manager.
 * @freefunc: A function called when the @items list is no longer needed.
 * @data: callback data for @freefunc and for item event handlers.
 * 
 * Adds @items to the list of events listened to on the event manager @emp.
 *
 * Return value: An opaque key which can later be passed to remove_items.
 **/
void *
e_event_add_items(EEvent *emp, GSList *items, EEventItemsFunc freefunc, void *data)
{
	struct _event_node *node;

	node = g_malloc(sizeof(*node));
	node->events = items;
	node->freefunc = freefunc;
	node->data = data;
	e_dlist_addtail(&emp->priv->events, (EDListNode *)node);

	if (emp->priv->sorted) {
		g_slist_foreach(emp->priv->sorted, (GFunc)g_free, NULL);
		g_slist_free(emp->priv->sorted);
		emp->priv->sorted = NULL;
	}

	return (void *)node;
}

/**
 * e_event_remove_items:
 * @emp: 
 * @handle: 
 * 
 * Remove items previously added.  They MUST have been previously
 * added, and may only be removed once.
 **/
void
e_event_remove_items(EEvent *emp, void *handle)
{
	struct _event_node *node = handle;

	e_dlist_remove((EDListNode *)node);
	if (node->freefunc)
		node->freefunc(emp, node->events, node->data);
	g_free(node);

	if (emp->priv->sorted) {
		g_slist_foreach(emp->priv->sorted, (GFunc)g_free, NULL);
		g_slist_free(emp->priv->sorted);
		emp->priv->sorted = NULL;
	}	
}

static int
ee_cmp(const void *ap, const void *bp)
{
	int a = ((struct _event_info **)ap)[0]->item->priority;
	int b = ((struct _event_info **)bp)[0]->item->priority;

	if (a < b)
		return 1;
	else if (a > b)
		return -1;
	else
		return 0;
}

/**
 * e_event_emit:
 * @ee: An initialised EEvent, potentially with registered event listeners.
 * @id: Event name.  This will be compared against EEventItem.id.
 * @target: The target describing the event context.  This will be implementation defined.
 * 
 * Emit an event.  @target will automatically be freed once its
 * emission is complete.
 **/
void
e_event_emit(EEvent *emp, const char *id, EEventTarget *target)
{
	struct _EEventPrivate *p = emp->priv;
	GSList *events;

	d(printf("emit event %s\n", id));

	g_assert(emp->target == NULL);

	emp->target = target;
	events = p->sorted;
	if (events == NULL) {
		struct _event_node *node = (struct _event_node *)p->events.head;

		for (;node->next;node=node->next) {
			GSList *l = node->events;
			
			for (;l;l=g_slist_next(l)) {
				struct _event_info *info;

				info = g_malloc0(sizeof(*info));
				info->parent = node;
				info->item = l->data;
				events = g_slist_prepend(events, info);
			}
		}

		p->sorted = events = g_slist_sort(events, ee_cmp);
	}

	for (;events;events=g_slist_next(events)) {
		struct _event_info *info = events->data;
		EEventItem *event = info->item;

		d(printf("event '%s' mask %08x target %08x\n", event->id, event->enable, target->mask));

		if (event->enable & target->mask)
			continue;

		if (strcmp(event->id, id) == 0) {
			event->handle(emp, event, info->parent->data);

			if (event->type == E_EVENT_SINK)
				break;
		}
	}

	e_event_target_free(emp, target);
	emp->target = NULL;
}

/**
 * e_event_target_new:
 * @ep: An initialised EEvent instance.
 * @type: type, up to implementor
 * @size: The size of memory to allocate.  This must be >= sizeof(EEventTarget).
 * 
 * Allocate a new event target suitable for this class.  It is up to
 * the implementation to define the available target types and their
 * structure.
 **/
void *e_event_target_new(EEvent *ep, int type, size_t size)
{
	EEventTarget *t;

	g_assert(size >= sizeof(EEventTarget));

	t = g_malloc0(size);
	t->event = ep;
	g_object_ref(ep);
	t->type = type;

	return t;
}

/**
 * e_event_target_free:
 * @ep: An initialised EEvent instance on which this target was allocated.
 * @o: The target to free.
 * 
 * Free a target.  This invokes the virtual free method on the EEventClass.
 **/
void
e_event_target_free(EEvent *ep, void *o)
{
	EEventTarget *t = o;

	((EEventClass *)G_OBJECT_GET_CLASS(ep))->target_free(ep, t);
}

/* ********************************************************************** */

/* Event menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.event:1.0"
  id="org.gnome.mail.plugin.event.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.eventMenu:1.0"
        handler="HandleEvent">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="ep_view_emacs"/>
  </menu>
  </extension>

  <hook class="org.gnome.evolution.mail.events:1.0">
  <event id=".folder.changed"
  target=""
    priority="0"
    handle="gotevent"
    enable="new"
    />
  <event id=".message.read"
    priority="0"
    handle="gotevent"
    mask="new"
    />
  </hook>

*/

static void *emph_parent_class;
#define emph ((EEventHook *)eph)

/* must have 1:1 correspondence with e-event types in order */
static const EPluginHookTargetKey emph_item_types[] = {
	{ "pass", E_EVENT_PASS },
	{ "sink", E_EVENT_SINK },
	{ 0 }
};

static void
emph_event_handle(EEvent *ee, EEventItem *item, void *data)
{
	struct _EEventHook *hook = data;

	/* FIXME: we could/should just remove the items we added to the event handler */
	if (!hook->hook.plugin->enabled)
		return;

	e_plugin_invoke(hook->hook.plugin, (char *)item->user_data, ee->target);
}

static void
emph_free_item(struct _EEventItem *item)
{
	g_free((char *)item->id);
	g_free(item->user_data);
	g_free(item);
}

static void
emph_free_items(EEvent *ee, GSList *items, void *data)
{
	/*EPluginHook *eph = data;*/

	g_slist_foreach(items, (GFunc)emph_free_item, NULL);
	g_slist_free(items);
}

static struct _EEventItem *
emph_construct_item(EPluginHook *eph, xmlNodePtr root, EEventHookClass *klass)
{
	struct _EEventItem *item;
	EEventHookTargetMap *map;
	char *tmp;

	item = g_malloc0(sizeof(*item));

	tmp = xmlGetProp(root, "target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup(klass->target_map, tmp);
	xmlFree(tmp);
	if (map == NULL)
		goto error;
	item->target_type = map->id;
	item->type = e_plugin_hook_id(root, emph_item_types, "type");
	if (item->type == -1)
		item->type = E_EVENT_PASS;
	item->priority = e_plugin_xml_int(root, "priority", 0);
	item->id = e_plugin_xml_prop(root, "id");
	item->enable = e_plugin_hook_mask(root, map->mask_bits, "enable");
	item->user_data = e_plugin_xml_prop(root, "handle");

	if (item->user_data == NULL || item->id == NULL)
		goto error;

	item->handle = emph_event_handle;

	return item;
error:
	emph_free_item(item);
	return NULL;
}

static int
emph_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	EEventHookClass *klass;
	GSList *items = NULL;

	g_return_val_if_fail(((EEventHookClass *)G_OBJECT_GET_CLASS(eph))->event != NULL, -1);

	d(printf("loading event hook\n"));

	if (((EPluginHookClass *)emph_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	klass = (EEventHookClass *)G_OBJECT_GET_CLASS(eph);

	node = root->children;
	while (node) {
		if (strcmp(node->name, "event") == 0) {
			struct _EEventItem *item;

			item = emph_construct_item(eph, node, klass);
			if (item)
				items = g_slist_prepend(items, item);
		}
		node = node->next;
	}

	eph->plugin = ep;

	if (items)
		e_event_add_items(klass->event, items, emph_free_items, eph);

	return 0;
}

static void
emph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)emph_parent_class)->finalize(o);
}

static void
emph_class_init(EPluginHookClass *klass)
{
	((GObjectClass *)klass)->finalize = emph_finalise;
	klass->construct = emph_construct;

	/* this is actually an abstract implementation but list it anyway */
	klass->id = "org.gnome.evolution.event:1.0";

	d(printf("EEventHook: init class %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type)));

	((EEventHookClass *)klass)->target_map = g_hash_table_new(g_str_hash, g_str_equal);
}

/**
 * e_event_hook_get_type:
 * 
 * Standard GObject function to get the EEvent object type.  Used to
 * subclass EEventHook.
 * 
 * Return value: The type of the event hook class.
 **/
GType
e_event_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EEventHookClass), NULL, NULL, (GClassInitFunc) emph_class_init, NULL, NULL,
			sizeof(EEventHook), 0, (GInstanceInitFunc) NULL,
		};

		emph_parent_class = g_type_class_ref(e_plugin_hook_get_type());
		type = g_type_register_static(e_plugin_hook_get_type(), "EEventHook", &info, 0);
	}
	
	return type;
}

/**
 * e_event_hook_class_add_target_map:
 * @klass: The derived EEventHook class.
 * @map: A map used to describe a single EEventTarget type for this
 * class.
 * 
 * Add a target map to a concrete derived class of EEvent.  The target
 * map enumerates a single target type and th eenable mask bit names,
 * so that the type can be loaded automatically by the base EEvent class.
 **/
void e_event_hook_class_add_target_map(EEventHookClass *klass, const EEventHookTargetMap *map)
{
	g_hash_table_insert(klass->target_map, (void *)map->type, (void *)map);
}
