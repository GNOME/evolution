/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc.
 */

#ifndef __E_SELECT_NAMES_MANAGER_H__
#define __E_SELECT_NAMES_MANAGER_H__

#include <stdio.h>
#include <time.h>
#include <gtk/gtkobject.h>
#include <e-util/e-list.h>
#include "e-select-names.h"

#define E_TYPE_SELECT_NAMES_MANAGER            (e_select_names_manager_get_type ())
#define E_SELECT_NAMES_MANAGER(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_SELECT_NAMES_MANAGER, ESelectNamesManager))
#define E_SELECT_NAMES_MANAGER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_MANAGER, ESelectNamesManagerClass))
#define E_IS_SELECT_NAMES_MANAGER(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_SELECT_NAMES_MANAGER))
#define E_IS_SELECT_NAMES_MANAGER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_MANAGER))

typedef struct _ESelectNamesManager ESelectNamesManager;
typedef struct _ESelectNamesManagerClass ESelectNamesManagerClass;

struct _ESelectNamesManager {
	GtkObject object;
	
	GList *sections;
	GList *entries;

	ESelectNames *names;

	GList *completion_books;
};

struct _ESelectNamesManagerClass {
	GtkObjectClass parent_class;

	void (*changed) (ESelectNamesManager *, const gchar *section_id, gint changed_working_copy);
	void (*ok)      (ESelectNamesManager *);
	void (*cancel)  (ESelectNamesManager *);
};

ESelectNamesManager *e_select_names_manager_new                    (void);
void                 e_select_names_manager_add_section            (ESelectNamesManager *manager,
							            const char *id,
							            const char *title);
void                 e_select_names_manager_add_section_with_limit (ESelectNamesManager *manager,
								    const char *id,
								    const char *title,
								    gint limit);
ESelectNamesModel   *e_select_names_manager_get_source             (ESelectNamesManager *manager,
								    const char *id);
GtkWidget           *e_select_names_manager_create_entry           (ESelectNamesManager *manager,
							            const char *id);
void                 e_select_names_manager_activate_dialog        (ESelectNamesManager *manager,
							            const char *id);
/* Standard Gtk function */			      
GtkType              e_select_names_manager_get_type               (void);

#endif /* ! __E_SELECT_NAMES_MANAGER_H__ */
