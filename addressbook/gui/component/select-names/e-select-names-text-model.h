/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 */

#ifndef __E_SELECT_NAMES_TEXT_MODEL_H__
#define __E_SELECT_NAMES_TEXT_MODEL_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "e-select-names-model.h"
#include <widgets/e-text/e-text-model.h>

#define E_TYPE_SELECT_NAMES_TEXT_MODEL            (e_select_names_text_model_get_type ())
#define E_SELECT_NAMES_TEXT_MODEL(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_SELECT_NAMES_TEXT_MODEL, ESelectNamesTextModel))
#define E_SELECT_NAMES_TEXT_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_TEXT_MODEL, ESelectNamesTextModelClass))
#define E_IS_SELECT_NAMES_TEXT_MODEL(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_SELECT_NAMES_TEXT_MODEL))
#define E_IS_SELECT_NAMES_TEXT_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_TEXT_MODEL))

typedef struct _ESelectNamesTextModel ESelectNamesTextModel;
typedef struct _ESelectNamesTextModelClass ESelectNamesTextModelClass;

struct _ESelectNamesTextModel {
	ETextModel parent;

	ESelectNamesModel *source;
	int source_changed_id;
	int *lengths;
};

struct _ESelectNamesTextModelClass {
	ETextModelClass parent_class;
};

ETextModel *e_select_names_text_model_new  (ESelectNamesModel *source);

/* Standard Gtk function */			      
GtkType     e_select_names_text_model_get_type (void);

#endif /* ! __E_SELECT_NAMES_TEXT_MODEL_H__ */
