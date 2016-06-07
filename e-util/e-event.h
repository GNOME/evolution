/*
 *
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
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

/* This a bit 'whipped together', so is likely to change mid-term */

#ifndef E_EVENT_H
#define E_EVENT_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_EVENT \
	(e_event_get_type ())
#define E_EVENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EVENT, EEvent))
#define E_EVENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EVENT, EEventClass))
#define E_IS_EVENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EVENT))
#define E_IS_EVENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EVENT))
#define E_EVENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EVENT, EEventClass))

G_BEGIN_DECLS

/* This is an abstract event management class. */

typedef struct _EEvent EEvent;
typedef struct _EEventClass EEventClass;
typedef struct _EEventPrivate EEventPrivate;

typedef struct _EEventItem EEventItem;
typedef struct _EEventFactory EEventFactory; /* anonymous type */
typedef struct _EEventTarget EEventTarget;

typedef void (*EEventItemsFunc)(EEvent *ee, GSList *items, gpointer data);
typedef void (*EEventFunc)(EEvent *ee, EEventItem *item, gpointer data);
typedef void (*EEventFactoryFunc)(EEvent *ee, gpointer);

/**
 * enum _e_event_t - Event type.
 *
 * @E_EVENT_PASS: A passthrough event handler which only receives the event.
 * @E_EVENT_SINK: A sink event handler swallows all events it processes.
 *
 * The event type defines what type of event listener this is.
 *
 * Events should normally be @E_EVENT_PASS.
 **/
enum _e_event_t {
	E_EVENT_INVALID = -1,
	E_EVENT_PASS,		/* passthrough */
	E_EVENT_SINK		/* sink events */
};

/**
 * struct _EEventItem - An event listener item.
 *
 * @type: The type of the event listener.
 * @priority: A signed number signifying the priority of the event
 * listener.  0 should be used normally.  This is used to order event
 * receipt when multiple listners are present.
 * @id: The name of the event to listen to.  By convention events are of the form
 * "component.subcomponent".  The target mask provides further
 * sub-event type qualification.
 * @target_type: Target type for this event.  This is implementation
 * specific.
 * @handle: Event handler callback.
 * @user_data: Callback data.
 * @enable: Target-specific mask to qualify the receipt of events.
 * This is target and implementation specific.
 *
 * An EEventItem defines a specific event listening point on a given
 * EEvent object.  When an event is broadcast onto an EEvent handler,
 * any matching EEventItems will be invoked in priority order.
 **/
struct _EEventItem {
	enum _e_event_t type;
	gint priority;		/* priority of event */
	const gchar *id;		/* event id */
	gint target_type;
	EEventFunc handle;
	gpointer user_data;
	guint32 enable;		/* enable mask */
};

/**
 * struct _EEventTarget - Base EventTarget.
 *
 * @event: Parent object.
 * @type: Target type.  Defined by the implementation.
 * @mask: Mask of this target.  This is defined by the implementation,
 * the type, and the actual content of the target.
 *
 * This defined a base EventTarget.  This must be subclassed by
 * implementations to provide contextual data for events, and define
 * the enablement qualifiers.
 *
 **/
struct _EEventTarget {
	EEvent *event;	/* used for virtual methods */

	guint32 type;		/* targe type, for implementors */
	guint32 mask;		/* depends on type, enable mask */

	/* implementation fields follow */
};

/**
 * struct _EEvent - An Event Manager.
 *
 * @object: Superclass.
 * @priv: Private data.
 * @id: Id of this event manager.
 * @target: The current target, only set during event emission.
 *
 * The EEvent manager object.  Each component which defines event
 * types supplies a single EEvent manager object.  This manager routes
 * all events invoked on this object to all registered listeners based
 * on their qualifiers.
 **/
struct _EEvent {
	GObject object;
	EEventPrivate *priv;
	gchar *id;
	EEventTarget *target;	/* current target during event emission */
};

/**
 * struct _EEventClass - Event management type.
 *
 * @object_class: Superclass.
 * @target_free: Virtual method to free the target.
 *
 * The EEvent class definition.  This must be sub-classed for each
 * component that wishes to provide hookable events.  The subclass
 * only needs to know how to allocate and free each target type it
 * supports.
 **/
struct _EEventClass {
	GObjectClass object_class;

	void		(*target_free)		(EEvent *event,
						 EEventTarget *target);
};

GType		e_event_get_type		(void) G_GNUC_CONST;
EEvent *	e_event_construct		(EEvent *event,
						 const gchar *id);
gpointer	e_event_add_items		(EEvent *event,
						 GSList *items,
						 EEventItemsFunc freefunc,
						 gpointer data);
void		e_event_remove_items		(EEvent *event,
						 gpointer handle);
void		e_event_emit			(EEvent *event,
						 const gchar *id,
						 EEventTarget *target);
gpointer	e_event_target_new		(EEvent *event,
						 gint type,
						 gsize size);
void		e_event_target_free		(EEvent *event,
						 gpointer target);

/* ********************************************************************** */

/* event plugin target, they are closely integrated */

/* To implement a basic event menu plugin, you just need to subclass
 * this and initialise the class target type tables */

/* For events, the plugin item talks to a specific instance, rather than
 * a set of instances of the hook handler */

#include <e-util/e-plugin.h>

/* Standard GObject macros */
#define E_TYPE_EVENT_HOOK \
	(e_event_hook_get_type ())
#define E_EVENT_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EVENT_HOOK, EEventHook))
#define E_EVENT_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EVENT_HOOK, EEventHookClass))
#define E_IS_EVENT_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EVENT_HOOK))
#define E_IS_EVENT_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EVENT_HOOK))
#define E_EVENT_HOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EVENT_HOOK, EEventHookClass))

typedef struct _EEventHook EEventHook;
typedef struct _EEventHookClass EEventHookClass;

typedef struct _EPluginHookTargetMap EEventHookTargetMap;
typedef struct _EPluginHookTargetKey EEventHookTargetMask;

typedef void (*EEventHookFunc)(EPlugin *plugin, EEventTarget *target);

/**
 * struct _EEventHook - An event hook.
 *
 * @hook: Superclass.
 *
 * The EEventHook class loads and manages the meta-data required to
 * track event listeners.  Unlike other hook types, there is a 1:1
 * match between an EEventHook instance class and its EEvent instance.
 *
 * When the hook is loaded, all of its event hooks are stored directly
 * on the corresponding EEvent which is stored in its class static area.
 **/
struct _EEventHook {
	EPluginHook hook;
};

/**
 * struct _EEventHookClass -
 *
 * @hook_class:
 * @target_map: Table of EPluginHookTargetMaps which enumerate the
 * target types and enable bits of the implementing class.
 * @event: The EEvent instance on which all loaded events must be registered.
 *
 * The EEventHookClass is an empty event hooking class, which must be
 * subclassed and initialised before use.
 *
 * The EPluginHookClass.id must be set to the name and version of the
 * hook handler itself, and then the type must be registered with the
 * EPlugin hook list before any plugins are loaded.
 **/
struct _EEventHookClass {
	EPluginHookClass hook_class;

	/* EEventHookTargetMap by .type */
	GHashTable *target_map;
	/* the event router these events's belong to */
	EEvent *event;
};

GType		e_event_hook_get_type	(void) G_GNUC_CONST;
void		e_event_hook_class_add_target_map
					(EEventHookClass *hook_class,
					 const EEventHookTargetMap *map);

G_END_DECLS

#endif /* E_EVENT_H */
