/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-object.c: Base class for Camel */

/*
 * Author:
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "camel-object.h"

#ifdef ENABLE_THREADS
#include <pthread.h>
#include <e-util/e-msgport.h>
#endif

/* I just mashed the keyboard for these... */
#define CAMEL_OBJECT_MAGIC_VALUE           0x77A344EF
#define CAMEL_OBJECT_CLASS_MAGIC_VALUE     0xEE26A990
#define CAMEL_OBJECT_FINALIZED_VALUE       0x84AC3656
#define CAMEL_OBJECT_CLASS_FINALIZED_VALUE 0x7621ABCD

#define DEFAULT_PREALLOCS 8

#define BAST_CASTARD 1		/* Define to return NULL when casts fail */

#define NULL_PREP_VALUE ((gpointer)make_global_classfuncs)	/* See camel_object_class_declare_event */

/* ** Quickie type system ************************************************* */

typedef struct _CamelTypeInfo
{
	CamelType self;
	CamelType parent;
	const gchar *name;

	size_t instance_size;
	GMemChunk *instance_chunk;
	CamelObjectInitFunc instance_init;
	CamelObjectFinalizeFunc instance_finalize;
	GList *free_instances;

	size_t classfuncs_size;
	CamelObjectClassInitFunc class_init;
	CamelObjectClassFinalizeFunc class_finalize;
	CamelObjectClass *global_classfuncs;
}
CamelTypeInfo;

/* A 'locked' hooklist, that is only allocated on demand */
typedef struct _CamelHookList {
	EMutex *lock;

	unsigned int depth:30;	/* recursive event depth */
	unsigned int flags:2;	/* flags, see below */

	unsigned int list_length;
	struct _CamelHookPair *list;
} CamelHookList;

#define CAMEL_HOOK_PAIR_REMOVED (1<<0)

/* a 'hook pair', actually a hook tuple, we just store all hooked events in the same list,
   and just comapre as we go, rather than storing separate lists for each hook type

   the name field just points directly to the key field in the class's preplist hashtable.
   This way we can just use a direct pointer compare when scanning it, and also saves
   copying the string */
typedef struct _CamelHookPair
{
	struct _CamelHookPair *next; /* next MUST be the first member */

	unsigned int flags;	/* removed, etc */

	const char *name;	/* points to the key field in the classes preplist, static memory */
	CamelObjectEventHookFunc func;
	void *data;
} CamelHookPair;

/* ************************************************************************ */

static void camel_type_lock_up (void);
static void camel_type_lock_down (void);

static void obj_init (CamelObject * obj);
static void obj_finalize (CamelObject * obj);
static void obj_class_init (CamelObjectClass * class);
static void obj_class_finalize (CamelObjectClass * class);

static gboolean shared_is_of_type (CamelObjectShared * sh, CamelType ctype,
				   gboolean is_obj);
static void make_global_classfuncs (CamelTypeInfo * type_info);

static void camel_object_free_hooks(CamelObject *o);

/* ************************************************************************ */

G_LOCK_DEFINE_STATIC (type_system);
G_LOCK_DEFINE_STATIC (type_system_level);
static GPrivate *type_system_locklevel = NULL;

G_LOCK_DEFINE_STATIC (refcount);

static gboolean type_system_initialized = FALSE;
static GHashTable *ctype_to_typeinfo = NULL;
static const CamelType camel_object_type = 1;
static CamelType cur_max_type = CAMEL_INVALID_TYPE;

/* ************************************************************************ */

#define LOCK_VAL (GPOINTER_TO_INT (g_private_get (type_system_locklevel)))
#define LOCK_SET( val ) g_private_set (type_system_locklevel, GINT_TO_POINTER (val))

static void
camel_type_lock_up (void)
{
	G_LOCK (type_system_level);

	if (type_system_locklevel == NULL)
		type_system_locklevel = g_private_new (GINT_TO_POINTER (0));

	if (LOCK_VAL == 0) {
		G_UNLOCK (type_system_level);
		G_LOCK (type_system);
		G_LOCK (type_system_level);
	}

	LOCK_SET (LOCK_VAL + 1);

	G_UNLOCK (type_system_level);
}

static void
camel_type_lock_down (void)
{
	G_LOCK (type_system_level);

	if (type_system_locklevel == NULL) {
		g_warning
			("camel_type_lock_down: lock down before a lock up?");
		type_system_locklevel = g_private_new (GINT_TO_POINTER (0));
		G_UNLOCK (type_system_level);
		return;
	}

	LOCK_SET (LOCK_VAL - 1);

	if (LOCK_VAL == 0)
		G_UNLOCK (type_system);

	G_UNLOCK (type_system_level);
}

void
camel_type_init (void)
{
	CamelTypeInfo *obj_info;

	camel_type_lock_up ();

	if (type_system_initialized) {
		g_warning
			("camel_type_init: type system already initialized.");
		camel_type_lock_down ();
		return;
	}

	type_system_initialized = TRUE;
	ctype_to_typeinfo = g_hash_table_new (g_direct_hash, g_direct_equal);

	obj_info = g_new (CamelTypeInfo, 1);
	obj_info->self = camel_object_type;
	obj_info->parent = CAMEL_INVALID_TYPE;
	obj_info->name = "CamelObject";

	obj_info->instance_size = sizeof (CamelObject);
	obj_info->instance_chunk =
		g_mem_chunk_create (CamelObject, DEFAULT_PREALLOCS,
				    G_ALLOC_ONLY);
	obj_info->instance_init = obj_init;
	obj_info->instance_finalize = obj_finalize;
	obj_info->free_instances = NULL;

	obj_info->classfuncs_size = sizeof (CamelObjectClass);
	obj_info->class_init = obj_class_init;
	obj_info->class_finalize = obj_class_finalize;

	g_hash_table_insert (ctype_to_typeinfo,
			     GINT_TO_POINTER (CAMEL_INVALID_TYPE), NULL);
	g_hash_table_insert (ctype_to_typeinfo,
			     GINT_TO_POINTER (camel_object_type), obj_info);

	/* Sigh. Ugly */
	make_global_classfuncs (obj_info);

	cur_max_type = camel_object_type;

	camel_type_lock_down ();
}

CamelType
camel_type_register (CamelType parent, const gchar * name,
		     size_t instance_size, size_t classfuncs_size,
		     CamelObjectClassInitFunc class_init,
		     CamelObjectClassFinalizeFunc class_finalize,
		     CamelObjectInitFunc instance_init,
		     CamelObjectFinalizeFunc instance_finalize)
{
	CamelTypeInfo *parent_info;
	CamelTypeInfo *obj_info;
	gchar *chunkname;

	g_return_val_if_fail (parent != CAMEL_INVALID_TYPE,
			      CAMEL_INVALID_TYPE);
	g_return_val_if_fail (name, CAMEL_INVALID_TYPE);
	g_return_val_if_fail (instance_size, CAMEL_INVALID_TYPE);
	g_return_val_if_fail (classfuncs_size, CAMEL_INVALID_TYPE);

	camel_type_lock_up ();

	if (type_system_initialized == FALSE) {
		G_UNLOCK (type_system);
		camel_type_init ();
		G_LOCK (type_system);
	}

	parent_info =
		g_hash_table_lookup (ctype_to_typeinfo,
				     GINT_TO_POINTER (parent));

	if (parent_info == NULL) {
		g_warning
			("camel_type_register: no such parent type %d of class `%s'",
			 parent, name);
		camel_type_lock_down ();
		return CAMEL_INVALID_TYPE;
	}

	if (parent_info->instance_size > instance_size) {
		g_warning
			("camel_type_register: instance of class `%s' would be smaller than parent `%s'",
			 name, parent_info->name);
		camel_type_lock_down ();
		return CAMEL_INVALID_TYPE;
	}

	if (parent_info->classfuncs_size > classfuncs_size) {
		g_warning
			("camel_type_register: classfuncs of class `%s' would be smaller than parent `%s'",
			 name, parent_info->name);
		camel_type_lock_down ();
		return CAMEL_INVALID_TYPE;
	}

	cur_max_type++;

	obj_info = g_new (CamelTypeInfo, 1);
	obj_info->self = cur_max_type;
	obj_info->parent = parent;
	obj_info->name = name;

	obj_info->instance_size = instance_size;
	chunkname =
		g_strdup_printf ("chunk for instances of Camel type `%s'",
				 name);
	obj_info->instance_chunk =
		g_mem_chunk_new (chunkname, instance_size,
				 instance_size * DEFAULT_PREALLOCS,
				 G_ALLOC_ONLY);
	g_free (chunkname);
	obj_info->instance_init = instance_init;
	obj_info->instance_finalize = instance_finalize;
	obj_info->free_instances = NULL;

	obj_info->classfuncs_size = classfuncs_size;
	obj_info->class_init = class_init;
	obj_info->class_finalize = class_finalize;

	g_hash_table_insert (ctype_to_typeinfo,
			     GINT_TO_POINTER (obj_info->self), obj_info);

	/* Sigh. Ugly. */
	make_global_classfuncs (obj_info);

	camel_type_lock_down ();
	return obj_info->self;
}

CamelObjectClass *
camel_type_get_global_classfuncs (CamelType type)
{
	CamelTypeInfo *type_info;

	g_return_val_if_fail (type != CAMEL_INVALID_TYPE, NULL);

	camel_type_lock_up ();
	type_info =
		g_hash_table_lookup (ctype_to_typeinfo,
				     GINT_TO_POINTER (type));
	camel_type_lock_down ();

	g_return_val_if_fail (type_info != NULL, NULL);

	return type_info->global_classfuncs;
}

const gchar *
camel_type_to_name (CamelType type)
{
	CamelTypeInfo *type_info;

	g_return_val_if_fail (type != CAMEL_INVALID_TYPE,
			      "(the invalid type)");

	camel_type_lock_up ();
	type_info =
		g_hash_table_lookup (ctype_to_typeinfo,
				     GINT_TO_POINTER (type));
	camel_type_lock_down ();

	g_return_val_if_fail (type_info != NULL,
			      "(a bad type parameter was specified)");

	return type_info->name;
}

/* ** The CamelObject ***************************************************** */

static void
obj_init (CamelObject * obj)
{
	obj->s.magic = CAMEL_OBJECT_MAGIC_VALUE;
	obj->ref_count = 1;
	obj->hooks = NULL;
	obj->in_event = 0;
	obj->destroying = 0;
}

static void
obj_finalize (CamelObject * obj)
{
	g_return_if_fail (obj->s.magic == CAMEL_OBJECT_MAGIC_VALUE);
	g_return_if_fail (obj->ref_count == 0);
	g_return_if_fail (obj->in_event == 0);

	obj->s.magic = CAMEL_OBJECT_FINALIZED_VALUE;

	camel_object_free_hooks(obj);
}

static void
obj_class_init (CamelObjectClass * class)
{
	class->s.magic = CAMEL_OBJECT_CLASS_MAGIC_VALUE;

	camel_object_class_declare_event (class, "finalize", NULL);
}

static void
obj_class_finalize (CamelObjectClass * class)
{
	g_return_if_fail (class->s.magic == CAMEL_OBJECT_CLASS_MAGIC_VALUE);

	class->s.magic = CAMEL_OBJECT_CLASS_FINALIZED_VALUE;

	if (class->event_to_preplist) {
		/* FIXME: This leaks the preplist slist entries */
		g_hash_table_foreach (class->event_to_preplist,
				      (GHFunc) g_free, NULL);
		g_hash_table_destroy (class->event_to_preplist);
		class->event_to_preplist = NULL;
	}
}

CamelType
camel_object_get_type (void)
{
	if (type_system_initialized == FALSE)
		camel_type_init ();

	return camel_object_type;
}

CamelObject *
camel_object_new (CamelType type)
{
	CamelTypeInfo *type_info;
	GSList *parents = NULL;
	GSList *head = NULL;
	CamelObject *instance;

	g_return_val_if_fail (type != CAMEL_INVALID_TYPE, NULL);

	/* Look up the type */

	camel_type_lock_up ();

	type_info =
		g_hash_table_lookup (ctype_to_typeinfo,
				     GINT_TO_POINTER (type));

	if (type_info == NULL) {
		g_warning
			("camel_object_new: trying to create object of invalid type %d",
			 type);
		camel_type_lock_down ();
		return NULL;
	}

	/* Grab an instance out of the freed ones if possible, alloc otherwise */

	if (type_info->free_instances) {
		GList *first;

		first = g_list_first (type_info->free_instances);
		instance = first->data;
		type_info->free_instances =
			g_list_remove_link (type_info->free_instances, first);
		g_list_free_1 (first);
		memset (instance, 0, type_info->instance_size);
	} else {
		instance = g_mem_chunk_alloc0 (type_info->instance_chunk);
	}

	/* Init the instance and classfuncs a bit */

	instance->s.type = type;
	instance->classfuncs = type_info->global_classfuncs;

	/* Loop through the parents in simplest -> most complex order, initing the class and instance.

	 * When parent = CAMEL_INVALID_TYPE and we're at the end of the line, _lookup returns NULL
	 * because we inserted it as corresponding to CAMEL_INVALID_TYPE. Clever, eh?
	 */

	while (type_info) {
		parents = g_slist_prepend (parents, type_info);
		type_info =
			g_hash_table_lookup (ctype_to_typeinfo,
					     GINT_TO_POINTER (type_info->
							      parent));
	}

	head = parents;

	for (; parents && parents->data; parents = parents->next) {
		CamelTypeInfo *thisinfo;

		thisinfo = parents->data;
		if (thisinfo->instance_init)
			(thisinfo->instance_init) (instance);
	}

	g_slist_free (head);

	camel_type_lock_down ();
	return instance;
}

void
camel_object_ref (CamelObject * obj)
{
	g_return_if_fail (CAMEL_IS_OBJECT (obj));

	G_LOCK (refcount);
	obj->ref_count++;
	G_UNLOCK (refcount);
}

void
camel_object_unref (CamelObject * obj)
{
	CamelTypeInfo *type_info;
	CamelTypeInfo *iter;
	GSList *parents = NULL;
	GSList *head = NULL;

	g_return_if_fail (CAMEL_IS_OBJECT (obj));

	G_LOCK (refcount);
	obj->ref_count--;

	if (obj->ref_count > 0) {
		G_UNLOCK (refcount);
		return;
	}

	G_UNLOCK (refcount);

	/* If the object already had its last unref, do not begin the
	 * destruction process again. This can happen if, for example,
	 * the object sends an event in its finalize handler (vfolders
	 * do this).
	 */

	if (obj->destroying)
		return;
	
	obj->destroying = 1;

	/* Send the finalize event */

	camel_object_trigger_event (obj, "finalize", NULL);

	/* Destroy it! hahaha! */

	camel_type_lock_up ();

	type_info =
		g_hash_table_lookup (ctype_to_typeinfo,
				     GINT_TO_POINTER (obj->s.type));

	if (type_info == NULL) {
		g_warning
			("camel_object_unref: seemingly valid object has a bad type %d",
			 obj->s.type);
		camel_type_lock_down ();
		return;
	}

	/* Loop through the parents in most complex -> simplest order, finalizing the class 
	 * and instance.
	 *
	 * When parent = CAMEL_INVALID_TYPE and we're at the end of the line, _lookup returns NULL
	 * because we inserted it as corresponding to CAMEL_INVALID_TYPE. Clever, eh?
	 *
	 * Use iter to preserve type_info for free_{instance,classfunc}s
	 */

	iter = type_info;

	while (iter) {
		parents = g_slist_prepend (parents, iter);
		iter =
			g_hash_table_lookup (ctype_to_typeinfo,
					     GINT_TO_POINTER (iter->parent));
	}

	/* ok, done with the type stuff, and our data pointers
	 * won't go bad. */
	camel_type_lock_down ();

	parents = g_slist_reverse (parents);
	head = parents;

	for (; parents && parents->data; parents = parents->next) {
		CamelTypeInfo *thisinfo;

		thisinfo = parents->data;
		if (thisinfo->instance_finalize)
			(thisinfo->instance_finalize) (obj);
	}

	g_slist_free (head);

	/* Sanity check */

	if (obj->ref_count != 0)
		g_warning ("camel_object_unref: destroyed object %s at %p somehow got"
			   " referenced in destruction chain.",
			   camel_type_to_name (obj->s.type),
			   obj);

	/* A little bit of cleaning up.

	 * Don't erase the type, so we can peek at it if a finalized object
	 * is check_cast'ed somewhere.  Fill it with gunk to help detect
	 * other invalid ref's of it.
	 */

	memset (obj, 0xEB, type_info->instance_size);
	obj->s.type = type_info->self;
	obj->s.magic = CAMEL_OBJECT_FINALIZED_VALUE;

	/* Tuck away the pointer for use in a new object */

	camel_type_lock_up ();

	type_info->free_instances =
		g_list_prepend (type_info->free_instances, obj);

	camel_type_lock_down ();
}

gboolean
camel_object_is_of_type (CamelObject * obj, CamelType ctype)
{
	return shared_is_of_type ((CamelObjectShared *) obj, ctype, TRUE);
}

gboolean
camel_object_class_is_of_type (CamelObjectClass * class, CamelType ctype)
{
	return shared_is_of_type ((CamelObjectShared *) class, ctype, FALSE);
}

#ifdef BAST_CASTARD
#define ERRVAL NULL
#else
#define ERRVAL obj
#endif

CamelObject *
camel_object_check_cast (CamelObject * obj, CamelType ctype)
{
	if (shared_is_of_type ((CamelObjectShared *) obj, ctype, TRUE))
		return obj;
	return ERRVAL;
}

CamelObjectClass *
camel_object_class_check_cast (CamelObjectClass * class, CamelType ctype)
{
	if (shared_is_of_type ((CamelObjectShared *) class, ctype, FALSE))
		return class;
	return ERRVAL;
}

#undef ERRVAL

gchar *
camel_object_describe (CamelObject * obj)
{
	if (obj == NULL)
		return g_strdup ("a NULL pointer");

	if (obj->s.magic == CAMEL_OBJECT_MAGIC_VALUE) {
		return g_strdup_printf ("an instance of `%s' at %p",
					camel_type_to_name (obj->s.type),
					obj);
	} else if (obj->s.magic == CAMEL_OBJECT_FINALIZED_VALUE) {
		return g_strdup_printf ("a finalized instance of `%s' at %p",
					camel_type_to_name (obj->s.type),
					obj);
	} else if (obj->s.magic == CAMEL_OBJECT_CLASS_MAGIC_VALUE) {
		return g_strdup_printf ("the classfuncs of `%s' at %p",
					camel_type_to_name (obj->s.type),
					obj);
	} else if (obj->s.magic == CAMEL_OBJECT_CLASS_FINALIZED_VALUE) {
		return
			g_strdup_printf
			("the finalized classfuncs of `%s' at %p",
			 camel_type_to_name (obj->s.type), obj);
	}

	return g_strdup ("not a CamelObject");
}

/* This is likely to be called in the class_init callback,
 * and the type will likely be somewhat uninitialized. 
 * Is this a problem? We'll see....
 */
void
camel_object_class_declare_event (CamelObjectClass * class,
				  const gchar * name,
				  CamelObjectEventPrepFunc prep)
{
	g_return_if_fail (CAMEL_IS_OBJECT_CLASS (class));
	g_return_if_fail (name);

	if (class->event_to_preplist == NULL)
		class->event_to_preplist =
			g_hash_table_new (g_str_hash, g_str_equal);
	else if (g_hash_table_lookup (class->event_to_preplist, name) != NULL) {
		g_warning
			("camel_object_class_declare_event: event `%s' already declared for `%s'",
			 name, camel_type_to_name (class->s.type));
		return;
	}

	/* AIEEEEEEEEEEEEEEEEEEEEEE

	 * I feel so naughty. Since it's valid to declare an event and not
	 * provide a hook, it should be valid to insert a NULL value into
	 * the table. However, then our lookup in trigger_event would be
	 * ambiguous, not telling us whether the event is undefined or whether
	 * it merely has no hook.
	 *
	 * So we create an 'NULL prep' value that != NULL... specifically, it
	 * equals the address of one of our static functions , because that
	 * can't possibly be your hook.
	 *
	 * Just don't forget to check for the 'evil value' and it'll work,
	 * I promise.
	 */

	if (prep == NULL)
		prep = NULL_PREP_VALUE;

	g_hash_table_insert (class->event_to_preplist, g_strdup (name), prep);
}

/* free hook data */
static void camel_object_free_hooks(CamelObject *o)
{
	CamelHookPair *pair, *next;

	if (o->hooks) {

		g_assert(o->hooks->depth == 0);
		g_assert((o->hooks->flags & CAMEL_HOOK_PAIR_REMOVED) == 0);

		pair = o->hooks->list;
		while (pair) {
			next = pair->next;
			g_free(pair);
			pair = next;
		}
		e_mutex_destroy(o->hooks->lock);
		g_free(o->hooks);
		o->hooks = NULL;
	}
}

/* return (allocate if required) the object's hook list, locking at the same time */
static CamelHookList *camel_object_get_hooks(CamelObject *o)
{
#ifdef ENABLE_THREADS
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#endif
	CamelHookList *hooks;

	/* if we have it, we dont have to do any other locking,
	   otherwise use a global lock to setup the object's hook data */
#ifdef ENABLE_THREADS
	if (o->hooks == NULL) {
		pthread_mutex_lock(&lock);
#endif
		if (o->hooks == NULL) {
			hooks = g_malloc(sizeof(*o->hooks));
#ifdef ENABLE_THREADS
			hooks->lock = e_mutex_new(E_MUTEX_REC);
#endif
			hooks->flags = 0;
			hooks->depth = 0;
			hooks->list_length = 0;
			hooks->list = NULL;
			o->hooks = hooks;
		}
#ifdef ENABLE_THREADS
		pthread_mutex_unlock(&lock);
	}
#endif

#ifdef ENABLE_THREADS
	e_mutex_lock(o->hooks->lock);
#endif
	return o->hooks;	
}

/* unlock object hooks' list */
#ifdef ENABLE_THREADS
#define camel_object_unget_hooks(o) (e_mutex_unlock((CAMEL_OBJECT(o)->hooks->lock)))
#else
#define camel_object_unget_hooks(o)
#endif

void
camel_object_hook_event (CamelObject * obj, const char * name,
			 CamelObjectEventHookFunc func, void *data)
{
	CamelHookPair *pair;
	const char *prepname;
	CamelObjectEventPrepFunc prep;
	CamelHookList *hooks;

	g_return_if_fail (CAMEL_IS_OBJECT (obj));
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL);

	/* first, does this event exist? */
	if (obj->classfuncs->event_to_preplist == NULL
	    || !g_hash_table_lookup_extended(obj->classfuncs->event_to_preplist, name,
					     (void **)&prepname, (void **)&prep)) {
		g_warning("camel_object_hook_event: trying to hook event `%s' in class `%s' with no defined events.",
			  name, camel_type_to_name (obj->s.type));
		return;
	}

	/* setup hook pair */
	pair = g_malloc(sizeof(*pair));
	pair->name = prepname;	/* effectively static! */
	pair->func = func;
	pair->data = data;
	pair->flags = 0;

	/* get the hook list object, locked, link in new event hook, unlock */
	hooks = camel_object_get_hooks(obj);
	pair->next = hooks->list;
	hooks->list = pair;
	hooks->list_length++;
	camel_object_unget_hooks(obj);
}

void
camel_object_unhook_event (CamelObject * obj, const char * name,
			   CamelObjectEventHookFunc func, void *data)
{
	char *prepname;
	CamelObjectEventPrepFunc prep;
	CamelHookList *hooks;
	CamelHookPair *pair, *parent;

	g_return_if_fail (CAMEL_IS_OBJECT (obj));
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL);

	if (obj->hooks == NULL) {
		g_warning("camel_object_unhook_event: trying to unhook `%s` from an instance of `%s' with no hooks",
			  name, camel_type_to_name(obj->s.type));
		return;
	}

	/* get event name static pointer */
	if (obj->classfuncs->event_to_preplist == NULL
	    || !g_hash_table_lookup_extended(obj->classfuncs->event_to_preplist, name,
					     (void **)&prepname, (void **)&prep)) {
		g_warning("camel_object_hook_event: trying to hook event `%s' in class `%s' with no defined events.",
			  name, camel_type_to_name (obj->s.type));
		return;
	}

	/* scan hooks for this event, remove it, or flag it if we're busy */
	hooks = camel_object_get_hooks(obj);
	parent = (CamelHookPair *)&hooks->list;
	pair = parent->next;
	while (pair) {
		if (pair->name == prepname
		    && pair->func == func
		    && pair->data == data
		    && (pair->flags & CAMEL_HOOK_PAIR_REMOVED) == 0) {
			if (hooks->depth > 0) {
				pair->flags |= CAMEL_HOOK_PAIR_REMOVED;
				hooks->flags |= CAMEL_HOOK_PAIR_REMOVED;
			} else {
				parent->next = pair->next;
				g_free(pair);
				hooks->list_length--;
			}
			camel_object_unget_hooks(obj);
			return;
		}
		parent = pair;
		pair = pair->next;
	}
	camel_object_unget_hooks(obj);

	g_warning("camel_object_unhook_event: cannot find hook/data pair %p/%p in an instance of `%s' attached to `%s'",
		  func, data, camel_type_to_name (obj->s.type), name);
}

void
camel_object_trigger_event (CamelObject * obj, const char * name, void *event_data)
{
	CamelObjectEventPrepFunc prep;
	const char *prepname;
	CamelHookList *hooks;
	CamelHookPair *pair, **pairs, *parent;
	int i, size;

	g_return_if_fail (CAMEL_IS_OBJECT (obj));
	g_return_if_fail (name);

	/* get event name static pointer/prep func */
	if (obj->classfuncs->event_to_preplist == NULL
	    || !g_hash_table_lookup_extended(obj->classfuncs->event_to_preplist, name,
					     (void **)&prepname, (void **)&prep)) {
		g_warning("camel_object_hook_event: trying to hook event `%s' in class `%s' with no defined events.",
			  name, camel_type_to_name (obj->s.type));
		return;
	}

	/* try prep function, if false, then quit */
	if (prep != NULL_PREP_VALUE && !prep(obj, event_data))
		return;

	/* also, no hooks, dont bother going further */
	if (obj->hooks == NULL)
		return;

	/* lock the object for hook emission */
	camel_object_ref(obj);
	hooks = camel_object_get_hooks(obj);

	if (hooks->list) {
		/* first, copy the items in the list, and say we're in an event */
		hooks->depth++;
		pair = hooks->list;
		size = 0;
		pairs = alloca(sizeof(pairs[0]) * hooks->list_length);
		while (pair) {
			if (pair->name == prepname)
				pairs[size++] = pair;
			pair = pair->next;
		}

		/* now execute the events we have, if they haven't been removed during our calls */
		for (i=0;i<size;i++) {
			pair = pairs[i];
			if ((pair->flags & CAMEL_HOOK_PAIR_REMOVED) == 0)
				(pair->func) (obj, event_data, pair->data);
		}
		hooks->depth--;

		/* and if we're out of any events, then clean up any pending removes */
		if (hooks->depth == 0 && (hooks->flags & CAMEL_HOOK_PAIR_REMOVED)) {
			parent = (CamelHookPair *)&hooks->list;
			pair = parent->next;
			while (pair) {
				if (pair->flags & CAMEL_HOOK_PAIR_REMOVED) {
					parent->next = pair->next;
					g_free(pair);
					hooks->list_length--;
				} else {
					parent = pair;
				}
				pair = parent->next;
			}
			hooks->flags &= ~CAMEL_HOOK_PAIR_REMOVED;
		}
	}

	camel_object_unget_hooks(obj);
	camel_object_unref(obj);
}

/* ** Static helpers ****************************************************** */

static gboolean
shared_is_of_type (CamelObjectShared * sh, CamelType ctype, gboolean is_obj)
{
	CamelTypeInfo *type_info;
	gchar *targtype;

	if (is_obj)
		targtype = "instance";
	else
		targtype = "classdata";

	if (ctype == CAMEL_INVALID_TYPE) {
		g_warning
			("shared_is_of_type: trying to cast to CAMEL_INVALID_TYPE");
		return FALSE;
	}

	if (sh == NULL) {
		g_warning
			("shared_is_of_type: trying to cast NULL to %s of `%s'",
			 targtype, camel_type_to_name (ctype));
		return FALSE;
	}

	if (sh->magic == CAMEL_OBJECT_FINALIZED_VALUE) {
		g_warning
			("shared_is_of_type: trying to cast finalized instance "
			 "of `%s' into %s of `%s'",
			 camel_type_to_name (sh->type), targtype,
			 camel_type_to_name (ctype));
		return FALSE;
	}

	if (sh->magic == CAMEL_OBJECT_CLASS_FINALIZED_VALUE) {
		g_warning
			("shared_is_of_type: trying to cast finalized classdata "
			 "of `%s' into %s of `%s'",
			 camel_type_to_name (sh->type), targtype,
			 camel_type_to_name (ctype));
		return FALSE;
	}

	if (is_obj) {
		if (sh->magic == CAMEL_OBJECT_CLASS_MAGIC_VALUE) {
			g_warning
				("shared_is_of_type: trying to cast classdata "
				 "of `%s' into instance of `%s'",
				 camel_type_to_name (sh->type),
				 camel_type_to_name (ctype));
			return FALSE;
		}

		if (sh->magic != CAMEL_OBJECT_MAGIC_VALUE) {
			g_warning
				("shared_is_of_type: trying to cast junk data "
				 "into instance of `%s'",
				 camel_type_to_name (ctype));
			return FALSE;
		}
	} else {
		if (sh->magic == CAMEL_OBJECT_MAGIC_VALUE) {
			g_warning
				("shared_is_of_type: trying to cast instance "
				 "of `%s' into classdata of `%s'",
				 camel_type_to_name (sh->type),
				 camel_type_to_name (ctype));
			return FALSE;
		}

		if (sh->magic != CAMEL_OBJECT_CLASS_MAGIC_VALUE) {
			g_warning
				("shared_is_of_type: trying to cast junk data "
				 "into classdata of `%s'",
				 camel_type_to_name (ctype));
			return FALSE;
		}
	}

	camel_type_lock_up ();

	type_info =
		g_hash_table_lookup (ctype_to_typeinfo,
				     GINT_TO_POINTER (sh->type));

	if (type_info == NULL) {
		g_warning ("shared_is_of_type: seemingly valid %s has "
			   "bad type %d.", targtype, sh->type);
		camel_type_lock_down ();
		return FALSE;
	}

	while (type_info) {
		if (type_info->self == ctype) {
			camel_type_lock_down ();
			return TRUE;
		}

		type_info =
			g_hash_table_lookup (ctype_to_typeinfo,
					     GINT_TO_POINTER (type_info->
							      parent));
	}

	/* this isn't an error, e.g. CAMEL_IS_FOLDER(folder), its upto  the
	   caller to handle the false case */
	/*g_warning
		("shared_is_of_type: %s of `%s' (@%p) is not also %s of `%s'",
		 targtype, camel_type_to_name (sh->type), sh, targtype,
		 camel_type_to_name (ctype));*/

	camel_type_lock_down ();
	return FALSE;
}

static void
make_global_classfuncs (CamelTypeInfo * type_info)
{
	CamelObjectClass *funcs;
	GSList *parents;
	GSList *head;

	g_assert (type_info);

	funcs = g_malloc0 (type_info->classfuncs_size);
	funcs->s.type = type_info->self;

	type_info->global_classfuncs = funcs;

	parents = NULL;
	while (type_info) {
		parents = g_slist_prepend (parents, type_info);
		type_info =
			g_hash_table_lookup (ctype_to_typeinfo,
					     GINT_TO_POINTER (type_info->
							      parent));
	}

	head = parents;

	for (; parents && parents->data; parents = parents->next) {
		CamelTypeInfo *thisinfo;

		thisinfo = parents->data;
		if (thisinfo->class_init)
			(thisinfo->class_init) (funcs);
	}

	g_slist_free (head);
}
