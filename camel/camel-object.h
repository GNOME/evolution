/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-object.h: Base class for Camel */
/*
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000-2004 Novell, Inc. (www.novell.com)
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

#include <glib.h>
#include <stdio.h>		/* FILE */
#include <stdlib.h>		/* size_t */
#include <stdarg.h>
#include <pthread.h>

#include <camel/camel-arg.h>
#include <camel/camel-types.h>	/* this is a @##$@#SF stupid header */

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
typedef struct _CamelObjectMeta CamelObjectMeta;

extern CamelType camel_interface_type;
#define CAMEL_INTERFACE_TYPE (camel_interface_type)
typedef struct _CamelInterface CamelInterface;

typedef void (*CamelObjectClassInitFunc) (CamelObjectClass *);
typedef void (*CamelObjectClassFinalizeFunc) (CamelObjectClass *);
typedef void (*CamelObjectInitFunc) (CamelObject *, CamelObjectClass *);
typedef void (*CamelObjectFinalizeFunc) (CamelObject *);

typedef gboolean (*CamelObjectEventPrepFunc) (CamelObject *, gpointer);
typedef void (*CamelObjectEventHookFunc) (CamelObject *, gpointer, gpointer);

#define CAMEL_INVALID_TYPE (NULL)

/* camel object args. */
enum {
	/* Get a description of the object. */
	CAMEL_OBJECT_ARG_DESCRIPTION = CAMEL_ARG_FIRST,	/* Get a copy of the meta-data list (should be freed) */
	CAMEL_OBJECT_ARG_METADATA,
	CAMEL_OBJECT_ARG_STATE_FILE,
	CAMEL_OBJECT_ARG_PERSISTENT_PROPERTIES,
};

enum {
	CAMEL_OBJECT_DESCRIPTION = CAMEL_OBJECT_ARG_DESCRIPTION | CAMEL_ARG_STR,
	/* Returns a CamelObjectMeta list */
	CAMEL_OBJECT_METADATA = CAMEL_OBJECT_ARG_METADATA | CAMEL_ARG_PTR,
	/* sets where the persistent data should reside, otherwise it isn't persistent */
	CAMEL_OBJECT_STATE_FILE = CAMEL_OBJECT_ARG_STATE_FILE | CAMEL_ARG_STR,
	/* returns a GSList CamelProperties of persistent properties */
	CAMEL_OBJECT_PERSISTENT_PROPERTIES = CAMEL_OBJECT_ARG_PERSISTENT_PROPERTIES | CAMEL_ARG_PTR,
};

enum _CamelObjectFlags {
	CAMEL_OBJECT_DESTROY = (1<<0),
};

/* returned by get::CAMEL_OBJECT_METADATA */
struct _CamelObjectMeta {
	struct _CamelObjectMeta *next;

	char *value;
	char name[1];		/* allocated as part of structure */
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

	/* root-class fields follow, type system above */

	/* get/set interface */
	int (*setv)(struct _CamelObject *, struct _CamelException *ex, CamelArgV *args);
	int (*getv)(struct _CamelObject *, struct _CamelException *ex, CamelArgGetV *args);
	/* we only free 1 at a time, and only pointer types, obviously */
	void (*free)(struct _CamelObject *, guint32 tag, void *ptr);

	/* get/set meta-data interface */
	char *(*meta_get)(struct _CamelObject *, const char * name);
	gboolean (*meta_set)(struct _CamelObject *, const char * name, const char *value);

	/* persistence stuff */
	int (*state_read)(struct _CamelObject *, FILE *fp);
	int (*state_write)(struct _CamelObject *, FILE *fp);
};

/* an interface is just a class with no instance data */
struct _CamelInterface {
	struct _CamelObjectClass type;
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

CamelType camel_interface_register(CamelType parent, const char *name,
				   size_t classfuncs_size,
				   CamelObjectClassInitFunc class_init,
				   CamelObjectClassFinalizeFunc class_finalize);

/* deprecated interface */
#define camel_type_get_global_classfuncs(x) ((CamelObjectClass *)(x))

/* object class methods (types == classes now) */
const char *camel_type_to_name (CamelType type);
CamelType camel_name_to_type (const char *name);
void camel_object_class_add_event (CamelObjectClass *klass, const char *name, CamelObjectEventPrepFunc prep);
void camel_object_class_add_interface(CamelObjectClass *klass, CamelType itype);

void camel_object_class_dump_tree (CamelType root);

/* casting */
CamelObject *camel_object_cast(CamelObject *obj, CamelType ctype);
gboolean camel_object_is(CamelObject *obj, CamelType ctype);

CamelObjectClass *camel_object_class_cast (CamelObjectClass *klass, CamelType ctype);
gboolean camel_object_class_is (CamelObjectClass *klass, CamelType ctype);

CamelObjectClass *camel_interface_cast(CamelObjectClass *klass, CamelType ctype);
gboolean camel_interface_is(CamelObjectClass *k, CamelType ctype);

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

/* interfaces */
void *camel_object_get_interface(void *vo, CamelType itype);

/* get/set methods */
int camel_object_set(void *obj, struct _CamelException *ex, ...);
int camel_object_setv(void *obj, struct _CamelException *ex, CamelArgV *);
int camel_object_get(void *obj, struct _CamelException *ex, ...);
int camel_object_getv(void *obj, struct _CamelException *ex, CamelArgGetV *);

/* not very efficient one-time calls */
void *camel_object_get_ptr(void *vo, CamelException *ex, int tag);
int camel_object_get_int(void *vo, CamelException *ex, int tag);

/* meta-data for user-specific data */
char *camel_object_meta_get(void *vo, const char * name);
gboolean camel_object_meta_set(void *vo, const char * name, const char *value);

/* reads/writes the state from/to the CAMEL_OBJECT_STATE_FILE */
int camel_object_state_read(void *vo);
int camel_object_state_write(void *vo);

/* free a retrieved object.  May be a noop for static data. */
void camel_object_free(void *vo, guint32 tag, void *value);

/* for managing bags of weakly-ref'd 'child' objects */
typedef struct _CamelObjectBag CamelObjectBag;
typedef void *(*CamelCopyFunc)(const void *vo);

CamelObjectBag *camel_object_bag_new(GHashFunc hash, GEqualFunc equal, CamelCopyFunc keycopy, GFreeFunc keyfree);
void *camel_object_bag_get(CamelObjectBag *bag, const void *key);
void *camel_object_bag_peek(CamelObjectBag *bag, const void *key);
void *camel_object_bag_reserve(CamelObjectBag *bag, const void *key);
void camel_object_bag_add(CamelObjectBag *bag, const void *key, void *o);
void camel_object_bag_abort(CamelObjectBag *bag, const void *key);
void camel_object_bag_rekey(CamelObjectBag *bag, void *o, const void *newkey);
GPtrArray *camel_object_bag_list(CamelObjectBag *bag);
void camel_object_bag_remove(CamelObjectBag *bag, void *o);
void camel_object_bag_destroy(CamelObjectBag *bag);

#define CAMEL_MAKE_CLASS(type, tname, parent, pname)				\
static CamelType type##_type;							\
static pname##Class * type##_parent_class;					\
										\
CamelType									\
type##_get_type(void)								\
{										\
	if (type##_type == 0) {							\
		type##_parent_class = (pname##Class *)parent##_get_type();	\
		type##_type = camel_type_register(				\
			type##_parent_class, #tname "Class",			\
			sizeof(tname),						\
			sizeof(tname ## Class),					\
			(CamelObjectClassInitFunc) type##_class_init,		\
			NULL,							\
			(CamelObjectInitFunc) type##_init,			\
			(CamelObjectFinalizeFunc) type##_finalise);		\
	}									\
										\
	return type##_type;							\
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_OBJECT_H */
