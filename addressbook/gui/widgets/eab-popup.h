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

#ifndef __EAB_POPUP_H__
#define __EAB_POPUP_H__

#include <glib-object.h>

#include "e-util/e-popup.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EABPopup EABPopup;
typedef struct _EABPopupClass EABPopupClass;

/**
 * enum _eab_popup_target_t - A list of mail popup target types.
 * 
 * @EAB_POPUP_TARGET_SELECT: A selection of cards
 * @EAB_POPUP_TARGET_SOURCE: A source selection.
 *
 * Defines the value of the targetid for all EABPopup target types.
 **/
enum _eab_popup_target_t {
	EAB_POPUP_TARGET_SELECT,
	EAB_POPUP_TARGET_SOURCE,
	EAB_POPUP_TARGET_SELECT_NAMES,
};

/**
 * enum _eab_popup_target_select_t - EABPopupTargetSelect qualifiers.
 * 
 * @EAB_POPUP_SELECT_ONE: Only one item is selected.
 * @EAB_POPUP_SELECT_MANY: Two or more items are selected.
 * @EAB_POPUP_SELECT_ANY: One or more items are selected.
 * @EAB_POPUP_SELECT_EDITABLE: Read/writable source.
 * @EAB_POPUP_SELECT_EMAIL: Has an email address.
 **/
enum _eab_popup_target_select_t {
	EAB_POPUP_SELECT_ONE = 1<<0,
	EAB_POPUP_SELECT_MANY = 1<<1,
	EAB_POPUP_SELECT_ANY = 1<<2,
	EAB_POPUP_SELECT_EDITABLE = 1<<3,
	EAB_POPUP_SELECT_EMAIL = 1<<4,
};

/**
 * enum _eab_popup_target_source_t - EABPopupTargetSource qualifiers.
 * 
 * @EAB_POPUP_SOURCE_PRIMARY: Has a primary selection.
 * @EAB_POPUP_SOURCE_SYSTEM: Is a 'system' folder.
 * 
 **/
enum _eab_popup_target_source_t {
	EAB_POPUP_SOURCE_PRIMARY = 1<<0,
	EAB_POPUP_SOURCE_SYSTEM = 1<<1,	/* system folder */
	EAB_POPUP_SOURCE_USER = 1<<2, /* user folder (!system) */
};

typedef struct _EABPopupTargetSelect EABPopupTargetSelect;
typedef struct _EABPopupTargetSource EABPopupTargetSource;
typedef struct _EABPopupTargetSelectNames EABPopupTargetSelectNames;

/**
 * struct _EABPopupTargetSelect - A list of address cards.
 * 
 * @target: Superclass.
 * @book: Book the cards belong to.
 * @cards: All selected cards.
 *
 * Used to represent a selection of cards as context for a popup
 * menu.
 **/
struct _EABPopupTargetSelect {
	EPopupTarget target;

	struct _EBook *book;
	GPtrArray *cards;
};

/**
 * struct _EABPopupTargetSource - A source target.
 * 
 * @target: Superclass.
 * @selector: Selector holding the source selection.
 *
 * This target is used to represent a source selection.
 **/
struct _EABPopupTargetSource {
	EPopupTarget target;

	struct _ESourceSelector *selector;
};

/**
 * struct _EABPopupTargetSelectNames - A select names target.
 * 
 * @target: Superclass.
 * @model: Select names model.
 * @row: Row of item selected.
 *
 * This target is used to represent an item selected in an
 * ESelectNames model.
 **/
struct _EABPopupTargetSelectNames {
	EPopupTarget target;

	struct _ESelectNamesModel *model;
	int row;
};

typedef struct _EPopupItem EABPopupItem;

/* The object */
struct _EABPopup {
	EPopup popup;

	struct _EABPopupPrivate *priv;
};

struct _EABPopupClass {
	EPopupClass popup_class;
};

GType eab_popup_get_type(void);

EABPopup *eab_popup_new(const char *menuid);

EABPopupTargetSelect *eab_popup_target_new_select(EABPopup *eabp, struct _EBook *book, int readonly, GPtrArray *cards);
EABPopupTargetSource *eab_popup_target_new_source(EABPopup *eabp, struct _ESourceSelector *selector);
EABPopupTargetSelectNames *eab_popup_target_new_select_names(EABPopup *eabp, struct _ESelectNamesModel *model, int row);

/* ********************************************************************** */

typedef struct _EABPopupHook EABPopupHook;
typedef struct _EABPopupHookClass EABPopupHookClass;

struct _EABPopupHook {
	EPopupHook hook;
};

struct _EABPopupHookClass {
	EPopupHookClass hook_class;
};

GType eab_popup_hook_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EAB_POPUP_H__ */
