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
#define E_SELECT_NAMES_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SELECT_NAMES_MANAGER, ESelectNamesManager))
#define E_SELECT_NAMES_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_MANAGER, ESelectNamesManagerClass))
#define E_IS_SELECT_NAMES_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SELECT_NAMES_MANAGER))
#define E_IS_SELECT_NAMES_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_MANAGER))

typedef struct _ESelectNamesManager ESelectNamesManager;
typedef struct _ESelectNamesManagerClass ESelectNamesManagerClass;

struct _ESelectNamesManager {
	GObject object;
	
	GList *sections;
	GList *entries;

	ESelectNames *names;

	ESourceList *source_list;	
	GList *completion_books;
	
	int minimum_query_length;

	GList *notifications;
};

struct _ESelectNamesManagerClass {
	GObjectClass parent_class;

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
GType                e_select_names_manager_get_type               (void);

#endif /* ! __E_SELECT_NAMES_MANAGER_H__ */
