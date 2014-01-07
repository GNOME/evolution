/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_ADDRESSBOOK_VIEW_H
#define E_ADDRESSBOOK_VIEW_H

#include <libebook/libebook.h>

#include <shell/e-shell-view.h>

#include "e-addressbook-model.h"
#include "eab-contact-display.h"

/* Standard GObject macros */
#define E_TYPE_ADDRESSBOOK_VIEW \
	(e_addressbook_view_get_type ())
#define E_ADDRESSBOOK_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ADDRESSBOOK_VIEW, EAddressbookView))
#define E_ADDRESSBOOK_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ADDRESSBOOK_VIEW, EAddressbookViewClass))
#define E_IS_ADDRESSBOOK_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ADDRESSBOOK_VIEW))
#define E_IS_ADDRESSBOOK_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_ADDRESSBOOK_VIEW))
#define E_ADDRESSBOOK_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ADDRESSBOOK_VIEW, EAddressbookViewClass))

G_BEGIN_DECLS

typedef struct _EAddressbookView EAddressbookView;
typedef struct _EAddressbookViewClass EAddressbookViewClass;
typedef struct _EAddressbookViewPrivate EAddressbookViewPrivate;

struct _EAddressbookView {
	GtkScrolledWindow parent;
	EAddressbookViewPrivate *priv;
};

struct _EAddressbookViewClass {
	GtkScrolledWindowClass parent_class;

	/* Signals */
	void	(*open_contact)			(EAddressbookView *view,
						 EContact *contact,
						 gboolean is_new_contact);
	void	(*popup_event)			(EAddressbookView *view,
						 GdkEvent *event);
	void	(*command_state_change)		(EAddressbookView *view);
	void	(*selection_change)		(EAddressbookView *view);
};

GType		e_addressbook_view_get_type	(void);
GtkWidget *	e_addressbook_view_new		(EShellView *shell_view,
						 ESource *source);
EAddressbookModel *
		e_addressbook_view_get_model	(EAddressbookView *view);
GalViewInstance *
		e_addressbook_view_get_view_instance
						(EAddressbookView *view);
GObject *	e_addressbook_view_get_view_object
						(EAddressbookView *view);
GSList *	e_addressbook_view_get_selected	(EAddressbookView *view);
ESelectionModel *
		e_addressbook_view_get_selection_model
						(EAddressbookView *view);
EShellView *	e_addressbook_view_get_shell_view
						(EAddressbookView *view);
ESource *	e_addressbook_view_get_source	(EAddressbookView *view);
GtkTargetList *	e_addressbook_view_get_copy_target_list
						(EAddressbookView *view);
GtkTargetList *	e_addressbook_view_get_paste_target_list
						(EAddressbookView *view);
void		e_addressbook_view_view		(EAddressbookView *view);
void		e_addressbook_view_print	(EAddressbookView *view,
						 gboolean selection_only,
						 GtkPrintOperationAction action);
void		e_addressbook_view_delete_selection
						(EAddressbookView *view,
						 gboolean is_delete);
void		e_addressbook_view_show_all	(EAddressbookView *view);
void		e_addressbook_view_stop		(EAddressbookView *view);
void		e_addressbook_view_copy_to_folder
						(EAddressbookView *view,
						 gboolean all);
void		e_addressbook_view_move_to_folder
						(EAddressbookView *view,
						 gboolean all);

gboolean	e_addressbook_view_can_create	(EAddressbookView *view);

void		e_addressbook_view_set_search	(EAddressbookView *view,
						 gint filter_id,
						 gint search_id,
						 const gchar *search_text,
						 EFilterRule *advanced_search);

void		e_addressbook_view_get_search	(EAddressbookView *view,
						 gint *filter_id,
						 gint *search_id,
						 gchar **search_text,
						 EFilterRule **advanced_search);

G_END_DECLS

#endif /* E_ADDRESSBOOK_VIEW_H */
