/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 */

#ifndef __E_SELECT_NAMES_MODEL_H__
#define __E_SELECT_NAMES_MODEL_H__

#include <time.h>
#include <gtk/gtkobject.h>
#include <stdio.h>
#include <e-util/e-list.h>
#include <addressbook/backend/ebook/e-card.h>
#include <addressbook/backend/ebook/e-destination.h>

#define E_TYPE_SELECT_NAMES_MODEL            (e_select_names_model_get_type ())
#define E_SELECT_NAMES_MODEL(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_SELECT_NAMES_MODEL, ESelectNamesModel))
#define E_SELECT_NAMES_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_MODEL, ESelectNamesModelClass))
#define E_IS_SELECT_NAMES_MODEL(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_SELECT_NAMES_MODEL))
#define E_IS_SELECT_NAMES_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_MODEL))

typedef struct _ESelectNamesModel ESelectNamesModel;
typedef struct _ESelectNamesModelClass ESelectNamesModelClass;
struct _ESelectNamesModelPrivate;

struct _ESelectNamesModel {
	GtkObject object;
	
	struct _ESelectNamesModelPrivate *priv;
};

struct _ESelectNamesModelClass {
	GtkObjectClass parent_class;

	void (*changed) (ESelectNamesModel *model);
	void (*resized) (ESelectNamesModel *model, gint index, gint old_len, gint new_len);
};

GtkType e_select_names_model_get_type  (void);

ESelectNamesModel *e_select_names_model_new       (void);
ESelectNamesModel *e_select_names_model_duplicate (ESelectNamesModel *old);

const gchar  *e_select_names_model_get_textification (ESelectNamesModel *model);
const gchar  *e_select_names_model_get_address_text  (ESelectNamesModel *model);

gint                e_select_names_model_count           (ESelectNamesModel *model);
const EDestination *e_select_names_model_get_destination (ESelectNamesModel *model, gint index);
ECard              *e_select_names_model_get_card        (ESelectNamesModel *model, gint index);
const gchar        *e_select_names_model_get_string      (ESelectNamesModel *model, gint index);

void          e_select_names_model_insert     (ESelectNamesModel *model, gint index, EDestination *dest);
void          e_select_names_model_replace    (ESelectNamesModel *model, gint index, EDestination *dest);
void          e_select_names_model_delete     (ESelectNamesModel *model, gint index);
void          e_select_names_model_delete_all (ESelectNamesModel *model);

void          e_select_names_model_name_pos (ESelectNamesModel *model, gint index, gint *pos, gint *length);
void          e_select_names_model_text_pos (ESelectNamesModel *model, gint pos, gint *index, gint *start_pos, gint *length);

#endif /* ! __E_SELECT_NAMES_MODEL_H__ */
