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
#ifndef __E_ADDRESSBOOK_SEARCH_DIALOG_H__
#define __E_ADDRESSBOOK_SEARCH_DIALOG_H__

#include <ebook/e-book.h>

#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "filter/rule-context.h"
#include "filter/filter-rule.h"
#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define E_ADDRESSBOOK_SEARCH_DIALOG_TYPE		(e_addressbook_search_dialog_get_type ())
#define E_ADDRESSBOOK_SEARCH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_ADDRESSBOOK_SEARCH_DIALOG_TYPE, EAddressbookSearchDialog))
#define E_ADDRESSBOOK_SEARCH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_ADDRESSBOOK_SEARCH_DIALOG_TYPE, EAddressbookSearchDialogClass))
#define E_IS_ADDRESSBOOK_SEARCH_DIALOG(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_ADDRESSBOOK_SEARCH_DIALOG_TYPE))
#define E_IS_ADDRESSBOOK_SEARCH_DIALOG_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_ADDRESSBOOK_SEARCH_DIALOG_TYPE))


typedef struct _EAddressbookSearchDialog       EAddressbookSearchDialog;
typedef struct _EAddressbookSearchDialogClass  EAddressbookSearchDialogClass;

struct _EAddressbookSearchDialog
{
	GtkDialog parent;

	GtkWidget *search;

	EAddressbookView *view;

	RuleContext *context;
	FilterRule *rule;
};

struct _EAddressbookSearchDialogClass
{
	GtkDialogClass parent_class;
};

GType      e_addressbook_search_dialog_get_type (void);

GtkWidget *e_addressbook_search_dialog_new (EAddressbookView *view);

G_END_DECLS

#endif /* __E_ADDRESSBOOK_SEARCH_DIALOG_H__ */
