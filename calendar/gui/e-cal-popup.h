/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef __E_CAL_POPUP_H__
#define __E_CAL_POPUP_H__

#include <glib-object.h>

#include "e-util/e-popup.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _ECalPopup ECalPopup;
typedef struct _ECalPopupClass ECalPopupClass;

struct _ECalendarView;

/**
 * enum _e_cal_popup_target_t - A list of mail popup target types.
 * 
 * @E_CAL_POPUP_TARGET_SELECT: A selection of cards
 * @E_CAL_POPUP_TARGET_SOURCE: A source selection.
 *
 * Defines the value of the targetid for all ECalPopup target types.
 **/
enum _e_cal_popup_target_t {
	E_CAL_POPUP_TARGET_SELECT,
	E_CAL_POPUP_TARGET_SOURCE,
};

/**
 * enum _e_cal_popup_target_select_t - ECalPopupTargetSelect qualifiers.
 * 
 * @E_CAL_POPUP_SELECT_ONE: Only one item is selected.
 * @E_CAL_POPUP_SELECT_MANY: More than one item selected.
 * @E_CAL_POPUP_SELECT_ANY: One ore more items are selected.
 * @E_CAL_POPUP_SELECT_EDITABLE: The selection is editable.
 * @E_CAL_POPUP_SELECT_RECURRING: Is a recurring event.
 * @E_CAL_POPUP_SELECT_NONRECURRING: Is not a recurring event.
 * @E_CAL_POPUP_SELECT_INSTANCE: This is an instance event.
 * @E_CAL_POPUP_SELECT_ORGANIZER: The user is the organiser of the event.
 * @E_CAL_POPUP_SELECT_NOTEDITING: The event is not being edited already.  Not implemented.
 * @E_CAL_POPUP_SELECT_NOTMEETING: The event is not a meeting.
 * @E_CAL_POPUP_SELECT_ASSIGNABLE: An assignable task.
 * @E_CAL_POPUP_SELECT_HASURL: A task that contains a URL.
 **/
enum _e_cal_popup_target_select_t {
	E_CAL_POPUP_SELECT_ONE = 1<<0,
	E_CAL_POPUP_SELECT_MANY = 1<<1,
	E_CAL_POPUP_SELECT_ANY = 1<<2,
	E_CAL_POPUP_SELECT_EDITABLE = 1<<3,
	E_CAL_POPUP_SELECT_RECURRING = 1<<4,
	E_CAL_POPUP_SELECT_NONRECURRING = 1<<5,
	E_CAL_POPUP_SELECT_INSTANCE = 1<<6,

	E_CAL_POPUP_SELECT_ORGANIZER = 1<<7,
	E_CAL_POPUP_SELECT_NOTEDITING = 1<<8,
	E_CAL_POPUP_SELECT_NOTMEETING = 1<<9,

	E_CAL_POPUP_SELECT_ASSIGNABLE = 1<<10,
	E_CAL_POPUP_SELECT_HASURL = 1<<11,
};

/**
 * enum _e_cal_popup_target_source_t - ECalPopupTargetSource qualifiers.
 * 
 * @E_CAL_POPUP_SOURCE_PRIMARY: Has a primary selection.
 * @E_CAL_POPUP_SOURCE_SYSTEM: Is a 'system' folder.
 * 
 **/
enum _e_cal_popup_target_source_t {
	E_CAL_POPUP_SOURCE_PRIMARY = 1<<0,
	E_CAL_POPUP_SOURCE_SYSTEM = 1<<1,	/* system folder */
	E_CAL_POPUP_SOURCE_USER = 1<<2, /* user folder (!system) */
};

typedef struct _ECalPopupTargetSelect ECalPopupTargetSelect;
typedef struct _ECalPopupTargetSource ECalPopupTargetSource;

/**
 * struct _ECalPopupTargetSelect - A list of address cards.
 * 
 * @target: Superclass.  target.widget is an ECalendarView.
 * @model: The ECalModel.
 * @events: The selected events.  These are ECalModelComponent's.
 *
 * Used to represent a selection of appointments as context for a popup
 * menu.
 *
 * TODO: For maximum re-usability references to the view could be removed
 * from this structure.
 **/
struct _ECalPopupTargetSelect {
	EPopupTarget target;

	struct _ECalModel *model;
	GPtrArray *events;
};

/**
 * struct _ECalPopupTargetSource - A source target.
 * 
 * @target: Superclass.
 * @selector: Selector holding the source selection.
 *
 * This target is used to represent a source selection.
 **/
struct _ECalPopupTargetSource {
	EPopupTarget target;

	struct _ESourceSelector *selector;
};

typedef struct _EPopupItem ECalPopupItem;

/* The object */
struct _ECalPopup {
	EPopup popup;

	struct _ECalPopupPrivate *priv;
};

struct _ECalPopupClass {
	EPopupClass popup_class;
};

GType e_cal_popup_get_type(void);

ECalPopup *e_cal_popup_new(const char *menuid);

ECalPopupTargetSelect *e_cal_popup_target_new_select(ECalPopup *eabp, struct _ECalModel *model, GPtrArray *events);
ECalPopupTargetSource *e_cal_popup_target_new_source(ECalPopup *eabp, struct _ESourceSelector *selector);

/* ********************************************************************** */

typedef struct _ECalPopupHook ECalPopupHook;
typedef struct _ECalPopupHookClass ECalPopupHookClass;

struct _ECalPopupHook {
	EPopupHook hook;
};

struct _ECalPopupHookClass {
	EPopupHookClass hook_class;
};

GType e_cal_popup_hook_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_CAL_POPUP_H__ */
