/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-select-names.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gnome.h>
#include <glade/glade.h>
#include <e-util/e-list.h>
#include <widgets/e-table/e-table.h>
#include "e-select-names-model.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* ESelectNames - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_SELECT_NAMES_TYPE			(e_select_names_get_type ())
#define E_SELECT_NAMES(obj)			(GTK_CHECK_CAST ((obj), E_SELECT_NAMES_TYPE, ESelectNames))
#define E_SELECT_NAMES_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_SELECT_NAMES_TYPE, ESelectNamesClass))
#define E_IS_SELECT_NAMES(obj)		(GTK_CHECK_TYPE ((obj), E_SELECT_NAMES_TYPE))
#define E_IS_SELECT_NAMES_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_SELECT_NAMES_TYPE))

typedef struct _ESelectNames       ESelectNames;
typedef struct _ESelectNamesClass  ESelectNamesClass;

struct _ESelectNames
{
	GnomeDialog parent;
	
	/* item specific fields */
	GladeXML *gui;
	
	GHashTable *children; /* Of type char * to ESelectNamesChild */
	int child_count;
	ETable *table;
	ETableModel *model;
	int currently_selected;
};

struct _ESelectNamesClass
{
	GnomeDialogClass parent_class;
};


GtkWidget *e_select_names_new          (void);
GtkType    e_select_names_get_type     (void);

void       e_select_names_add_section  (ESelectNames *e_select_names,
				       	char         *name,
				       	char         *id,
				       	ESelectNamesModel *source);
ESelectNamesModel *e_select_names_get_source   (ESelectNames *e_select_names,
						char *id);
/* Returns a ref counted list of addresses. */
EList     *e_select_names_get_section  (ESelectNames *e_select_names,
				       	char         *id);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_SELECT_NAMES_H__ */
