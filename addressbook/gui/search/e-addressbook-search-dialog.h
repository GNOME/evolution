/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard-view-widget.h
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

#ifndef __EAB_SEARCH_DIALOG_H__
#define __EAB_SEARCH_DIALOG_H__

#include <libebook/e-book.h>

#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "filter/rule-context.h"
#include "filter/filter-rule.h"
#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define EAB_SEARCH_DIALOG_TYPE		(eab_search_dialog_get_type ())
#define EAB_SEARCH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EAB_SEARCH_DIALOG_TYPE, EABSearchDialog))
#define EAB_SEARCH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EAB_SEARCH_DIALOG_TYPE, EABSearchDialogClass))
#define E_IS_ADDRESSBOOK_SEARCH_DIALOG(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EAB_SEARCH_DIALOG_TYPE))
#define E_IS_ADDRESSBOOK_SEARCH_DIALOG_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((obj), EAB_SEARCH_DIALOG_TYPE))


typedef struct _EABSearchDialog       EABSearchDialog;
typedef struct _EABSearchDialogClass  EABSearchDialogClass;

struct _EABSearchDialog
{
	GtkDialog parent;

	GtkWidget *search;
	EABView *view;
};

struct _EABSearchDialogClass
{
	GtkDialogClass parent_class;
};

GType      eab_search_dialog_get_type (void);

GtkWidget *eab_search_dialog_new (EABView *view);

G_END_DECLS

#endif /* __EAB_SEARCH_DIALOG_H__ */
