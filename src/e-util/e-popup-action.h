/*
 * e-popup-action.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* A popup action is an action that lives in a popup menu.  It proxies an
 * equivalent action in the main menu, with two differences:
 *
 * 1) If the main menu action is insensitive, the popup action is invisible.
 * 2) The popup action may have a different label than the main menu action.
 *
 * To use:
 *
 * Create an array of EPopupActionEntry structs.  Add the main menu actions
 * that serve as related actions for the popup actions to an action group
 * first.  Then pass the same action group and the EPopupActionEntry array
 * to e_action_group_add_popup_actions() to add popup actions.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_POPUP_ACTION_H
#define E_POPUP_ACTION_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_POPUP_ACTION \
	(e_popup_action_get_type ())
#define E_POPUP_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_POPUP_ACTION, EPopupAction))
#define E_POPUP_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_POPUP_ACTION, EPopupActionClass))
#define E_IS_POPUP_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_POPUP_ACTION))
#define E_IS_POPUP_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_POPUP_ACTION))
#define E_POPUP_ACTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_POPUP_ACTION, EPopupActionClass))

G_BEGIN_DECLS

typedef struct _EPopupAction EPopupAction;
typedef struct _EPopupActionClass EPopupActionClass;
typedef struct _EPopupActionPrivate EPopupActionPrivate;
typedef struct _EPopupActionEntry EPopupActionEntry;

struct _EPopupAction {
	GtkAction parent;
	EPopupActionPrivate *priv;
};

struct _EPopupActionClass {
	GtkActionClass parent_class;
};

struct _EPopupActionEntry {
	const gchar *name;
	const gchar *label;	/* optional: overrides the related action */
	const gchar *related;	/* name of the related action */
};

GType		e_popup_action_get_type		(void) G_GNUC_CONST;
EPopupAction *	e_popup_action_new		(const gchar *name);

void		e_action_group_add_popup_actions
						(GtkActionGroup *action_group,
						 const EPopupActionEntry *entries,
						 guint n_entries);

G_END_DECLS

#endif /* E_POPUP_ACTION_H */
