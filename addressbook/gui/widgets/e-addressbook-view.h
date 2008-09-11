/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */
#ifndef EAB_VIEW_H
#define EAB_VIEW_H

#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-component.h>
#include <widgets/menus/gal-view-instance.h>
#include <libebook/e-book.h>
#include "e-addressbook-model.h"
#include "eab-contact-display.h"
#include "misc/e-search-bar.h"

/* Standard GObject macros */
#define E_TYPE_AB_VIEW \
	(eab_view_get_type ())
#define EAB_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_AB_VIEW, EABView))
#define EAB_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_AB_VIEW, EABViewClass))
#define E_IS_ADDRESSBOOK_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_AB_VIEW))
#define E_IS_ADDRESSBOOK_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_AB_VIEW))
#define E_ADDRESSBOOK_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_AB_VIEW, EABViewClass))

G_BEGIN_DECLS

struct _EABMenu;
struct _EABMenuTargetSelect;

typedef enum {
	EAB_VIEW_NONE, /* initialized to this */
	EAB_VIEW_MINICARD,
	EAB_VIEW_TABLE,
} EABViewType;


typedef struct _EABView EABView;
typedef struct _EABViewClass EABViewClass;

struct _EABView {
	GtkVBox parent;

	/* item specific fields */
	EABViewType view_type;

	EABModel   *model;

	GtkWidget *invisible;
	GList *clipboard_contacts;

	EBook *book;
	ESource *source;
	char  *query;
	guint editable : 1;

	gint displayed_contact;

	GObject *object;
	GtkWidget *widget;

	GtkWidget *contact_display_window;
	GtkWidget *contact_display;
	GtkWidget *paned;

	/* Menus handler and the view instance */
	GalViewInstance *view_instance;
	/*GalViewMenus *view_menus;*/
	GalView *current_view;
};

struct _EABViewClass {
	GtkVBoxClass parent_class;

	/* Signals */
	void (*status_message)        (EABView *view, const gchar *message);
	void (*search_result)         (EABView *view, EBookViewStatus status);
	void (*folder_bar_message)    (EABView *view, const gchar *message);
	void (*command_state_change)  (EABView *view);
};

GType		eab_view_get_type		(void);
GtkWidget *	eab_view_new			(void);
void		eab_view_show_contact_preview	(EABView *view,
						 gboolean show);
void		eab_view_save_as		(EABView *view,
						 gboolean all);
void		eab_view_view			(EABView *view);
void		eab_view_send			(EABView *view);
void		eab_view_send_to		(EABView *view);
void		eab_view_print			(EABView *view,
						 GtkPrintOperationAction action);
void		eab_view_delete_selection	(EABView *view,
						 gboolean is_delete);
void		eab_view_cut			(EABView *view);
void		eab_view_copy			(EABView *view);
void		eab_view_paste			(EABView *view);
void		eab_view_select_all		(EABView *view);
void		eab_view_show_all		(EABView *view);
void		eab_view_stop			(EABView *view);
void		eab_view_copy_to_folder		(EABView *view,
						 gboolean all);
void		eab_view_move_to_folder		(EABView *view,
						 gboolean all);

gboolean	eab_view_can_create		(EABView *view);
gboolean	eab_view_can_print		(EABView *view);
gboolean	eab_view_can_save_as		(EABView *view);
gboolean	eab_view_can_view		(EABView *view);
gboolean	eab_view_can_send		(EABView *view);
gboolean	eab_view_can_send_to		(EABView *view);
gboolean	eab_view_can_delete		(EABView *view);
gboolean	eab_view_can_cut		(EABView *view);
gboolean	eab_view_can_copy		(EABView *view);
gboolean	eab_view_can_paste		(EABView *view);
gboolean	eab_view_can_select_all		(EABView *view);
gboolean	eab_view_can_stop		(EABView *view);
gboolean	eab_view_can_copy_to_folder	(EABView *view);
gboolean	eab_view_can_move_to_folder	(EABView *view);

struct _EABMenuTargetSelect *
		eab_view_get_menu_target	(EABView *view,
						 struct _EABMenu *menu);

G_END_DECLS

#endif /* EAB_VIEW_H */
