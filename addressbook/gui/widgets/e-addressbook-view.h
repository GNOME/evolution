/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-addressbook-view.h
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __EAB_VIEW_H__
#define __EAB_VIEW_H__

#include <gtk/gtkvbox.h>
#include <bonobo/bonobo-ui-component.h>
#include <gal/menus/gal-view-instance.h>
#include <libebook/e-book.h>
#include "e-addressbook-model.h"
#include "eab-contact-display.h"
#include "widgets/menus/gal-view-menus.h"
#include "widgets/misc/e-search-bar.h"
#include "widgets/misc/e-filter-bar.h"

G_BEGIN_DECLS

struct _EABMenu;
struct _EABMenuTargetSelect;

/* EABView - A card displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_TYPE_AB_VIEW                          (eab_view_get_type ())
#define EAB_VIEW(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_AB_VIEW, EABView))
#define EAB_VIEW_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_AB_VIEW, EABViewClass))
#define E_IS_ADDRESSBOOK_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_AB_VIEW))
#define E_IS_ADDRESSBOOK_VIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_AB_VIEW))

typedef enum {
	EAB_VIEW_NONE, /* initialized to this */
	EAB_VIEW_MINICARD,
	EAB_VIEW_TABLE,
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	,EAB_VIEW_TREEVIEW
#endif
} EABViewType;


typedef struct _EABView       EABView;
typedef struct _EABViewClass  EABViewClass;

struct _EABView
{
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
	GalViewMenus *view_menus;
	GalView *current_view;
	BonoboUIComponent *uic;

	/* the search bar and related machinery */
	ESearchBar  *search;
	gint         ecml_changed_id;
	RuleContext *search_context;
	FilterRule  *search_rule;
};

struct _EABViewClass
{
	GtkVBoxClass parent_class;

	/*
	 * Signals
	 */
	void (*status_message)        (EABView *view, const gchar *message);
	void (*search_result)         (EABView *view, EBookViewStatus status);
	void (*folder_bar_message)    (EABView *view, const gchar *message);
	void (*command_state_change)  (EABView *view);
};

GtkWidget *eab_view_new                 (void);
GType      eab_view_get_type            (void);

void       eab_view_show_contact_preview (EABView *view, gboolean show);

void       eab_view_setup_menus         (EABView  *view,
					 BonoboUIComponent *uic);
void       eab_view_discard_menus       (EABView  *view);

RuleContext *eab_view_peek_search_context (EABView *view);
FilterRule  *eab_view_peek_search_rule    (EABView *view);

void       eab_view_save_as             (EABView  *view);
void       eab_view_view                (EABView  *view);
void       eab_view_send                (EABView  *view);
void       eab_view_send_to             (EABView  *view);
void       eab_view_print               (EABView  *view);
void       eab_view_print_preview       (EABView  *view);
void       eab_view_delete_selection    (EABView  *view);
void       eab_view_cut                 (EABView  *view);
void       eab_view_copy                (EABView  *view);
void       eab_view_paste               (EABView  *view);
void       eab_view_select_all          (EABView  *view);
void       eab_view_show_all            (EABView  *view);
void       eab_view_stop                (EABView  *view);
void       eab_view_copy_to_folder      (EABView  *view);
void       eab_view_move_to_folder      (EABView  *view);

gboolean   eab_view_can_create          (EABView  *view);
gboolean   eab_view_can_print           (EABView  *view);
gboolean   eab_view_can_save_as         (EABView  *view);
gboolean   eab_view_can_view            (EABView  *view);
gboolean   eab_view_can_send            (EABView  *view);
gboolean   eab_view_can_send_to         (EABView  *view);
gboolean   eab_view_can_delete          (EABView  *view);
gboolean   eab_view_can_cut             (EABView  *view);
gboolean   eab_view_can_copy            (EABView  *view);
gboolean   eab_view_can_paste           (EABView  *view);
gboolean   eab_view_can_select_all      (EABView  *view);
gboolean   eab_view_can_stop            (EABView  *view);
gboolean   eab_view_can_copy_to_folder  (EABView  *view);
gboolean   eab_view_can_move_to_folder  (EABView  *view);

struct _EABMenuTargetSelect *eab_view_get_menu_target (EABView *view, struct _EABMenu *menu);

G_END_DECLS;

#endif /* __EAB_VIEW_H__ */
