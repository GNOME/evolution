/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "e-event.h"

#include <glib/gi18n.h>

#define d(x)

struct _event_node {
	GSList *events;
	gpointer data;
	EEventItemsFunc freefunc;
};

struct _event_info {
	struct _event_node *parent;
	EEventItem *item;
};

struct _EEventPrivate {
	GQueue events;
	GSList *sorted;		/* sorted list of struct _event_info's */
};

G_DEFINE_TYPE_WITH_PRIVATE (EEvent, e_event, G_TYPE_OBJECT)

static void
event_finalize (GObject *object)
{
	EEvent *event = (EEvent *) object;
	EEventPrivate *p = event->priv;

	if (event->target)
		e_event_target_free (event, event->target);

	g_free (event->id);

	while (!g_queue_is_empty (&p->events)) {
		struct _event_node *node;

		node = g_queue_pop_head (&p->events);

		if (node->freefunc != NULL)
			node->freefunc (event, node->events, node->data);

		g_free (node);
	}

	g_slist_foreach (p->sorted, (GFunc) g_free, NULL);
	g_slist_free (p->sorted);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_event_parent_class)->finalize (object);
}

static void
event_target_free (EEvent *event,
                   EEventTarget *target)
{
	g_free (target);
	g_object_unref (event);
}

static void
e_event_class_init (EEventClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = event_finalize;

	class->target_free = event_target_free;
}

static void
e_event_init (EEvent *event)
{
	event->priv = e_event_get_instance_private (event);

	g_queue_init (&event->priv->events);
}

/**
 * e_event_construct:
 * @event: An instantiated but uninitialised EEvent.
 * @id: Event manager id.
 *
 * Construct the base event instance with standard parameters.
 *
 * Returns: the @event
 **/
EEvent *
e_event_construct (EEvent *event,
                   const gchar *id)
{
	event->id = g_strdup (id);

	return event;
}

/**
 * e_event_add_items:
 * @event: An initialised EEvent structure.
 * @items: A list of EEventItems event listeners to register on this event manager.
 * @freefunc: A function called when the @items list is no longer needed.
 * @data: callback data for @freefunc and for item event handlers.
 *
 * Adds @items to the list of events listened to on the event manager @event.
 *
 * Return value: An opaque key which can later be passed to remove_items.
 **/
gpointer
e_event_add_items (EEvent *event,
                   GSList *items,
                   EEventItemsFunc freefunc,
                   gpointer data)
{
	struct _event_node *node;

	node = g_malloc (sizeof (*node));
	node->events = items;
	node->freefunc = freefunc;
	node->data = data;

	g_queue_push_tail (&event->priv->events, node);

	if (event->priv->sorted) {
		g_slist_foreach (event->priv->sorted, (GFunc) g_free, NULL);
		g_slist_free (event->priv->sorted);
		event->priv->sorted = NULL;
	}

	return (gpointer) node;
}

/**
 * e_event_remove_items:
 * @event: an #EEvent
 * @handle: an opaque key returned by e_event_add_items()
 *
 * Remove items previously added.  They MUST have been previously
 * added, and may only be removed once.
 **/
void
e_event_remove_items (EEvent *event,
                      gpointer handle)
{
	struct _event_node *node = handle;

	g_queue_remove (&event->priv->events, node);

	if (node->freefunc)
		node->freefunc (event, node->events, node->data);
	g_free (node);

	if (event->priv->sorted) {
		g_slist_foreach (event->priv->sorted, (GFunc) g_free, NULL);
		g_slist_free (event->priv->sorted);
		event->priv->sorted = NULL;
	}
}

static gint
ee_cmp (gconstpointer ap,
        gconstpointer bp)
{
	gint a = ((struct _event_info **) ap)[0]->item->priority;
	gint b = ((struct _event_info **) bp)[0]->item->priority;

	if (a < b)
		return 1;
	else if (a > b)
		return -1;
	else
		return 0;
}

/**
 * e_event_emit:
 * event: An initialised EEvent, potentially with registered event listeners.
 * @id: Event name.  This will be compared against EEventItem.id.
 * @target: The target describing the event context.  This will be
 * implementation defined.
 *
 * Emit an event.  @target will automatically be freed once its
 * emission is complete.
 **/
void
e_event_emit (EEvent *event,
              const gchar *id,
              EEventTarget *target)
{
	EEventPrivate *p = event->priv;
	GSList *events;

	d (printf ("emit event %s\n", id));

	if (event->target != NULL) {
		g_warning ("Event already in progress.\n");
		return;
	}

	event->target = target;
	events = p->sorted;
	if (events == NULL) {
		GList *link = g_queue_peek_head_link (&p->events);

		while (link != NULL) {
			struct _event_node *node = link->data;
			GSList *l = node->events;

			for (; l; l = g_slist_next (l)) {
				struct _event_info *info;

				info = g_malloc0 (sizeof (*info));
				info->parent = node;
				info->item = l->data;
				events = g_slist_prepend (events, info);
			}

			link = g_list_next (link);
		}

		p->sorted = events = g_slist_sort (events, ee_cmp);
	}

	for (; events; events = g_slist_next (events)) {
		struct _event_info *info = events->data;
		EEventItem *item = info->item;

		if (item->enable & target->mask)
			continue;

		if (strcmp (item->id, id) == 0) {
			item->handle (event, item, info->parent->data);

			if (item->type == E_EVENT_SINK)
				break;
		}
	}

	e_event_target_free (event, target);
	event->target = NULL;
}

/**
 * e_event_target_new:
 * @event: An initialised EEvent instance.
 * @type: type, up to implementor
 * @size: The size of memory to allocate.  This must be >= sizeof(EEventTarget).
 *
 * Allocate a new event target suitable for this class.  It is up to
 * the implementation to define the available target types and their
 * structure.
 **/
gpointer
e_event_target_new (EEvent *event,
                    gint type,
                    gsize size)
{
	EEventTarget *target;

	if (size < sizeof (EEventTarget)) {
		g_warning ("Size is less than the size of EEventTarget\n");
		size = sizeof (EEventTarget);
	}

	target = g_malloc0 (size);
	target->event = g_object_ref (event);
	target->type = type;

	return target;
}

/**
 * e_event_target_free:
 * @event: An initialised EEvent instance on which this target was allocated.
 * @target: The target to free.
 *
 * Free a target.  This invokes the virtual free method on the EEventClass.
 **/
void
e_event_target_free (EEvent *event,
                     gpointer target)
{
	E_EVENT_GET_CLASS (event)->target_free (
		event, (EEventTarget *) target);
}

/* ********************************************************************** */

/* Event menu plugin handler */

/*
 * <e-plugin
 *  class="org.gnome.mail.plugin.event:1.0"
 *  id="org.gnome.mail.plugin.event.item:1.0"
 *  type="shlib"
 *  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
 *  name="imap"
 *  description="IMAP4 and IMAP4v1 mail store">
 *  <hook class="org.gnome.mail.eventMenu:1.0"
 *      handler="HandleEvent">
 *   <menu id="any" target="select">
 *   <item
 *    type="item|toggle|radio|image|submenu|bar"
 *    active
 *    path="foo/bar"
 *    label="label"
 *    icon="foo"
 *    mask="select_one"
 *    activate="ep_view_emacs"/>
 *   </menu>
 *  </hook>
 *
 * <hook class="org.gnome.evolution.mail.events:1.0">
 * <event id=".folder.changed"
 *  target=""
 *  priority="0"
 *  handle="gotevent"
 *  enable="new"
 *  />
 * <event id=".message.read"
 *  priority="0"
 *  handle="gotevent"
 *  mask="new"
 *  />
 * </hook>
 *
 */

#define emph ((EEventHook *)eph)

/* must have 1:1 correspondence with e-event types in order */
static const EPluginHookTargetKey emph_item_types[] = {
	{ "pass", E_EVENT_PASS },
	{ "sink", E_EVENT_SINK },
	{ NULL }
};

G_DEFINE_TYPE (
	EEventHook,
	e_event_hook,
	E_TYPE_PLUGIN_HOOK)

static void
emph_event_handle (EEvent *ee,
                   EEventItem *item,
                   gpointer data)
{
	EEventHook *hook = data;

	/* FIXME We could/should just remove the items
	 *       we added to the event handler. */
	if (!hook->hook.plugin->enabled)
		return;

	e_plugin_invoke (
		hook->hook.plugin, (gchar *) item->user_data, ee->target);
}

static void
emph_free_item (EEventItem *item)
{
	g_free ((gchar *) item->id);
	g_free (item->user_data);
	g_free (item);
}

static void
emph_free_items (EEvent *ee,
                 GSList *items,
                 gpointer data)
{
	/*EPluginHook *eph = data;*/

	g_slist_foreach (items, (GFunc) emph_free_item, NULL);
	g_slist_free (items);
}

static EEventItem *
emph_construct_item (EPluginHook *eph,
                     xmlNodePtr root,
                     EEventHookClass *class)
{
	EEventItem *item;
	EEventHookTargetMap *map;
	gchar *tmp;

	item = g_malloc0 (sizeof (*item));

	tmp = (gchar *) xmlGetProp (root, (const guchar *)"target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup (class->target_map, tmp);
	xmlFree (tmp);
	if (map == NULL)
		goto error;
	item->target_type = map->id;
	item->type = e_plugin_hook_id (root, emph_item_types, "type");
	if (item->type == E_EVENT_INVALID)
		item->type = E_EVENT_PASS;
	item->priority = e_plugin_xml_int (root, "priority", 0);
	item->id = e_plugin_xml_prop (root, "id");
	item->enable = e_plugin_hook_mask (root, map->mask_bits, "enable");
	item->user_data = e_plugin_xml_prop (root, "handle");

	if (item->user_data == NULL || item->id == NULL)
		goto error;

	item->handle = emph_event_handle;

	return item;
error:
	emph_free_item (item);
	return NULL;
}

static gint
emph_construct (EPluginHook *eph,
                EPlugin *ep,
                xmlNodePtr root)
{
	xmlNodePtr node;
	EEventHookClass *class;
	GSList *items = NULL;

	d (printf ("loading event hook\n"));

	if (((EPluginHookClass *) e_event_hook_parent_class)->
		construct (eph, ep, root) == -1)
		return -1;

	class = E_EVENT_HOOK_GET_CLASS (eph);
	g_return_val_if_fail (class->event != NULL, -1);

	node = root->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "event") == 0) {
			EEventItem *item;

			item = emph_construct_item (eph, node, class);
			if (item)
				items = g_slist_prepend (items, item);
		}
		node = node->next;
	}

	eph->plugin = ep;

	if (items)
		e_event_add_items (class->event, items, emph_free_items, eph);

	return 0;
}

static void
e_event_hook_class_init (EEventHookClass *class)
{
	EPluginHookClass *plugin_hook_class;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.event:1.0";
	plugin_hook_class->construct = emph_construct;

	class->target_map = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
e_event_hook_init (EEventHook *hook)
{
}

/**
 * e_event_hook_class_add_target_map:
 * @hook_class: The derived EEventHook class.
 * @map: A map used to describe a single EEventTarget type for this class.
 *
 * Add a target map to a concrete derived class of EEvent.  The target
 * map enumerates a single target type and th eenable mask bit names,
 * so that the type can be loaded automatically by the base EEvent class.
 **/
void
e_event_hook_class_add_target_map (EEventHookClass *hook_class,
                                   const EEventHookTargetMap *map)
{
	g_hash_table_insert (
		hook_class->target_map,
		(gpointer) map->type, (gpointer) map);
}
