/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-select-names.h
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
#ifndef __E_SELECT_NAMES_H__
#define __E_SELECT_NAMES_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>
#include <glade/glade.h>
#include <gal/e-table/e-table.h>
#include <gal/e-table/e-table-scrolled.h>
#include <libedataserver/e-source-list.h>
#include <libedataserverui/e-source-option-menu.h>

#include "e-addressbook-model.h"

#include "e-select-names-model.h"

G_BEGIN_DECLS

/* ESelectNames - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_TYPE_SELECT_NAMES		(e_select_names_get_type ())
#define E_SELECT_NAMES(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SELECT_NAMES, ESelectNames))
#define E_SELECT_NAMES_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES, ESelectNamesClass))
#define E_IS_SELECT_NAMES(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SELECT_NAMES))
#define E_IS_SELECT_NAMES_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SELECT_NAMES))

typedef struct _ESelectNames       ESelectNames;
typedef struct _ESelectNamesClass  ESelectNamesClass;
typedef struct _ESelectNamesFolder ESelectNamesFolder;

struct _ESelectNames
{
	GtkDialog parent;
	
	ESourceList *source_list;
	
	/* item specific fields */
	GladeXML *gui;
	
	GHashTable *children; /* Of type char * to ESelectNamesChild */
	int child_count;
	ETableScrolled *table;
	ETableModel *adapter;
	ETableModel *without;
	EABModel *model;
	GtkWidget *categories;
	GtkWidget *select_entry;
	GtkWidget *status_message;
	char *def;
	ESelectNamesFolder *current_folder;

	/* signal handlers */
	gulong status_id;
	gulong search_id;
};

struct _ESelectNamesClass
{
	GtkDialogClass parent_class;
};


GType      e_select_names_get_type     (void);

GtkWidget *e_select_names_new          (void);

void       e_select_names_add_section  (ESelectNames         *e_select_names,
				       	const char           *name,
				       	const char           *id,
				       	ESelectNamesModel    *source);
void       e_select_names_set_default  (ESelectNames         *e_select_names,
					const char           *id);

G_END_DECLS

#endif /* __E_SELECT_NAMES_H__ */
