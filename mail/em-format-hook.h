/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Michel Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#ifndef __EM_FORMAT_HOOK_H__
#define __EM_FORMAT_HOOK_H__

#include <glib-object.h>
#include "e-util/e-msgport.h"
#include "e-util/e-plugin.h"

#include "em-format.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EMFormatHookItem EMFormatHookItem;
typedef struct _EMFormatHookGroup EMFormatHookGroup;
typedef struct _EMFormatHook EMFormatHook;
typedef struct _EMFormatHookClass EMFormatHookClass;

typedef struct _EMFormatHookTarget EMFormatHookTarget;

typedef void (*EMFormatHookFunc)(struct _EPlugin *plugin, EMFormatHookTarget *data);

struct _EMFormatHookTarget {
	struct _EMFormat *format;
	struct _CamelStream *stream;
	struct _CamelMimePart *part;
	struct _EMFormatHookItem *item;
};

struct _EMFormatHookItem {
	EMFormatHandler handler;

	struct _EMFormatHook *hook; /* parent pointer */
	char *format;		/* format handler */
};

struct _EMFormatHookGroup {
	struct _EMFormatHook *hook; /* parent pointer */
	char *id;		/* target formatter id */
	GSList *items;		/* items to consider */
};

/**
 * struct _EMFormatHook - Mail formatter hook.
 * 
 * @hook: 
 * @groups: 
 *
 * The Mail formatter hook links all of the plugin formatter hooks
 * into the relevent formatter classes.
 **/
struct _EMFormatHook {
	EPluginHook hook;

	GSList *groups;
};

struct _EMFormatHookClass {
	EPluginHookClass hook_class;

	/* which class to add matching items to */
	GHashTable *format_classes;
};

GType em_format_hook_get_type(void);

/* register a type as a possible formatter hook point */
void em_format_hook_register_type(GType type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_FORMAT_HOOK_H__ */
