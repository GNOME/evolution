/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 * Author:
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "camel-object.h"
#include "camel-file-utils.h"

#include <e-util/e-memory.h>
#include <e-util/e-msgport.h>

#define d(x)
#define b(x) 			/* object bag */
#define h(x) 			/* hooks */

/* I just mashed the keyboard for these... */
#define CAMEL_OBJECT_MAGIC           	 0x77A344ED
#define CAMEL_OBJECT_CLASS_MAGIC     	 0xEE26A997
#define CAMEL_OBJECT_FINALISED_MAGIC       0x84AC365F
#define CAMEL_OBJECT_CLASS_FINALISED_MAGIC 0x7621ABCD

/* ** Quickie type system ************************************************* */

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

	unsigned int id:30;
	unsigned int flags:2;	/* removed, etc */

	const char *name;	/* points to the key field in the classes preplist, static memory */
	union {
		CamelObjectEventHookFunc event;
		CamelObjectEventPrepFunc prep;
		char *filename;
	} func;
	void *data;
} CamelHookPair;

struct _CamelObjectBagKey {
	struct _CamelObjectBagKey *next;

	void *key;		/* the key reserved */
	int waiters;		/* count of threads waiting for key */
	pthread_t owner;	/* the thread that has reserved the bag for a new entry */
	sem_t reserve_sem;	/* used to track ownership */
};

struct _CamelObjectBag {
	GHashTable *object_table; /* object by key */
	GHashTable *key_table;	/* key by object */
	GEqualFunc equal_key;
	CamelCopyFunc copy_key;
	GFreeFunc free_key;

	struct _CamelObjectBagKey *reserved;
};

/* used to tag a bag hookpair */
static const char *bag_name = "object:bag";

/* meta-data stuff */
static void co_metadata_free(CamelObject *obj, CamelObjectMeta *meta);
static CamelObjectMeta *co_metadata_get(CamelObject *obj);
static CamelHookPair *co_metadata_pair(CamelObject *obj, int create);

static const char *meta_name = "object:meta";
#define CAMEL_OBJECT_STATE_FILE_MAGIC "CLMD"

/* ********************************************************************** */

static CamelHookList *camel_object_get_hooks(CamelObject *o);
static void camel_object_free_hooks(CamelObject *o);
static void camel_object_bag_remove_unlocked(CamelObjectBag *inbag, CamelObject *o, CamelHookList *hooks);

#define camel_object_unget_hooks(o) (e_mutex_unlock((CAMEL_OBJECT(o)->hooks->lock)))


/* ********************************************************************** */

static pthread_mutex_t chunks_lock = PTHREAD_MUTEX_INITIALIZER;

static EMemChunk *pair_chunks;
static EMemChunk *hook_chunks;
static unsigned int pair_id = 1;

static EMutex *type_lock;

static GHashTable *type_table;
static EMemChunk *type_chunks;

CamelType camel_object_type;

#define P_LOCK(l) (pthread_mutex_lock(&l))
#define P_UNLOCK(l) (pthread_mutex_unlock(&l))
#define E_LOCK(l) (e_mutex_lock(l))
#define E_UNLOCK(l) (e_mutex_unlock(l))
#define CLASS_LOCK(k) (g_mutex_lock((((CamelObjectClass *)k)->lock)))
#define CLASS_UNLOCK(k) (g_mutex_unlock((((CamelObjectClass *)k)->lock)))


static struct _CamelHookPair *
pair_alloc(void)
{
	CamelHookPair *pair;

	P_LOCK(chunks_lock);
	pair = e_memchunk_alloc(pair_chunks);
	pair->id = pair_id++;
	if (pair_id == 0)
		pair_id = 1;
	P_UNLOCK(chunks_lock);

	return pair;
}

static void
pair_free(CamelHookPair *pair)
{
	g_assert(pair_chunks != NULL);

	P_LOCK(chunks_lock);
	e_memchunk_free(pair_chunks, pair);
	P_UNLOCK(chunks_lock);
}

static struct _CamelHookList *
hooks_alloc(void)
{
	CamelHookList *hooks;

	P_LOCK(chunks_lock);
	hooks = e_memchunk_alloc(hook_chunks);
	P_UNLOCK(chunks_lock);

	return hooks;
}

static void
hooks_free(CamelHookList *hooks)
{
	g_assert(hook_chunks != NULL);

	P_LOCK(chunks_lock);
	e_memchunk_free(hook_chunks, hooks);
	P_UNLOCK(chunks_lock);
}

/* not checked locked, who cares, only required for people that want to redefine root objects */
void
camel_type_init(void)
{
	static int init = FALSE;

	if (init)
		return;

	init = TRUE;
	pair_chunks = e_memchunk_new(16, sizeof(CamelHookPair));
	hook_chunks = e_memchunk_new(16, sizeof(CamelHookList));
	type_lock = e_mutex_new(E_MUTEX_REC);
	type_chunks = e_memchunk_new(32, sizeof(CamelType));
	type_table = g_hash_table_new(NULL, NULL);
}

/* ************************************************************************ */

/* Should this return the object to the caller? */
static void
cobject_init(CamelObject *o, CamelObjectClass *klass)
{
	o->klass = klass;
	o->magic = CAMEL_OBJECT_MAGIC;
	o->ref_count = 1;
	o->flags = 0;
}

static void
cobject_finalise(CamelObject *o)
{
	/*printf("%p: finalise %s\n", o, o->klass->name);*/

	g_assert(o->ref_count == 0);

	camel_object_free_hooks(o);

	o->magic = CAMEL_OBJECT_FINALISED_MAGIC;
	o->klass = NULL;
}

static int
cobject_getv(CamelObject *o, CamelException *ex, CamelArgGetV *args)
{
	int i;
	guint32 tag;

	for (i=0;i<args->argc;i++) {
		CamelArgGet *arg = &args->argv[i];

		tag = arg->tag;

		switch (tag & CAMEL_ARG_TAG) {
		case CAMEL_OBJECT_ARG_DESCRIPTION:
			*arg->ca_str = (char *)o->klass->name;
			break;
		case CAMEL_OBJECT_ARG_METADATA:
			*arg->ca_ptr = co_metadata_get(o);
			break;
		case CAMEL_OBJECT_ARG_STATE_FILE: {
			CamelHookPair *pair = co_metadata_pair(o, FALSE);

			if (pair) {
				*arg->ca_str = g_strdup(pair->func.filename);
				camel_object_unget_hooks(o);
			}
			break; }
		}
	}

	/* could have flags or stuff here? */
	return 0;
}

static int
cobject_setv(CamelObject *o, CamelException *ex, CamelArgV *args)
{
	int i;
	guint32 tag;

	for (i=0;i<args->argc;i++) {
		CamelArg *arg = &args->argv[i];

		tag = arg->tag;

		switch (tag & CAMEL_ARG_TAG) {
		case CAMEL_OBJECT_ARG_STATE_FILE: {
			CamelHookPair *pair;

			/* We store the filename on the meta-data hook-pair */
			pair = co_metadata_pair(o, TRUE);
			g_free(pair->func.filename);
			pair->func.filename = g_strdup(arg->ca_str);
			camel_object_unget_hooks(o);
			break; }
		}
	}

	/* could have flags or stuff here? */
	return 0;
}

static void
cobject_free(CamelObject *o, guint32 tag, void *value)
{
	switch(tag & CAMEL_ARG_TAG) {
	case CAMEL_OBJECT_ARG_METADATA:
		co_metadata_free(o, value);
		break;
	case CAMEL_OBJECT_ARG_STATE_FILE:
		g_free(value);
		break;
	case CAMEL_OBJECT_ARG_PERSISTENT_PROPERTIES:
		g_slist_free((GSList *)value);
		break;
	}
}

static char *
cobject_meta_get(CamelObject *obj, const char * name)
{
	CamelHookPair *pair;
	CamelObjectMeta *meta;
	char *res = NULL;

	g_return_val_if_fail(CAMEL_IS_OBJECT (obj), 0);
	g_return_val_if_fail(name != NULL, 0);

	pair = co_metadata_pair(obj, FALSE);
	if (pair) {
		meta = pair->data;
		while (meta) {
			if (!strcmp(meta->name, name)) {
				res = g_strdup(meta->value);
				break;
			}
			meta = meta->next;
		}
		camel_object_unget_hooks(obj);
	}
	
	return res;
}

static gboolean
cobject_meta_set(CamelObject *obj, const char * name, const char *value)
{
	CamelHookPair *pair;
	int changed = FALSE;
	CamelObjectMeta *meta, *metap;

	g_return_val_if_fail(CAMEL_IS_OBJECT (obj), FALSE);
	g_return_val_if_fail(name != NULL, FALSE);

	if (obj->hooks == NULL && value == NULL)
		return FALSE;

	pair = co_metadata_pair(obj, TRUE);
	meta = pair->data;
	metap = (CamelObjectMeta *)&pair->data;
	while (meta) {
		if (!strcmp(meta->name, name))
			break;
		metap = meta;
		meta = meta->next;
	}

	/* TODO: The camelobjectmeta structure is identical to
	   CamelTag, they could be merged or share common code */
	if (meta == NULL) {
		if (value == NULL)
			goto done;
		meta = g_malloc(sizeof(*meta) + strlen(name));
		meta->next = pair->data;
		pair->data = meta;
		strcpy(meta->name, name);
		meta->value = g_strdup(value);
		changed = TRUE;
	} else if (value == NULL) {
		metap->next = meta->next;
		g_free(meta->value);
		g_free(meta);
		changed = TRUE;
	} else if (strcmp(meta->value, value) != 0) {
		g_free(meta->value);
		meta->value = g_strdup(value);
		changed = TRUE;
	}

done:
	camel_object_unget_hooks(obj);

	return changed;
}

/* State file for CamelObject data.  Any later versions should only append data.

   version:uint32

   Version 0 of the file:

   version:uint32 = 0
   count:uint32  				-- count of meta-data items
   ( name:string value:string ) *count		-- meta-data items

   Version 1 of the file adds:
   count:uint32					-- count of persistent properties
   ( tag:uing32 value:tagtype ) *count		-- persistent properties

*/

static int
cobject_state_read(CamelObject *obj, FILE *fp)
{
	guint32 i, count, version;

	/* NB: for later versions, just check the version is 1 .. known version */
	if (camel_file_util_decode_uint32(fp, &version) == -1
	    || version > 1
	    || camel_file_util_decode_uint32(fp, &count) == -1)
		return -1;

	for (i=0;i<count;i++) {
		char *name = NULL, *value = NULL;
			
		if (camel_file_util_decode_string(fp, &name) == 0
		    && camel_file_util_decode_string(fp, &value) == 0) {
			camel_object_meta_set(obj, name, value);
			g_free(name);
			g_free(value);
		} else {
			g_free(name);
			g_free(value);

			return -1;
		}
	}

	if (version > 0) {
		CamelArgV *argv;

		if (camel_file_util_decode_uint32(fp, &count) == -1
			|| count == 0 || count > 1024) {
			/* maybe it was just version 0 afterall */
			return 0;
		}
		
		/* we batch up the properties and set them in one go */
		if (!(argv = g_try_malloc (sizeof (*argv) + (count - CAMEL_ARGV_MAX) * sizeof (argv->argv[0]))))
			return -1;
		
		argv->argc = 0;
		for (i=0;i<count;i++) {
			if (camel_file_util_decode_uint32(fp, &argv->argv[argv->argc].tag) == -1)
				goto cleanup;

			/* so far,only do strings and ints, doubles could be added,
			   object's would require a serialisation interface */

			switch(argv->argv[argv->argc].tag & CAMEL_ARG_TYPE) {
			case CAMEL_ARG_INT:
			case CAMEL_ARG_BOO:
				if (camel_file_util_decode_uint32(fp, &argv->argv[argv->argc].ca_int) == -1)
					goto cleanup;
				break;
			case CAMEL_ARG_STR:
				if (camel_file_util_decode_string(fp, &argv->argv[argv->argc].ca_str) == -1)
					goto cleanup;
				break;
			default:
				goto cleanup;
			}

			argv->argc++;
		}

		camel_object_setv(obj, NULL, argv);
	cleanup:
		for (i=0;i<argv->argc;i++) {
			if ((argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
				g_free(argv->argv[i].ca_str);
		}
		g_free(argv);
	}

	return 0;
}

/* TODO: should pass exception around */
static int
cobject_state_write(CamelObject *obj, FILE *fp)
{
	gint32 count, i;
	CamelObjectMeta *meta = NULL, *scan;
	int res = -1;
	GSList *props = NULL, *l;
	CamelArgGetV *arggetv = NULL;
	CamelArgV *argv = NULL;

	camel_object_get(obj, NULL, CAMEL_OBJECT_METADATA, &meta, NULL);

	count = 0;
	scan = meta;
	while (scan) {
		count++;
		scan = scan->next;
	}

	/* current version is 1 */
	if (camel_file_util_encode_uint32(fp, 1) == -1
	    || camel_file_util_encode_uint32(fp, count) == -1)
		goto abort;

	scan = meta;
	while (scan) {
		if (camel_file_util_encode_string(fp, scan->name) == -1
		    || camel_file_util_encode_string(fp, scan->value) == -1)
			goto abort;
		scan = scan->next;
	}

	camel_object_get(obj, NULL, CAMEL_OBJECT_PERSISTENT_PROPERTIES, &props, NULL);

	/* we build an arggetv to query the object atomically,
	   we also need an argv to store the results - bit messy */

	count = g_slist_length(props);

	arggetv = g_malloc0(sizeof(*arggetv) + (count - CAMEL_ARGV_MAX) * sizeof(arggetv->argv[0]));
	argv = g_malloc0(sizeof(*argv) + (count - CAMEL_ARGV_MAX) * sizeof(argv->argv[0]));
	l = props;
	i = 0;
	while (l) {
		CamelProperty *prop = l->data;

		argv->argv[i].tag = prop->tag;
		arggetv->argv[i].tag = prop->tag;
		arggetv->argv[i].ca_ptr = &argv->argv[i].ca_ptr;

		i++;
		l = l->next;
	}
	arggetv->argc = i;
	argv->argc = i;

	camel_object_getv(obj, NULL, arggetv);

	if (camel_file_util_encode_uint32(fp, count) == -1)
		goto abort;

	for (i=0;i<argv->argc;i++) {
		CamelArg *arg = &argv->argv[i];

		if (camel_file_util_encode_uint32(fp, arg->tag) == -1)
			goto abort;

		switch (arg->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_INT:
		case CAMEL_ARG_BOO:
			if (camel_file_util_encode_uint32(fp, arg->ca_int) == -1)
				goto abort;
			break;
		case CAMEL_ARG_STR:
			if (camel_file_util_encode_string(fp, arg->ca_str) == -1)
				goto abort;
			break;
		}
	}

	res = 0;
abort:
	for (i=0;i<argv->argc;i++) {
		CamelArg *arg = &argv->argv[i];

		if ((argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
			camel_object_free(obj, arg->tag, arg->ca_str);
	}

	g_free(argv);
	g_free(arggetv);

	if (props)
		camel_object_free(obj, CAMEL_OBJECT_PERSISTENT_PROPERTIES, props);

	if (meta)
		camel_object_free(obj, CAMEL_OBJECT_METADATA, meta);

	return res;
}


static void
cobject_class_init(CamelObjectClass *klass)
{
	klass->magic = CAMEL_OBJECT_CLASS_MAGIC;

	klass->getv = cobject_getv;
	klass->setv = cobject_setv;
	klass->free = cobject_free;

	klass->meta_get = cobject_meta_get;
	klass->meta_set = cobject_meta_set;
	klass->state_read = cobject_state_read;
	klass->state_write = cobject_state_write;

	camel_object_class_add_event(klass, "finalize", NULL);
	camel_object_class_add_event(klass, "meta_changed", NULL);
}

static void
cobject_class_finalise(CamelObjectClass * klass)
{
	klass->magic = CAMEL_OBJECT_CLASS_FINALISED_MAGIC;

	g_free(klass);
}

CamelType
camel_object_get_type(void)
{
	if (camel_object_type == CAMEL_INVALID_TYPE) {
		camel_type_init();

		camel_object_type = camel_type_register(NULL, "CamelObject", /*, 0, 0*/
							sizeof(CamelObject), sizeof(CamelObjectClass),
							cobject_class_init, cobject_class_finalise,
							cobject_init, cobject_finalise);
	}

	return camel_object_type;
}

static void
camel_type_class_init(CamelObjectClass *klass, CamelObjectClass *type)
{
	if (type->parent)
		camel_type_class_init(klass, type->parent);

	if (type->klass_init)
		type->klass_init(klass);
}

CamelType
camel_type_register(CamelType parent, const char * name,
		    /*unsigned int ver, unsigned int rev,*/
		    size_t object_size, size_t klass_size,
		    CamelObjectClassInitFunc class_init,
		    CamelObjectClassFinalizeFunc class_finalise,
		    CamelObjectInitFunc object_init,
		    CamelObjectFinalizeFunc object_finalise)
{
	CamelObjectClass *klass;
	/*int offset;
	  size_t size;*/

	if (parent != NULL && parent->magic != CAMEL_OBJECT_CLASS_MAGIC) {
		g_warning("camel_type_register: invalid junk parent class for '%s'", name);
		return NULL;
	}

	E_LOCK(type_lock);

	/* Have to check creation, it might've happened in another thread before we got here */
	klass = g_hash_table_lookup(type_table, name);
	if (klass != NULL) {
		if (klass->klass_size != klass_size || klass->object_size != object_size
		    || klass->klass_init != class_init || klass->klass_finalise != class_finalise
		    || klass->init != object_init || klass->finalise != object_finalise) {
			g_warning("camel_type_register: Trying to re-register class '%s'", name);
			klass = NULL;
		}
		E_UNLOCK(type_lock);
		return klass;
	}

	/* this is for objects with no parent as part of their struct ('interfaces'?) */
	/*offset = parent?parent->klass_size:0;
	offset = (offset + 3) & (~3);

	size = offset + klass_size;

	klass = g_malloc0(size);

	klass->klass_size = size;
	klass->klass_data = offset;

	offset = parent?parent->object_size:0;
	offset = (offset + 3) & (~3);

	klass->object_size = offset + object_size;
	klass->object_data = offset;*/

	if (parent
	    && klass_size < parent->klass_size) {
		g_warning("camel_type_register: '%s' has smaller class size than parent '%s'", name, parent->name);
		E_UNLOCK(type_lock);
		return NULL;
	}

	klass = g_malloc0(klass_size);
	klass->klass_size = klass_size;
	klass->object_size = object_size;
	klass->lock = g_mutex_new();
	klass->instance_chunks = e_memchunk_new(8, object_size);
	
	klass->parent = parent;
	if (parent) {
		klass->next = parent->child;
		parent->child = klass;
	}
	klass->name = name;

	/*klass->version = ver;
	  klass->revision = rev;*/

	klass->klass_init = class_init;
	klass->klass_finalise = class_finalise;

	klass->init = object_init;
	klass->finalise = object_finalise;

	/* setup before class init, incase class init func uses the type or looks it up ? */
	g_hash_table_insert(type_table, (void *)name, klass);

	camel_type_class_init(klass, klass);

	E_UNLOCK(type_lock);

	return klass;
}

static void
camel_object_init(CamelObject *o, CamelObjectClass *klass, CamelType type)
{
	if (type->parent)
		camel_object_init(o, klass, type->parent);

	if (type->init)
		type->init(o, klass);
}

CamelObject *
camel_object_new(CamelType type)
{
	CamelObject *o;

	if (type == NULL)
		return NULL;

	if (type->magic != CAMEL_OBJECT_CLASS_MAGIC)
		return NULL;

	CLASS_LOCK(type);

	o = e_memchunk_alloc0(type->instance_chunks);

#ifdef CAMEL_OBJECT_TRACK_INSTANCES
	if (type->instances)
		type->instances->prev = o;
	o->next = type->instances;
	o->prev = NULL;
	type->instances = o;
#endif

	CLASS_UNLOCK(type);

	camel_object_init(o, type, type);

	d(printf("%p: new %s()\n", o, o->klass->name));

	return o;
}

void
camel_object_ref(void *vo)
{
	register CamelObject *o = vo;

	g_return_if_fail(CAMEL_IS_OBJECT(o));

	E_LOCK(type_lock);

	o->ref_count++;
	d(printf("%p: ref %s(%d)\n", o, o->klass->name, o->ref_count));

	E_UNLOCK(type_lock);
}

void
camel_object_unref(void *vo)
{
	register CamelObject *o = vo;
	register CamelObjectClass *klass, *k;
	CamelHookList *hooks = NULL;

	g_return_if_fail(CAMEL_IS_OBJECT(o));
	
	klass = o->klass;

	if (o->hooks)
		hooks = camel_object_get_hooks(o);

	E_LOCK(type_lock);

	o->ref_count--;

	d(printf("%p: unref %s(%d)\n", o, o->klass->name, o->ref_count));

	if (o->ref_count > 0
	    || (o->flags & CAMEL_OBJECT_DESTROY)) {
		E_UNLOCK(type_lock);
		if (hooks)
			camel_object_unget_hooks(o);
		return;
	}

	o->flags |= CAMEL_OBJECT_DESTROY;

	if (hooks)
		camel_object_bag_remove_unlocked(NULL, o, hooks);

	E_UNLOCK(type_lock);

	if (hooks)
		camel_object_unget_hooks(o);

	camel_object_trigger_event(o, "finalize", NULL);

	k = klass;
	while (k) {
		if (k->finalise)
			k->finalise(o);
		k = k->parent;
	}

	o->magic = CAMEL_OBJECT_FINALISED_MAGIC;

	CLASS_LOCK(klass);
#ifdef CAMEL_OBJECT_TRACK_INSTANCES
	if (o->prev)
		o->prev->next = o->next;
	else
		klass->instances = o->next;
	if (o->next)
		o->next->prev = o->prev;
#endif
	e_memchunk_free(klass->instance_chunks, o);
	CLASS_UNLOCK(klass);
}

const char *
camel_type_to_name(CamelType type)
{
	if (type == NULL)
		return "(NULL class)";

	if (type->magic == CAMEL_OBJECT_CLASS_MAGIC)
		return type->name;

	return "(Junk class)";
}

CamelType camel_name_to_type(const char *name)
{
	/* TODO: Load a class off disk (!) */

	return g_hash_table_lookup(type_table, name);
}

static char *
desc_data(CamelObject *o, guint32 ok)
{
	char *what;

	if (o == NULL)
		what = g_strdup("NULL OBJECT");
	else if (o->magic == ok)
		what = NULL;
	else if (o->magic == CAMEL_OBJECT_MAGIC)
		what = g_strdup_printf("CLASS '%s'", ((CamelObjectClass *)o)->name);
	else if (o->magic == CAMEL_OBJECT_CLASS_MAGIC)
		what = g_strdup_printf("CLASS '%s'", ((CamelObjectClass *)o)->name);
	else if (o->magic == CAMEL_OBJECT_FINALISED_MAGIC)
		what = g_strdup_printf("finalised OBJECT");
	else if (o->magic == CAMEL_OBJECT_CLASS_FINALISED_MAGIC)
		what = g_strdup_printf("finalised CLASS");
	else 
		what = g_strdup_printf("junk data");

	return what;
}

#define check_magic(o, ctype, omagic) \
	( ((CamelObject *)(o))->magic == (omagic) \
	&& (ctype)->magic == CAMEL_OBJECT_CLASS_MAGIC) \
	? 1 : check_magic_fail(o, ctype, omagic)

static gboolean
check_magic_fail(void *o, CamelType ctype, guint32 omagic)
{
	char *what, *to;

	what = desc_data(o, omagic);
	to = desc_data((CamelObject *)ctype, CAMEL_OBJECT_CLASS_MAGIC);

	if (what || to) {
		if (what == NULL) {
			if (omagic == CAMEL_OBJECT_MAGIC)
				what = g_strdup_printf("OBJECT '%s'", ((CamelObject *)o)->klass->name);
			else
				what = g_strdup_printf("OBJECT '%s'", ((CamelObjectClass *)o)->name);
		}		
		if (to == NULL)
			to = g_strdup_printf("OBJECT '%s'", ctype->name);
		g_warning("Trying to check %s is %s", what, to);
		g_free(what);
		g_free(to);

		return FALSE;
	}

	return TRUE;
}

gboolean
camel_object_is(CamelObject *o, CamelType ctype)
{
	CamelObjectClass *k;

	g_return_val_if_fail(check_magic(o, ctype, CAMEL_OBJECT_MAGIC), FALSE);

	k = o->klass;
	while (k) {
		if (k == ctype)
			return TRUE;
		k = k->parent;
	}

	return FALSE;
}

gboolean
camel_object_class_is(CamelObjectClass *k, CamelType ctype)
{
	g_return_val_if_fail(check_magic(k, ctype, CAMEL_OBJECT_CLASS_MAGIC), FALSE);

	while (k) {
		if (k == ctype)
			return TRUE;
		k = k->parent;
	}

	return FALSE;
}

CamelObject *
camel_object_cast(CamelObject *o, CamelType ctype)
{
	CamelObjectClass *k;

	g_return_val_if_fail(check_magic(o, ctype, CAMEL_OBJECT_MAGIC), NULL);

	k = o->klass;
	while (k) {
		if (k == ctype)
			return o;
		k = k->parent;
	}

	g_warning("Object %p (class '%s') doesn't have '%s' in its hierarchy", o, o->klass->name, ctype->name);

	return NULL;
}

CamelObjectClass *
camel_object_class_cast(CamelObjectClass *k, CamelType ctype)
{
	CamelObjectClass *r = k;

	g_return_val_if_fail(check_magic(k, ctype, CAMEL_OBJECT_CLASS_MAGIC), NULL);

	while (k) {
		if (k == ctype)
			return r;
		k = k->parent;
	}

	g_warning("Class '%s' doesn't have '%s' in its hierarchy", r->name, ctype->name);

	return NULL;
}

void
camel_object_class_add_event(CamelObjectClass *klass, const char *name, CamelObjectEventPrepFunc prep)
{
	CamelHookPair *pair;

	g_return_if_fail (name);

	pair = klass->hooks;
	while (pair) {
		if (strcmp(pair->name, name) == 0) {
			g_warning("camel_object_class_add_event: `%s' is already declared for '%s'\n",
				  name, klass->name);
			return;
		}
		pair = pair->next;
	}

	pair = pair_alloc();
	pair->name = name;
	pair->func.prep = prep;
	pair->flags = 0;

	pair->next = klass->hooks;
	klass->hooks = pair;
}

/* free hook data */
static void
camel_object_free_hooks(CamelObject *o)
{
	CamelHookPair *pair, *next;

	if (o->hooks) {
		g_assert(o->hooks->depth == 0);
		g_assert((o->hooks->flags & CAMEL_HOOK_PAIR_REMOVED) == 0);

		pair = o->hooks->list;
		while (pair) {
			next = pair->next;

			if (pair->name == meta_name) {
				co_metadata_free(o, pair->data);
				g_free(pair->func.filename);
			}

			pair_free(pair);
			pair = next;
		}
		e_mutex_destroy(o->hooks->lock);
		hooks_free(o->hooks);
		o->hooks = NULL;
	}
}

/* return (allocate if required) the object's hook list, locking at the same time */
static CamelHookList *
camel_object_get_hooks(CamelObject *o)
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	CamelHookList *hooks;

	/* if we have it, we dont have to do any other locking,
	   otherwise use a global lock to setup the object's hook data */
	if (o->hooks == NULL) {
		pthread_mutex_lock(&lock);
		if (o->hooks == NULL) {
			hooks = hooks_alloc();
			hooks->lock = e_mutex_new(E_MUTEX_REC);
			hooks->flags = 0;
			hooks->depth = 0;
			hooks->list_length = 0;
			hooks->list = NULL;
			o->hooks = hooks;
		}
		pthread_mutex_unlock(&lock);
	}
	
	e_mutex_lock(o->hooks->lock);
	
	return o->hooks;	
}

unsigned int
camel_object_hook_event(void *vo, const char * name, CamelObjectEventHookFunc func, void *data)
{
	CamelObject *obj = vo;
	CamelHookPair *pair, *hook;
	CamelHookList *hooks;
	int id;

	g_return_val_if_fail(CAMEL_IS_OBJECT (obj), 0);
	g_return_val_if_fail(name != NULL, 0);
	g_return_val_if_fail(func != NULL, 0);

	hook = obj->klass->hooks;
	while (hook) {
		if (strcmp(hook->name, name) == 0)
			goto setup;
		hook = hook->next;
	}

	g_warning("camel_object_hook_event: trying to hook event `%s' in class `%s' with no defined events.",
		  name, obj->klass->name);

	return 0;

setup:
	/* setup hook pair */
	pair = pair_alloc();
	pair->name = hook->name;	/* effectively static! */
	pair->func.event = func;
	pair->data = data;
	pair->flags = 0;
	id = pair->id;

	/* get the hook list object, locked, link in new event hook, unlock */
	hooks = camel_object_get_hooks(obj);
	pair->next = hooks->list;
	hooks->list = pair;
	hooks->list_length++;
	camel_object_unget_hooks(obj);

	h(printf("%p hook event '%s' %p %p = %d\n", vo, name, func, data, id));

	return id;
}

void
camel_object_remove_event(void *vo, unsigned int id)
{
	CamelObject *obj = vo;
	CamelHookList *hooks;
	CamelHookPair *pair, *parent;

	g_return_if_fail (CAMEL_IS_OBJECT (obj));
	g_return_if_fail (id != 0);

	if (obj->hooks == NULL) {
		g_warning("camel_object_unhook_event: trying to unhook `%d` from an instance of `%s' with no hooks",
			  id, obj->klass->name);
		return;
	}

	h(printf("%p remove event %d\n", vo, id));

	/* scan hooks for this event, remove it, or flag it if we're busy */
	hooks = camel_object_get_hooks(obj);
	parent = (CamelHookPair *)&hooks->list;
	pair = parent->next;
	while (pair) {
		if (pair->id == id
		    && (pair->flags & CAMEL_HOOK_PAIR_REMOVED) == 0) {
			if (hooks->depth > 0) {
				pair->flags |= CAMEL_HOOK_PAIR_REMOVED;
				hooks->flags |= CAMEL_HOOK_PAIR_REMOVED;
			} else {
				parent->next = pair->next;
				pair_free(pair);
				hooks->list_length--;
			}
			camel_object_unget_hooks(obj);
			return;
		}
		parent = pair;
		pair = pair->next;
	}
	camel_object_unget_hooks(obj);

	g_warning("camel_object_unhook_event: cannot find hook id %d in instance of `%s'",
		  id, obj->klass->name);
}

void
camel_object_unhook_event(void *vo, const char * name, CamelObjectEventHookFunc func, void *data)
{
	CamelObject *obj = vo;
	CamelHookList *hooks;
	CamelHookPair *pair, *parent;

	g_return_if_fail (CAMEL_IS_OBJECT (obj));
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL);

	if (obj->hooks == NULL) {
		g_warning("camel_object_unhook_event: trying to unhook `%s` from an instance of `%s' with no hooks",
			  name, obj->klass->name);
		return;
	}

	h(printf("%p unhook event '%s' %p %p\n", vo, name, func, data));

	/* scan hooks for this event, remove it, or flag it if we're busy */
	hooks = camel_object_get_hooks(obj);
	parent = (CamelHookPair *)&hooks->list;
	pair = parent->next;
	while (pair) {
		if (pair->func.event == func
		    && pair->data == data
		    && strcmp(pair->name, name) == 0
		    && (pair->flags & CAMEL_HOOK_PAIR_REMOVED) == 0) {
			if (hooks->depth > 0) {
				pair->flags |= CAMEL_HOOK_PAIR_REMOVED;
				hooks->flags |= CAMEL_HOOK_PAIR_REMOVED;
			} else {
				parent->next = pair->next;
				pair_free(pair);
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
		  func, data, obj->klass->name, name);
}

void
camel_object_trigger_event(void *vo, const char * name, void *event_data)
{
	CamelObject *obj = vo;
	CamelHookList *hooks;
	CamelHookPair *pair, **pairs, *parent, *hook;
	int i, size;
	const char *prepname;

	g_return_if_fail (CAMEL_IS_OBJECT (obj));
	g_return_if_fail (name);

	hook = obj->klass->hooks;
	while (hook) {
		if (strcmp(hook->name, name) == 0)
			goto trigger;
		hook = hook->next;
	}

	g_warning("camel_object_trigger_event: trying to trigger unknown event `%s' in class `%s'",
		  name, obj->klass->name);

	return;

trigger:
	/* try prep function, if false, then quit */
	if (hook->func.prep != NULL && !hook->func.prep(obj, event_data))
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
		prepname = hook->name;
		while (pair) {
			if (pair->name == prepname)
				pairs[size++] = pair;
			pair = pair->next;
		}

		/* now execute the events we have, if they haven't been removed during our calls */
		for (i=size-1;i>=0;i--) {
			pair = pairs[i];
			if ((pair->flags & CAMEL_HOOK_PAIR_REMOVED) == 0)
				(pair->func.event) (obj, event_data, pair->data);
		}
		hooks->depth--;

		/* and if we're out of any events, then clean up any pending removes */
		if (hooks->depth == 0 && (hooks->flags & CAMEL_HOOK_PAIR_REMOVED)) {
			parent = (CamelHookPair *)&hooks->list;
			pair = parent->next;
			while (pair) {
				if (pair->flags & CAMEL_HOOK_PAIR_REMOVED) {
					parent->next = pair->next;
					pair_free(pair);
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

/* get/set arg methods */
int camel_object_set(void *vo, CamelException *ex, ...)
{
	CamelArgV args;
	CamelObject *o = vo;
	CamelObjectClass *klass = o->klass;
	int ret = 0;

	g_return_val_if_fail(CAMEL_IS_OBJECT(o), -1);

	camel_argv_start(&args, ex);

	while (camel_argv_build(&args) && ret == 0)
		ret = klass->setv(o, ex, &args);
	if (ret == 0)
		ret = klass->setv(o, ex, &args);

	camel_argv_end(&args);

	return ret;
}

int camel_object_setv(void *vo, CamelException *ex, CamelArgV *args)
{
	g_return_val_if_fail(CAMEL_IS_OBJECT(vo), -1);

	return ((CamelObject *)vo)->klass->setv(vo, ex, args);
}

int camel_object_get(void *vo, CamelException *ex, ...)
{
	CamelObject *o = vo;
	CamelArgGetV args;
	CamelObjectClass *klass = o->klass;
	int ret = 0;

	g_return_val_if_fail(CAMEL_IS_OBJECT(o), -1);

	camel_argv_start(&args, ex);

	while (camel_arggetv_build(&args) && ret == 0)
		ret = klass->getv(o, ex, &args);
	if (ret == 0)
		ret = klass->getv(o, ex, &args);

	camel_argv_end(&args);

	return ret;
}

void *camel_object_get_ptr(void *vo, CamelException *ex, int tag)
{
	CamelObject *o = vo;
	CamelArgGetV args;
	CamelObjectClass *klass = o->klass;
	int ret = 0;
	void *val = NULL;

	g_return_val_if_fail(CAMEL_IS_OBJECT(o), NULL);
	g_return_val_if_fail((tag & CAMEL_ARG_TYPE) == CAMEL_ARG_OBJ
			     || (tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR
			     || (tag & CAMEL_ARG_TYPE) == CAMEL_ARG_PTR, 0);

	/* woefully inefficient, *shrug */
	args.argc = 1;
	args.argv[0].tag = tag;
	args.argv[0].ca_ptr = &val;

	ret = klass->getv(o, ex, &args);
	if (ret != 0)
		return NULL;
	else
		return val;
}

int camel_object_get_int(void *vo, CamelException *ex, int tag)
{
	CamelObject *o = vo;
	CamelArgGetV args;
	CamelObjectClass *klass = o->klass;
	int ret = 0;
	int val = 0;

	g_return_val_if_fail(CAMEL_IS_OBJECT(o), 0);
	g_return_val_if_fail((tag & CAMEL_ARG_TYPE) == CAMEL_ARG_INT
			     || (tag & CAMEL_ARG_TYPE) == CAMEL_ARG_BOO, 0);

	/* woefully inefficient, *shrug */
	args.argc = 1;
	args.argv[0].tag = tag;
	args.argv[0].ca_int = &val;

	ret = klass->getv(o, ex, &args);
	if (ret != 0)
		return 0;
	else
		return val;
}

int camel_object_getv(void *vo, CamelException *ex, CamelArgGetV *args)
{
	g_return_val_if_fail(CAMEL_IS_OBJECT(vo), -1);

	return ((CamelObject *)vo)->klass->getv(vo, ex, args);
}

/* NB: If this doesn't return NULL, then you must unget_hooks when done */
static CamelHookPair *
co_metadata_pair(CamelObject *obj, int create)
{
	CamelHookPair *pair;
	CamelHookList *hooks;

	if (obj->hooks == NULL && !create)
		return NULL;

	hooks = camel_object_get_hooks(obj);
	pair = hooks->list;
	while (pair) {
		if (pair->name == meta_name)
			return pair;

		pair = pair->next;
	}

	if (create) {
		pair = pair_alloc();
		pair->name = meta_name;
		pair->data = NULL;
		pair->flags = 0;
		pair->func.filename = NULL;
		pair->next = hooks->list;
		hooks->list = pair;
		hooks->list_length++;
	} else {
		camel_object_unget_hooks(obj);
	}

	return pair;
}

static CamelObjectMeta *
co_metadata_get(CamelObject *obj)
{
	CamelHookPair *pair;
	CamelObjectMeta *meta = NULL, *metaout = NULL, *metalast;

	pair = co_metadata_pair(obj, FALSE);
	if (pair) {
		meta = pair->data;

		while (meta) {
			CamelObjectMeta *m;

			m = g_malloc(sizeof(*metalast) + strlen(meta->name));
			m->next = NULL;
			strcpy(m->name, meta->name);
			m->value = g_strdup(meta->value);
			if (metaout == NULL)
				metalast = metaout = m;
			else {
				metalast->next = m;
				metalast = m;
			}
			meta = meta->next;
		}

		camel_object_unget_hooks(obj);
	}

	return metaout;
}

static void
co_metadata_free(CamelObject *obj, CamelObjectMeta *meta)
{
	while (meta) {
		CamelObjectMeta *metan = meta->next;

		g_free(meta->value);
		g_free(meta);
		meta = metan;
	}
}

/**
 * camel_object_meta_get:
 * @vo: 
 * @name: 
 * 
 * Get a meta-data on an object.
 * 
 * Return value: NULL if the meta-data is not set.
 **/
char *
camel_object_meta_get(void *vo, const char * name)
{
	CamelObject *obj = vo;

	g_return_val_if_fail(CAMEL_IS_OBJECT (obj), 0);
	g_return_val_if_fail(name != NULL, 0);

	return obj->klass->meta_get(obj, name);
}

/**
 * camel_object_meta_set:
 * @vo: 
 * @name: Name of meta-data.  Should be prefixed with class of setter.
 * @value: Value to set.  If NULL, then the meta-data is removed.
 * 
 * Set a meta-data item on an object.  If the object supports persistent
 * data, then the meta-data will be persistent across sessions.
 *
 * If the meta-data changes, is added, or removed, then a
 * "meta_changed" event will be triggered with the name of the changed
 * data.
 *
 * Return Value: TRUE if the setting caused a change to the object's
 * metadata.
 **/
gboolean
camel_object_meta_set(void *vo, const char * name, const char *value)
{
	CamelObject *obj = vo;

	g_return_val_if_fail(CAMEL_IS_OBJECT (obj), FALSE);
	g_return_val_if_fail(name != NULL, FALSE);

	if (obj->klass->meta_set(obj, name, value)) {
		camel_object_trigger_event(obj, "meta_changed", (void *)name);
		return TRUE;
	}

	return FALSE;
}

/**
 * camel_object_state_read:
 * @vo: 
 * 
 * Read persistent object state from object_set(CAMEL_OBJECT_STATE_FILE).
 * 
 * Return value: -1 on error.
 **/
int camel_object_state_read(void *vo)
{
	CamelObject *obj = vo;
	int res = -1;
	char *file;
	FILE *fp;
	char magic[4];

	camel_object_get(vo, NULL, CAMEL_OBJECT_STATE_FILE, &file, NULL);
	if (file == NULL)
		return 0;

	fp = fopen(file, "r");
	if (fp != NULL) {
		if (fread(magic, 4, 1, fp) == 1
		    && memcmp(magic, CAMEL_OBJECT_STATE_FILE_MAGIC, 4) == 0)
			res = obj->klass->state_read(obj, fp);
		else
			res = -1;
		fclose(fp);
	}

	camel_object_free(vo, CAMEL_OBJECT_STATE_FILE, file);

	return res;
}

/**
 * camel_object_state_write:
 * @vo: 
 * 
 * Write persistent state to the file as set by object_set(CAMEL_OBJECT_STATE_FILE).
 * 
 * Return value: -1 on error.
 **/
int camel_object_state_write(void *vo)
{
	CamelObject *obj = vo;
	int res = -1;
	char *file, *savename, *tmp;
	FILE *fp;

	camel_object_get(vo, NULL, CAMEL_OBJECT_STATE_FILE, &file, NULL);
	if (file == NULL)
		return 0;

	savename = camel_file_util_savename(file);
	tmp = strrchr(savename, '/');
	if (tmp) {
		*tmp = 0;
		camel_mkdir(savename, 0777);
		*tmp = '/';
	}
	fp = fopen(savename, "w");
	if (fp != NULL) {
		if (fwrite(CAMEL_OBJECT_STATE_FILE_MAGIC, 4, 1, fp) == 1
		    && obj->klass->state_write(obj, fp) == 0) {
			if (fclose(fp) == 0) {
				res = 0;
				rename(savename, file);
			}
		} else {
			fclose(fp);
		}
	} else {
		g_warning("Could not save object state file to '%s': %s", savename, g_strerror(errno));
	}

	g_free(savename);
	camel_object_free(vo, CAMEL_OBJECT_STATE_FILE, file);

	return res;
}

/* free an arg object, you can only free objects 1 at a time */
void camel_object_free(void *vo, guint32 tag, void *value)
{
	g_return_if_fail(CAMEL_IS_OBJECT(vo));

	/* We could also handle freeing of args differently

	   Add a 'const' bit to the arg type field,
	   specifying that the object should not be freed.
	   
	   And, add free handlers for any pointer objects which are
	   not const.  The free handlers would be hookpairs off of the
	   class.

	   Then we handle the basic types OBJ,STR here, and pass PTR
	   types to their appropriate handler, without having to pass
	   through the invocation heirarchy of the free method.

	   This would also let us copy and do other things with args
	   we can't do, but i can't see a use for that yet ...  */

	((CamelObject *)vo)->klass->free(vo, tag, value);
}

static void
object_class_dump_tree_rec(CamelType root, int depth)
{
	char *p;
#ifdef CAMEL_OBJECT_TRACK_INSTANCES
	struct _CamelObject *o;
#endif

	p = alloca(depth*2+1);
	memset(p, ' ', depth*2);
	p[depth*2] = 0;

	while (root) {
		CLASS_LOCK(root);
		printf("%sClass: %s\n", p, root->name);
		/*printf("%sVersion: %u.%u\n", p, root->version, root->revision);*/
		if (root->hooks) {
			CamelHookPair *pair = root->hooks;

			while (pair) {
				printf("%s  event '%s' prep %p\n", p, pair->name, pair->func.prep);
				pair = pair->next;
			}
		}
#ifdef CAMEL_OBJECT_TRACK_INSTANCES
		o = root->instances;
		while (o) {
			printf("%s instance %p [%d]\n", p, o, o->ref_count);
			/* todo: should lock hooks while it scans them */
			if (o->hooks) {
				CamelHookPair *pair = o->hooks->list;

				while (pair) {
					printf("%s  hook '%s' func %p data %p\n", p, pair->name, pair->func.event, pair->data);
					pair = pair->next;
				}
			}
			o = o->next;
		}
#endif
		CLASS_UNLOCK(root);

		if (root->child)
			object_class_dump_tree_rec(root->child, depth+1);

		root = root->next;
	}
}

void
camel_object_class_dump_tree(CamelType root)
{
	object_class_dump_tree_rec(root, 0);
}

/**
 * camel_object_bag_new:
 * @hash: 
 * @equal: 
 * @keycopy: 
 * @keyfree: 
 * 
 * Allocate a new object bag.  Object bag's are key'd hash tables of
 * camel-objects which can be updated atomically using transaction
 * semantics.
 * 
 * Return value: 
 **/
CamelObjectBag *
camel_object_bag_new(GHashFunc hash, GEqualFunc equal, CamelCopyFunc keycopy, GFreeFunc keyfree)
{
	CamelObjectBag *bag;

	bag = g_malloc(sizeof(*bag));
	bag->object_table = g_hash_table_new(hash, equal);
	bag->equal_key = equal;
	bag->copy_key = keycopy;
	bag->free_key = keyfree;
	bag->key_table = g_hash_table_new(NULL, NULL);
	bag->reserved = NULL;

	return bag;
}

static void
save_object(void *key, CamelObject *o, GPtrArray *objects)
{
	g_ptr_array_add(objects, o);
}

void
camel_object_bag_destroy(CamelObjectBag *bag)
{
	GPtrArray *objects = g_ptr_array_new();
	int i;

	g_assert(bag->reserved == NULL);

	g_hash_table_foreach(bag->object_table, (GHFunc)save_object, objects);
	for (i=0;i<objects->len;i++)
		camel_object_bag_remove(bag, objects->pdata[i]);
	
	g_ptr_array_free(objects, TRUE);
	g_hash_table_destroy(bag->object_table);
	g_hash_table_destroy(bag->key_table);
	g_free(bag);
}

/* must be called with type_lock held */
static void
co_bag_unreserve(CamelObjectBag *bag, const void *key)
{
	struct _CamelObjectBagKey *res, *resp;

	resp = (struct _CamelObjectBagKey *)&bag->reserved;
	res = resp->next;
	while (res) {
		if (bag->equal_key(res->key, key))
			break;
		resp = res;
		res = res->next;
	}

	g_assert(res != NULL);
	g_assert(res->owner == pthread_self());

	if (res->waiters > 0) {
		b(printf("unreserve bag, waking waiters\n"));
		res->owner = 0;
		sem_post(&res->reserve_sem);
	} else {
		b(printf("unreserve bag, no waiters, freeing reservation\n"));
		resp->next = res->next;
		bag->free_key(res->key);
		sem_destroy(&res->reserve_sem);
		g_free(res);
	}
}

/**
 * camel_object_bag_add:
 * @bag: 
 * @key: 
 * @vo: 
 * 
 * Add an object @vo to the object bag @bag.  The @key MUST have
 * previously been reserved using camel_object_bag_reserve().
 **/
void
camel_object_bag_add(CamelObjectBag *bag, const void *key, void *vo)
{
	CamelObject *o = vo;
	CamelHookList *hooks;
	CamelHookPair *pair;
	void *k;

	hooks = camel_object_get_hooks(o);
	E_LOCK(type_lock);

	pair = hooks->list;
	while (pair) {
		if (pair->name == bag_name && pair->data == bag) {
			E_UNLOCK(type_lock);
			camel_object_unget_hooks(o);
			return;
		}
		pair = pair->next;
	}

	pair = pair_alloc();
	pair->name = bag_name;
	pair->data = bag;
	pair->flags = 0;
	pair->func.event = NULL;

	pair->next = hooks->list;
	hooks->list = pair;
	hooks->list_length++;

	k = bag->copy_key(key);
	g_hash_table_insert(bag->object_table, k, vo);
	g_hash_table_insert(bag->key_table, vo, k);

	co_bag_unreserve(bag, key);
	
	E_UNLOCK(type_lock);
	camel_object_unget_hooks(o);
}

/**
 * camel_object_bag_get:
 * @bag: 
 * @key: 
 * 
 * Lookup an object by @key.  If the key is currently reserved, then
 * wait until the key has been committed before continuing.
 * 
 * Return value: NULL if the object corresponding to @key is not
 * in the bag.  Otherwise a ref'd object pointer which the caller owns
 * the ref to.
 **/
void *
camel_object_bag_get(CamelObjectBag *bag, const void *key)
{
	CamelObject *o;

	E_LOCK(type_lock);

	o = g_hash_table_lookup(bag->object_table, key);
	if (o) {
		/* we use the same lock as the refcount */
		o->ref_count++;
	} else {
		struct _CamelObjectBagKey *res = bag->reserved;

		/* check if this name is reserved currently, if so wait till its finished */
		while (res) {
			if (bag->equal_key(res->key, key))
				break;
			res = res->next;
		}

		if (res) {
			res->waiters++;
			g_assert(res->owner != pthread_self());
			E_UNLOCK(type_lock);
			sem_wait(&res->reserve_sem);
			E_LOCK(type_lock);
			res->waiters--;

			/* re-check if it slipped in */
			o = g_hash_table_lookup(bag->object_table, key);
			if (o)
				o->ref_count++;

			/* we don't actually reserve it */
			res->owner = pthread_self();
			co_bag_unreserve(bag, key);
		}
	}
	
	E_UNLOCK(type_lock);
	
	return o;
}

/**
 * camel_object_bag_peek:
 * @bag: 
 * @key: 
 * 
 * Lookup the object @key in @bag, ignoring any reservations.  If it
 * isn't committed, then it isn't considered.  This should only be
 * used where reliable transactional-based state is not required.
 * 
 * Unlike other 'peek' operations, the object is still reffed if
 * found.
 *
 * Return value: A referenced object, or NULL if @key is not
 * present in the bag.
 **/
void *
camel_object_bag_peek(CamelObjectBag *bag, const void *key)
{
	CamelObject *o;

	E_LOCK(type_lock);

	o = g_hash_table_lookup(bag->object_table, key);
	if (o) {
		/* we use the same lock as the refcount */
		o->ref_count++;
	}
	E_UNLOCK(type_lock);

	return o;
}

/**
 * camel_object_bag_reserve:
 * @bag: 
 * @key: 
 * 
 * Reserve a key in the object bag.  If the key is already reserved in
 * another thread, then wait until the reservation has been committed.
 *
 * After reserving a key, you either get a reffed pointer to the
 * object corresponding to the key, similar to object_bag_get, or you
 * get NULL, signifying that you then MIST call either object_bag_add
 * or object_bag_abort.
 *
 * You may reserve multiple keys from the same thread, but they should
 * always be reserved in the same order, to avoid deadlocks.
 * 
 * Return value: 
 **/
void *
camel_object_bag_reserve(CamelObjectBag *bag, const void *key)
{
	CamelObject *o;

	E_LOCK(type_lock);

	o = g_hash_table_lookup(bag->object_table, key);
	if (o) {
		o->ref_count++;
	} else {
		struct _CamelObjectBagKey *res = bag->reserved;

		while (res) {
			if (bag->equal_key(res->key, key))
				break;
			res = res->next;
		}

		if (res) {
			b(printf("bag reserve, already reserved, waiting\n"));
			g_assert(res->owner != pthread_self());
			res->waiters++;
			E_UNLOCK(type_lock);
			sem_wait(&res->reserve_sem);
			E_LOCK(type_lock);
			res->waiters--;
			/* incase its slipped in while we were waiting */
			o = g_hash_table_lookup(bag->object_table, key);
			if (o) {
				o->ref_count++;
				/* in which case we dont need to reserve the bag either */
				res->owner = pthread_self();
				co_bag_unreserve(bag, key);
			} else {
				res->owner = pthread_self();
			}
		} else {
			b(printf("bag reserve, no key, reserving\n"));
			res = g_malloc(sizeof(*res));
			res->waiters = 0;
			res->key = bag->copy_key(key);
			sem_init(&res->reserve_sem, 0, 0);
			res->owner = pthread_self();
			res->next = bag->reserved;
			bag->reserved = res;
		}
	}
	
	E_UNLOCK(type_lock);

	return o;
}

/**
 * camel_object_bag_abort:
 * @bag: 
 * @key: 
 * 
 * Abort a key reservation.
 **/
void
camel_object_bag_abort(CamelObjectBag *bag, const void *key)
{
	E_LOCK(type_lock);

	co_bag_unreserve(bag, key);

	E_UNLOCK(type_lock);
}

/**
 * camel_object_bag_rekey:
 * @bag: 
 * @o: 
 * @newkey: 
 * 
 * Re-key an object, atomically.  The key for object @o is set to
 * @newkey, in an atomic manner.
 *
 * It is an api (fatal) error if @o is not currently in the bag.
 **/
void
camel_object_bag_rekey(CamelObjectBag *bag, void *o, const void *newkey)
{
	void *oldkey;

	E_LOCK(type_lock);

	if (g_hash_table_lookup_extended(bag->key_table, o, NULL, &oldkey)) {
		g_hash_table_remove(bag->object_table, oldkey);
		g_hash_table_remove(bag->key_table, o);
		bag->free_key(oldkey);
		oldkey = bag->copy_key(newkey);
		g_hash_table_insert(bag->object_table, oldkey, o);
		g_hash_table_insert(bag->key_table, o, oldkey);
	} else {
		abort();
	}

	E_UNLOCK(type_lock);
}

static void
save_bag(void *key, CamelObject *o, GPtrArray *list)
{
	/* we have the refcount lock already */
	o->ref_count++;
	g_ptr_array_add(list, o);
}

/* get a list of all objects in the bag, ref'd
   ignores any reserved keys */
GPtrArray *
camel_object_bag_list(CamelObjectBag *bag)
{
	GPtrArray *list;

	list = g_ptr_array_new();

	E_LOCK(type_lock);
	g_hash_table_foreach(bag->object_table, (GHFunc)save_bag, list);
	E_UNLOCK(type_lock);

	return list;
}

/* if bag is NULL, remove all bags from object */
static void
camel_object_bag_remove_unlocked(CamelObjectBag *inbag, CamelObject *o, CamelHookList *hooks)
{
	CamelHookPair *pair, *parent;
	void *oldkey;
	CamelObjectBag *bag;

	parent = (CamelHookPair *)&hooks->list;
	pair = parent->next;
	while (pair) {
		if (pair->name == bag_name
		    && (inbag == NULL || inbag == pair->data)) {
			bag = pair->data;
			/* lookup object in table? */
			oldkey = g_hash_table_lookup(bag->key_table, o);
			if (oldkey) {
				g_hash_table_remove(bag->key_table, o);
				g_hash_table_remove(bag->object_table, oldkey);
				bag->free_key(oldkey);
			}
			parent->next = pair->next;
			pair_free(pair);
			hooks->list_length--;
		} else {
			parent = pair;
		}
		pair = parent->next;
	}
}

void
camel_object_bag_remove(CamelObjectBag *inbag, void *vo)
{
	CamelObject *o = vo;
	CamelHookList *hooks;

	if (o->hooks == NULL)
		return;

	hooks = camel_object_get_hooks(o);
	E_LOCK(type_lock);

	camel_object_bag_remove_unlocked(inbag, o, hooks);
		
	E_UNLOCK(type_lock);
	camel_object_unget_hooks(o);
}
