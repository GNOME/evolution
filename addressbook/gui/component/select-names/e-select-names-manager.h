/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 */

#ifndef __E_SELECT_NAMES_MANAGER_H__
#define __E_SELECT_NAMES_MANAGER_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <e-util/e-list.h>

#define E_TYPE_SELECT_NAMES_MANAGER            (e_select_names_manager_get_type ())
#define E_SELECT_NAMES_MANAGER(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_SELECT_NAMES_MANAGER, ESelectNamesManager))
#define E_SELECT_NAMES_MANAGER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_MANAGER, ESelectNamesManagerClass))
#define E_IS_SELECT_NAMES_MANAGER(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_SELECT_NAMES_MANAGER))
#define E_IS_SELECT_NAMES_MANAGER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_MANAGER))

typedef struct _ESelectNamesManager ESelectNamesManager;
typedef struct _ESelectNamesManagerClass ESelectNamesManagerClass;

struct _ESelectNamesManager {
	GtkObject object;
	
	EList *sections;
};

struct _ESelectNamesManagerClass {
	GtkObjectClass parent_class;
};

ESelectNamesManager *e_select_names_manager_new               (void);
void                 e_select_names_manager_add_section       (ESelectNamesManager *manager,
							       char *id,
							       char *title);
GtkWidget           *e_select_names_manager_create_entry      (ESelectNamesManager *manager,
							       char *id);
void                 e_select_names_manager_activate_dialog   (ESelectNamesManager *manager,
							       char *id);

/* Of type ECard */
EList               *e_select_names_manager_get_cards         (ESelectNamesManager *manager,
							       char *id);

/* Standard Gtk function */			      
GtkType              e_select_names_manager_get_type          (void);

#endif /* ! __E_SELECT_NAMES_MANAGER_H__ */
