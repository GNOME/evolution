/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-object.h: Base class for Camel */

/*
 * Author:
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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

#ifndef CAMEL_OBJECT_H
#define CAMEL_OBJECT_H 1

#ifdef __cplusplus
extern "C"
{
#pragma }
#endif				/* __cplusplus } */

#include <stdlib.h>		/* size_t */
#include <camel/camel-types.h>
#include <glib.h>

#ifdef G_DISABLE_CHECKS
#define CAMEL_CHECK_CAST( obj, ctype, ptype )         ((ptype *) obj)
#define CAMEL_CHECK_CLASS_CAST( class, ctype, ptype ) ((ptype *) class)
#define CAMEL_CHECK_TYPE( obj, ctype )                (TRUE)
#define CAMEL_CHECK_CLASS_TYPE( class, ctype )        (TRUE)
#else
#define CAMEL_CHECK_CAST( obj, ctype, ptype )         ((ptype *) camel_object_check_cast( (CamelObject *)(obj), (CamelType)(ctype) ))
#define CAMEL_CHECK_CLASS_CAST( class, ctype, ptype ) ((ptype *) camel_object_class_check_cast( (CamelObjectClass *)(class), (CamelType)(ctype) ))
#define CAMEL_CHECK_TYPE( obj, ctype )                (camel_object_is_of_type( (CamelObject *)(obj), (CamelType)(ctype) ))
#define CAMEL_CHECK_CLASS_TYPE( class, ctype )        (camel_object_class_is_of_type( (CamelObjectClass *)(class), (CamelType)(ctype) ))
#endif

#define CAMEL_INVALID_TYPE ((CamelType)0)

#define CAMEL_OBJECT_TYPE        (camel_object_get_type ())

#define CAMEL_OBJECT(obj)        (CAMEL_CHECK_CAST((obj), CAMEL_OBJECT_TYPE, CamelObject))
#define CAMEL_OBJECT_CLASS(k)    (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_OBJECT_TYPE, CamelObjectClass))
#define CAMEL_IS_OBJECT(o)       (CAMEL_CHECK_TYPE((o), CAMEL_OBJECT_TYPE))
#define CAMEL_IS_OBJECT_CLASS(k) (CAMEL_CHECK_CLASS_TYPE((k), CAMEL_OBJECT_TYPE))

#define CAMEL_OBJECT_GET_CLASS(o) ((CamelObjectClass *)(CAMEL_OBJECT(o))->classfuncs)
#define CAMEL_OBJECT_GET_TYPE(o)  ((CamelType)(CAMEL_OBJECT(o))->s.type)

	typedef guint32 CamelType;

	typedef struct _CamelObjectShared
	{
		guint32 magic;
		CamelType type;
	}
	CamelObjectShared;

	typedef struct _CamelObjectClass
	{
		CamelObjectShared s;

		GHashTable *event_to_preplist;
	}
	CamelObjectClass;

	typedef struct _CamelObject
	{
		CamelObjectShared s;
		guint32 ref_count:31;
		guint32 in_event:1;
		CamelObjectClass *classfuncs;
		GHashTable *event_to_hooklist;
	}
	CamelObject;

	typedef void (*CamelObjectClassInitFunc) (CamelObjectClass *);
	typedef void (*CamelObjectClassFinalizeFunc) (CamelObjectClass *);
	typedef void (*CamelObjectInitFunc) (CamelObject *);
	typedef void (*CamelObjectFinalizeFunc) (CamelObject *);

	typedef gboolean (*CamelObjectEventPrepFunc) (CamelObject *,
						      gpointer);
	typedef void (*CamelObjectEventHookFunc) (CamelObject *, gpointer,
						  gpointer);

/* The type system .... it's pretty simple..... */

	void camel_type_init (void);
	CamelType camel_type_register (CamelType parent, const gchar * name,
				       size_t instance_size,
				       size_t classfuncs_size,
				       CamelObjectClassInitFunc class_init,
				       CamelObjectClassFinalizeFunc
				       class_finalize,
				       CamelObjectInitFunc instance_init,
				       CamelObjectFinalizeFunc
				       instance_finalize);
	CamelObjectClass *camel_type_get_global_classfuncs (CamelType type);
	const gchar *camel_type_to_name (CamelType type);

	CamelType camel_object_get_type (void);
	CamelObject *camel_object_new (CamelType type);
	void camel_object_ref (CamelObject * obj);
	void camel_object_unref (CamelObject * obj);
	CamelObject *camel_object_check_cast (CamelObject * obj,
					      CamelType ctype);
	CamelObjectClass *camel_object_class_check_cast (CamelObjectClass *
							 class,
							 CamelType ctype);
	gboolean camel_object_is_of_type (CamelObject * obj, CamelType ctype);
	gboolean camel_object_class_is_of_type (CamelObjectClass * class,
						CamelType ctype);
	gchar *camel_object_describe (CamelObject * obj);
	void camel_object_class_declare_event (CamelObjectClass * class,
					       const gchar * name,
					       CamelObjectEventPrepFunc prep);
	void camel_object_hook_event (CamelObject * obj, const gchar * name,
				      CamelObjectEventHookFunc hook,
				      gpointer user_data);
	void camel_object_unhook_event (CamelObject * obj, const gchar * name,
					CamelObjectEventHookFunc hook,
					gpointer user_data);
	void camel_object_trigger_event (CamelObject * obj,
					 const gchar * name,
					 gpointer event_data);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* CAMEL_OBJECT_H */
