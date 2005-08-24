/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Vivek Jain <jvivek@novell.com>
 *
 *  Copyright 2005 Novell, Inc. (www.novell.com)
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

#ifndef __EM_JUNK_HOOK_H__
#define __EM_JUNK_HOOK_H__

#include "e-util/e-plugin.h"

#include <camel/camel-junk-plugin.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EMJunkHookItem EMJunkHookItem;
typedef struct _EMJunkHookGroup EMJunkHookGroup;
typedef struct _EMJunkHook EMJunkHook;
typedef struct _EMJunkHookClass EMJunkHookClass;
typedef struct _EMJunk EMJunk;
typedef struct _EMJunkClass EMJunkClass;

typedef struct _EMJunkHookTarget EMJunkHookTarget;

typedef void (*EMJunkHookFunc)(struct _EPlugin *plugin, EMJunkHookTarget *data);

struct _EMJunkHookTarget {
	struct _CamelMimeMessage *m;
};

struct _EMJunkHookItem {
	CamelJunkPlugin csp;
	struct _EMJunkHook *hook; /* parent pointer */
	char *check_junk;
	char *report_junk;
	char *report_non_junk;
	char *commit_reports;
};

struct _EMJunkHookGroup {
	struct _EMJunkHook *hook; /* parent pointer */
	char *id;		/* target id */
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_JUNK_HOOK_H__ */
