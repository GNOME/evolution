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

#ifndef __EM_MENU_H__
#define __EM_MENU_H__

#include <glib-object.h>

#include "e-util/e-menu.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EMMenu EMMenu;
typedef struct _EMMenuClass EMMenuClass;

/* Current target description */
/* Types of popup tagets */
enum _em_menu_target_t {
	EM_MENU_TARGET_SELECT,
};

/* Flags that describe a TARGET_SELECT */
enum {
	EM_MENU_SELECT_ONE                = 1<<1,
	EM_MENU_SELECT_MANY               = 1<<2,
	EM_MENU_SELECT_MARK_READ          = 1<<3,
	EM_MENU_SELECT_MARK_UNREAD        = 1<<4,
	EM_MENU_SELECT_DELETE             = 1<<5,
	EM_MENU_SELECT_UNDELETE           = 1<<6,
	EM_MENU_SELECT_MAILING_LIST       = 1<<7,
	EM_MENU_SELECT_EDIT               = 1<<8,
	EM_MENU_SELECT_MARK_IMPORTANT     = 1<<9,
	EM_MENU_SELECT_MARK_UNIMPORTANT   = 1<<10,
	EM_MENU_SELECT_FLAG_FOLLOWUP      = 1<<11,
	EM_MENU_SELECT_FLAG_COMPLETED     = 1<<12,
	EM_MENU_SELECT_FLAG_CLEAR         = 1<<13,
	EM_MENU_SELECT_ADD_SENDER         = 1<<14,
	EM_MENU_SELECT_MARK_JUNK          = 1<<15,
	EM_MENU_SELECT_MARK_NOJUNK        = 1<<16,
	EM_MENU_SELECT_FOLDER             = 1<<17,    /* do we have any folder at all? */
	EM_MENU_SELECT_LAST               = 1<<18     /* reserve 2 slots */
};

typedef struct _EMMenuTargetSelect EMMenuTargetSelect;

struct _EMMenuTargetSelect {
	EMenuTarget target;
	struct _CamelFolder *folder;
	char *uri;
	GPtrArray *uids;
};

typedef struct _EMenuItem EMMenuItem;

/* The object */
struct _EMMenu {
	EMenu popup;

	struct _EMMenuPrivate *priv;
};

struct _EMMenuClass {
	EMenuClass popup_class;
};

GType em_menu_get_type(void);

EMMenu *em_menu_new(const char *menuid);

EMMenuTargetSelect *em_menu_target_new_select(EMMenu *emp, struct _CamelFolder *folder, const char *folder_uri, GPtrArray *uids);

/* ********************************************************************** */

typedef struct _EMMenuHook EMMenuHook;
typedef struct _EMMenuHookClass EMMenuHookClass;

struct _EMMenuHook {
	EMenuHook hook;
};

struct _EMMenuHookClass {
	EMenuHookClass hook_class;
};

GType em_menu_hook_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_MENU_H__ */
