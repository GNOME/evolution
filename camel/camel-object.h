/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-object.h: Base class for Camel */

/*
 * Author:
 *  Dan Winship <danw@ximian.com>
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
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


#ifndef CAMEL_OBJECT_H
#define CAMEL_OBJECT_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <stdlib.h>		/* size_t */
#include <glib.h>
#include <stdarg.h>
#include <camel/camel-arg.h>
#include <camel/camel-types.h>	/* this is a @##$@#SF stupid header */

/* this crap shouldn't be here */
#include <camel/camel-i18n.h>

#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

/* turn on so that camel_object_class_dump_tree() dumps object instances as well */
#define CAMEL_OBJECT_TRACK_INSTANCES

typedef struct _CamelObjectClass *CamelType;

#ifdef G_DISABLE_CHECKS
#define CAMEL_CHECK_CAST(obj, ctype, ptype)         ((ptype *) obj)
#define CAMEL_CHECK_CLASS_CAST(klass, ctype, ptype) ((ptype *) klass)
#else
#define CAMEL_CHECK_CAST(obj, ctype, ptype)         ((ptype *) camel_object_cast ((CamelObject *)(obj), (CamelType)(ctype)))
#define CAMEL_CHECK_CLASS_CAST(klass, ctype, ptype) ((ptype *) camel_object_class_cast ((CamelObjectClass *)(klass), (CamelType)(ctype) ))
#endif
#define CAMEL_CHECK_TYPE(obj, ctype)                (camel_object_is ((CamelObject *)(obj), (CamelType)(ctype) ))
#define CAMEL_CHECK_CLASS_TYPE(klass, ctype)        (camel_object_class_is ((CamelObjectClass *)(klass), (CamelType)(ctype)))

extern CamelType camel_object_type;

#define CAMEL_OBJECT_TYPE        (camel_object_type)

/* we can't check casts till we've got the type, use the global type variable because its cheaper */
#define CAMEL_OBJECT(obj)        (CAMEL_CHECK_CAST((obj), camel_object_type, CamelObject))
#define CAMEL_OBJECT_CLASS(k)    (CAMEL_CHECK_CLASS_CAST ((k), camel_object_type, CamelObjectClass))
#define CAMEL_IS_OBJECT(o)       (CAMEL_CHECK_TYPE((o), camel_object_type))
#define CAMEL_IS_OBJECT_CLASS(k) (CAMEL_CHECK_CLASS_TYPE((k), camel_object_type))

#define CAMEL_OBJECT_GET_CLASS(o) ((CamelObjectClass *)(CAMEL_OBJECT(o))->klass)
#define CAMEL_OBJECT_GET_TYPE(o)  ((CamelType)(CAMEL_OBJECT(o))->klass)

typedef struct _CamelObjectClass CamelObjectClass;
typedef struct _CamelObject CamelObject;
typedef unsigned int CamelObjectHookID;

typedef void (*CamelObjectClassInitFunc) (CamelObjectClass *);
typedef void (*CamelObjectClassFinalizeFunc) (CamelObjectClass *);
typedef void (*CamelObjectInitFunc) (CamelObject *, CamelObjectClass *);
typedef void (*CamelObjectFinalizeFunc) (CamelObject *);

typedef gboolean (*CamelObjectEventPrepFunc) (CamelObject *, gpointer);
typedef void (*CamelObjectEventHookFunc) (CamelObject *, gpointer, gpointer);

#define CAMEL_INVALID_TYPE (NULL)

/* camel object args */
enum {
	CAMEL_OBJECT_ARG_DESCRIPTION = CAMEL_ARG_FIRST,
};

enum {
	CAMEL_OBJECT_DESCRIPTION = CAMEL_OBJECT_ARG_DESCRIPTION | CAMEL_ARG_STR,
};

enum _CamelObjectFlags {
	CAMEL_OBJECT_DESTROY = (1<<0),
};

/* TODO: create a simpleobject which has no events on it, or an interface for events */
struct _CamelObject {
	struct _CamelObjectClass *klass;

	guint32 magic;		/* only really needed for debugging ... */

	/* current hooks on this object */
	struct _CamelHookList *hooks;

	guint32 ref_count:24;
	guint32 flags:8;

#ifdef CAMEL_OBJECT_TRACK_INSTANCES
	struct _CamelObject *next, *prev;
#endif
};

struct _CamelObjectClass
{
	struct _CamelObjectClass *parent;

	guint32 magic;		/* in same spot for validation */

	struct _CamelObjectClass *next, *child; /* maintain heirarchy, just for kicks */

	const char *name;

	void *lock;		/* lock when used in threading, else just pads struct */

	/*unsigned short version, revision;*/

	/* if the object's bigger than 64K, it could use redesigning */
	unsigned short object_size/*, object_data*/;
	unsigned short klass_size/*, klass_data*/;

	/* available hooks for this class */
	struct _CamelHookPair *hooks;

	/* memchunks for this type */
	struct _EMemChunk *instance_chunks;
#ifdef CAMEL_OBJECT_TRACK_INSTANCES
	struct _CamelObject *instances;
#endif

	/* init class */
	void (*klass_init)(struct _CamelObjectClass *);
	void (*klass_finalise)(struct _CamelObjectClass *);

	/* init/finalise object */
	void (*init)(struct _CamelObject *, struct _CamelObjectClass *);
	void (*finalise)(struct _CamelObject *);

	/* get/set interface */
	int (*setv)(struct _CamelObject *, struct _CamelException *ex, CamelArgV *args);
	int (*getv)(struct _CamelObject *, struct _CamelException *ex, CamelArgGetV *args);
	/* we only free 1 at a time, and only pointer types, obviously */
	void (*free)(struct _CamelObject *, guint32 tag, void *ptr);
};

/* The type system .... it's pretty simple..... */
void camel_type_init (void);
CamelType camel_type_register(CamelType parent, const char * name, /*unsigned int ver, unsigned int rev,*/
			      size_t instance_size,
			      size_t classfuncs_size,
			      CamelObjectClassInitFunc class_init,
			      CamelObjectClassFinalizeFunc  class_finalize,
			      CamelObjectInitFunc instance_init,
			      CamelObjectFinalizeFunc instance_finalize);

/* deprecated interface */
#define camel_type_get_global_classfuncs(x) ((CamelObjectClass *)(x))

/* object class methods (types == classes now) */
const char *camel_type_to_name (CamelType type);
CamelType camel_name_to_type (const char *name);
void camel_object_class_add_event (CamelObjectClass *klass, const char *name, CamelObjectEventPrepFunc prep);

void camel_object_class_dump_tree (CamelType root);

/* casting */
CamelObject *camel_object_cast(CamelObject *obj, CamelType ctype);
gboolean camel_object_is(CamelObject *obj, CamelType ctype);

CamelObjectClass *camel_object_class_cast (CamelObjectClass *klass, CamelType ctype);
gboolean camel_object_class_is (CamelObjectClass *klass, CamelType ctype);

CamelType camel_object_get_type (void);

CamelObject *camel_object_new (CamelType type);
CamelObject *camel_object_new_name (const char *name);

void camel_object_ref(void *);
void camel_object_unref(void *);

#ifdef CAMEL_DEBUG
#define camel_object_ref(o) (printf("%s (%s:%d):ref (%p)\n", __FUNCTION__, __FILE__, __LINE__, o), camel_object_ref(o))
#define camel_object_unref(o) (printf("%s (%s:%d):unref (%p)\n", __FUNCTION__, __FILE__, __LINE__, o), camel_object_unref (o))
#endif

/* hooks */
CamelObjectHookID camel_object_hook_event(void *obj, const char *name, CamelObjectEventHookFunc hook, void *data);
void camel_object_remove_event(void *obj, CamelObjectHookID id);
void camel_object_unhook_event(void *obj, const char *name, CamelObjectEventHookFunc hook, void *data);
void camel_object_trigger_event(void *obj, const char *name, void *event_data);

/* get/set methods */
int camel_object_set(void *obj, struct _CamelException *ex, ...);
int camel_object_setv(void *obj, struct _CamelException *ex, CamelArgV *);
int camel_object_get(void *obj, struct _CamelException *ex, ...);
int camel_object_getv(void *obj, struct _CamelException *ex, CamelArgGetV *);

/* free a bunch of objects, list must be 0 terminated */
void camel_object_free(void *vo, guint32 tag, void *value);

/* for managing bags of weakly-ref'd 'child' objects */
typedef struct _CamelObjectBag CamelObjectBag;
typedef void *(*CamelCopyFunc)(const void *vo);

CamelObjectBag *camel_object_bag_new(GHashFunc hash, GEqualFunc equal, CamelCopyFunc keycopy, GFreeFunc keyfree);
void *camel_object_bag_get(CamelObjectBag *bag, const void *key);
void *camel_object_bag_reserve(CamelObjectBag *bag, const void *key);
void camel_object_bag_add(CamelObjectBag *bag, const void *key, void *o);
void camel_object_bag_abort(CamelObjectBag *bag, const void *key);
GPtrArray *camel_object_bag_list(CamelObjectBag *bag);
void camel_object_bag_remove(CamelObjectBag *bag, void *o);
void camel_object_bag_destroy(CamelObjectBag *bag);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_OBJECT_H */
