/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Michel Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell Inc. (www.novell.com)
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

#ifndef __E_CAL_MENU_H__
#define __E_CAL_MENU_H__

#include <glib-object.h>

#include "e-util/e-menu.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _ECalMenu ECalMenu;
typedef struct _ECalMenuClass ECalMenuClass;

/* Current target description */
/* Types of popup tagets */
enum _e_cal_menu_target_t {
	E_CAL_MENU_TARGET_SELECT,
};

/**
 * enum _e_cal_menu_target_select_t - ECalPopupTargetSelect qualifiers.
 * 
 * @E_CAL_MENU_SELECT_ONE: Only one item is selected.
 * @E_CAL_MENU_SELECT_MANY: More than one item selected.
 * @E_CAL_MENU_SELECT_ANY: One or more items selected.
 * @E_CAL_MENU_SELECT_EDITABLE: The selection is editable.
 * @E_CAL_MENU_SELECT_RECURRING: Is a recurring event.
 * @E_CAL_MENU_SELECT_NONRECURRING: Is not a recurring event.
 * @E_CAL_MENU_SELECT_INSTANCE: This is an instance event.
 * @E_CAL_MENU_SELECT_ORGANIZER: The user is the organiser of the event.
 * @E_CAL_MENU_SELECT_NOTEDITING: The event is not being edited already.  Not implemented.
 * @E_CAL_MENU_SELECT_NOTMEETING: The event is not a meeting.
 * @E_CAL_MENU_SELECT_ASSIGNABLE: An assignable task.
 * @E_CAL_MENU_SELECT_HASURL: A task that contains a URL.
 **/
enum _e_cal_menu_target_select_t {
	E_CAL_MENU_SELECT_ONE = 1<<0,
	E_CAL_MENU_SELECT_MANY = 1<<1,
	E_CAL_MENU_SELECT_ANY = 1<<2,
	E_CAL_MENU_SELECT_EDITABLE = 1<<3,
	E_CAL_MENU_SELECT_RECURRING = 1<<4,
	E_CAL_MENU_SELECT_NONRECURRING = 1<<5,
	E_CAL_MENU_SELECT_INSTANCE = 1<<6,

	E_CAL_MENU_SELECT_ORGANIZER = 1<<7,
	E_CAL_MENU_SELECT_NOTEDITING = 1<<8,
	E_CAL_MENU_SELECT_NOTMEETING = 1<<9,

	E_CAL_MENU_SELECT_ASSIGNABLE = 1<<10,
	E_CAL_MENU_SELECT_HASURL = 1<<11,
};

typedef struct _ECalMenuTargetSelect ECalMenuTargetSelect;

struct _ECalMenuTargetSelect {
	EMenuTarget target;

	struct _ECalModel *model;
	GPtrArray *events;
};

typedef struct _EMenuItem ECalMenuItem;

/* The object */
struct _ECalMenu {
	EMenu menu;

	struct _ECalMenuPrivate *priv;
};

struct _ECalMenuClass {
	EMenuClass menu_class;
};

GType e_cal_menu_get_type(void);

ECalMenu *e_cal_menu_new(const char *menuid);

ECalMenuTargetSelect *e_cal_menu_target_new_select(ECalMenu *emp, struct _ECalModel *model, GPtrArray *events);

/* ********************************************************************** */

typedef struct _ECalMenuHook ECalMenuHook;
typedef struct _ECalMenuHookClass ECalMenuHookClass;

struct _ECalMenuHook {
	EMenuHook hook;
};

struct _ECalMenuHookClass {
	EMenuHookClass hook_class;
};

GType e_cal_menu_hook_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_CAL_MENU_H__ */
