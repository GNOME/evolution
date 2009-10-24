/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_FORMAT_HOOK_H__
#define __EM_FORMAT_HOOK_H__

#include <e-util/e-plugin.h>
#include <em-format/em-format.h>

G_BEGIN_DECLS

typedef struct _EMFormatHookItem EMFormatHookItem;
typedef struct _EMFormatHookGroup EMFormatHookGroup;
typedef struct _EMFormatHook EMFormatHook;
typedef struct _EMFormatHookClass EMFormatHookClass;

typedef struct _EMFormatHookTarget EMFormatHookTarget;

typedef void (*EMFormatHookFunc)(struct _EPlugin *plugin, EMFormatHookTarget *data);

struct _EMFormatHookTarget {
	struct _EMFormat *format;
	CamelStream *stream;
	CamelMimePart *part;
	struct _EMFormatHookItem *item;
};

struct _EMFormatHookItem {
	EMFormatHandler handler;

	struct _EMFormatHook *hook; /* parent pointer */
	gchar *format;		/* format handler */
};

struct _EMFormatHookGroup {
	struct _EMFormatHook *hook; /* parent pointer */
	gchar *id;		/* target formatter id */
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

G_END_DECLS

#endif /* __EM_FORMAT_HOOK_H__ */
