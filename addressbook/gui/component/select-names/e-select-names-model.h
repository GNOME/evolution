/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 */

#ifndef __E_SELECT_NAMES_MODEL_H__
#define __E_SELECT_NAMES_MODEL_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <e-util/e-list.h>
#include <addressbook/backend/ebook/e-card.h>

#define E_TYPE_SELECT_NAMES_MODEL            (e_select_names_model_get_type ())
#define E_SELECT_NAMES_MODEL(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_SELECT_NAMES_MODEL, ESelectNamesModel))
#define E_SELECT_NAMES_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_MODEL, ESelectNamesModelClass))
#define E_IS_SELECT_NAMES_MODEL(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_SELECT_NAMES_MODEL))
#define E_IS_SELECT_NAMES_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_MODEL))

typedef enum   _ESelectNamesModelDataType ESelectNamesModelDataType;
typedef struct _ESelectNamesModelData ESelectNamesModelData;
typedef struct _ESelectNamesModel ESelectNamesModel;
typedef struct _ESelectNamesModelClass ESelectNamesModelClass;

enum _ESelectNamesModelDataType {
	E_SELECT_NAMES_MODEL_DATA_TYPE_CARD,
	E_SELECT_NAMES_MODEL_DATA_TYPE_STRING_ADDRESS,
	E_SELECT_NAMES_MODEL_DATA_TYPE_SEPARATION_MATERIAL,
};

struct _ESelectNamesModelData {
	ESelectNamesModelDataType type;
	ECard *card;
	char *string;
};

struct _ESelectNamesModel {
	GtkObject object;

	char *id;
	char *title;
	
	EList *data; /* Of type ESelectNamesModelData. */
};

struct _ESelectNamesModelClass {
	GtkObjectClass parent_class;

	void (*changed) (ESelectNamesModel *model);
};

ESelectNamesModel *e_select_names_model_new               (void);

/* These lengths are allowed to go over objects and act just like the text model does. */
void               e_select_names_model_insert            (ESelectNamesModel *model,
							   EIterator *iterator, /* Must be one of the iterators in the model. */
							   int index,
							   char *data);
void               e_select_names_model_insert_length     (ESelectNamesModel *model,
							   EIterator *iterator, /* Must be one of the iterators in the model. */
							   int index,
							   char *data,
							   int length);
void               e_select_names_model_delete            (ESelectNamesModel *model,
							   EIterator *iterator, /* Must be one of the iterators in the model. */
							   int index,
							   int length);
void               e_select_names_model_replace           (ESelectNamesModel *model,
							   EIterator *iterator, /* Must be one of the iterators in the model. */
							   int index,
							   int replacement_length,
							   char *data);

void               e_select_names_model_add_item          (ESelectNamesModel *model,
							   EIterator *iterator, /* NULL for at the beginning. */
							   ESelectNamesModelData *data);
void               e_select_names_model_remove_item       (ESelectNamesModel *model,
							   EIterator *iterator);

/* Of type ECard */
EList             *e_select_names_model_get_cards         (ESelectNamesModel *model);

/* Of type ESelectNamesModelData */
EList             *e_select_names_model_get_data          (ESelectNamesModel *model);

/* Standard Gtk function */			      
GtkType            e_select_names_model_get_type          (void);

#endif /* ! __E_SELECT_NAMES_MODEL_H__ */
