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
 *		Vivek Jain <jvivek@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_JUNK_HOOK_H__
#define __EM_JUNK_HOOK_H__

#include "e-util/e-plugin.h"

#include <camel/camel-junk-plugin.h>
#include <camel/camel-mime-message.h>

G_BEGIN_DECLS

typedef struct _EMJunkHookItem EMJunkHookItem;
typedef struct _EMJunkHookGroup EMJunkHookGroup;
typedef struct _EMJunkHook EMJunkHook;
typedef struct _EMJunkHookClass EMJunkHookClass;
typedef struct _EMJunk EMJunk;
typedef struct _EMJunkClass EMJunkClass;

typedef struct _EMJunkHookTarget EMJunkHookTarget;

typedef void (*EMJunkHookFunc)(struct _EPlugin *plugin, EMJunkHookTarget *data);

GQuark em_junk_error_quark (void);

#define EM_JUNK_ERROR em_junk_error_quark ()

struct _EMJunkHookTarget {
	CamelMimeMessage *m;
	GError *error;
};

struct _EMJunkHookItem {
	CamelJunkPlugin csp;
	struct _EMJunkHook *hook; /* parent pointer */
	gchar *check_junk;
	gchar *report_junk;
	gchar *report_non_junk;
	gchar *commit_reports;
	gchar *validate_binary;
	gchar *plugin_name;
};

struct _EMJunkHookGroup {
	struct _EMJunkHook *hook; /* parent pointer */
	gchar *id;		/* target id */
	GSList *items;		/* items to consider */
};

struct _EMJunkHook {
	EPluginHook hook;
	GSList *groups;
};

struct _EMJunkHookClass {
	EPluginHookClass hook_class;

	/* which class to add matching items to */
	GHashTable *junk_classes;
};

GType em_junk_hook_get_type(void);
void em_junk_hook_register_type(GType type);

struct _EMJunk {
	GObject object;

        CamelJunkPlugin csp;
};

struct _EMJunkClass {
	GObjectClass parent_class;
	/* which class to add matching items to */
	GHashTable *junk_classes;
};

GType emj_get_type(void);

G_END_DECLS

#endif /* __EM_JUNK_HOOK_H__ */
