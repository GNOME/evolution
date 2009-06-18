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

#ifndef __ES_MENU_H__
#define __ES_MENU_H__

#include <glib-object.h>

#include "e-util/e-menu.h"

G_BEGIN_DECLS

typedef struct _ESMenu ESMenu;
typedef struct _ESMenuClass ESMenuClass;

/* Current target description */
/* Types of popup tagets */
enum _es_menu_target_t {
	ES_MENU_TARGET_SHELL
};

/* Flags that describe a TARGET_SHELL */
enum {
	ES_MENU_SHELL_ONLINE = 1<<0,
	ES_MENU_SHELL_OFFLINE = 1<<1
};

typedef struct _ESMenuTargetShell ESMenuTargetShell;

struct _ESMenuTargetShell {
	EMenuTarget target;

	/* current component?? */
};

typedef struct _EMenuItem ESMenuItem;

/* The object */
struct _ESMenu {
	EMenu menu;

	struct _ESMenuPrivate *priv;
};

struct _ESMenuClass {
	EMenuClass menu_class;
};

GType es_menu_get_type(void);

ESMenu *es_menu_new(const gchar *menuid);

ESMenuTargetShell *es_menu_target_new_shell(ESMenu *emp, guint32 flags);

/* ********************************************************************** */

typedef struct _ESMenuHook ESMenuHook;
typedef struct _ESMenuHookClass ESMenuHookClass;

struct _ESMenuHook {
	EMenuHook hook;
};

struct _ESMenuHookClass {
	EMenuHookClass hook_class;
};

GType es_menu_hook_get_type(void);

G_END_DECLS

#endif /* __ES_MENU_H__ */
