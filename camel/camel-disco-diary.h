/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * camel-disco-diary.h: class for logging disconnected operation
 *
 * Authors: Dan Winship <danw@ximian.com>
 *
 * Copyright 2001 Ximian, Inc.
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

#ifndef CAMEL_DISCO_DIARY_H
#define CAMEL_DISCO_DIARY_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-object.h"
#include <stdarg.h>
#include <stdio.h>

#define CAMEL_DISCO_DIARY_TYPE     (camel_disco_diary_get_type ())
#define CAMEL_DISCO_DIARY(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_DISCO_DIARY_TYPE, CamelDiscoDiary))
#define CAMEL_DISCO_DIARY_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_DISCO_DIARY_TYPE, CamelDiscoDiaryClass))
#define CAMEL_IS_DISCO_DIARY(o)    (CAMEL_CHECK_TYPE((o), CAMEL_DISCO_DIARY_TYPE))

typedef enum {
	CAMEL_DISCO_DIARY_END = 0,

	CAMEL_DISCO_DIARY_FOLDER_EXPUNGE,
	CAMEL_DISCO_DIARY_FOLDER_APPEND,
	CAMEL_DISCO_DIARY_FOLDER_TRANSFER,
} CamelDiscoDiaryAction;

typedef enum {
	CAMEL_DISCO_DIARY_ARG_NONE = 0,

	CAMEL_DISCO_DIARY_ARG_FOLDER,
	CAMEL_DISCO_DIARY_ARG_UID,
	CAMEL_DISCO_DIARY_ARG_UID_LIST
} CamelDiscoDiaryArgType;

struct _CamelDiscoDiary {
	CamelObject parent_object;

	CamelDiscoStore *store;
	FILE *file;
	GHashTable *folders, *uidmap;
};

typedef struct {
	CamelObjectClass parent_class;

} CamelDiscoDiaryClass;


/* public methods */
CamelDiscoDiary *camel_disco_diary_new    (CamelDiscoStore *store,
					   const char *filename,
					   CamelException *ex);

gboolean         camel_disco_diary_empty  (CamelDiscoDiary *diary);

void             camel_disco_diary_log    (CamelDiscoDiary *diary,
					   CamelDiscoDiaryAction action,
					   ...);
void             camel_disco_diary_replay (CamelDiscoDiary *diary,
					   CamelException *ex);

/* Temporary->Permanent UID map stuff */
void        camel_disco_diary_uidmap_add    (CamelDiscoDiary *diary,
					     const char *old_uid,
					     const char *new_uid);
const char *camel_disco_diary_uidmap_lookup (CamelDiscoDiary *diary,
					     const char *uid);

/* Standard Camel function */
CamelType camel_disco_diary_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_DISCO_DIARY_H */
