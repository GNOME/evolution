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
#include <libebook/e-contact.h>
#include <addressbook/util/e-destination.h>

#define E_TYPE_SELECT_NAMES_MODEL            (e_select_names_model_get_type ())
#define E_SELECT_NAMES_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SELECT_NAMES_MODEL, ESelectNamesModel))
#define E_SELECT_NAMES_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_MODEL, ESelectNamesModelClass))
#define E_IS_SELECT_NAMES_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SELECT_NAMES_MODEL))
#define E_IS_SELECT_NAMES_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_SELECT_NAMES_MODEL))

typedef struct _ESelectNamesModel ESelectNamesModel;
typedef struct _ESelectNamesModelClass ESelectNamesModelClass;
struct _ESelectNamesModelPrivate;

struct _ESelectNamesModel {
	GObject object;
	
	struct _ESelectNamesModelPrivate *priv;
};

struct _ESelectNamesModelClass {
	GObjectClass parent_class;

	void (*changed) (ESelectNamesModel *model);
	void (*resized) (ESelectNamesModel *model, gint index, gint old_len, gint new_len);
};

GType   e_select_names_model_get_type  (void);

ESelectNamesModel *e_select_names_model_new       (void);
ESelectNamesModel *e_select_names_model_duplicate (ESelectNamesModel *old);

gchar             *e_select_names_model_get_textification (ESelectNamesModel *model, const char *separator);
gchar             *e_select_names_model_get_address_text  (ESelectNamesModel *model, const char *separator);

gint                e_select_names_model_count               (ESelectNamesModel *model);
gint                e_select_names_model_get_limit           (ESelectNamesModel *model);
void                e_select_names_model_set_limit           (ESelectNamesModel *model, gint limit);
gboolean            e_select_names_model_at_limit            (ESelectNamesModel *model);

const EDestination   *e_select_names_model_get_destination     (ESelectNamesModel *model, gint index);
gchar                *e_select_names_model_export_destinationv (ESelectNamesModel *model);
void                  e_select_names_model_import_destinationv (ESelectNamesModel *model,
							      gchar *destinationv);
EContact           *e_select_names_model_get_contact         (ESelectNamesModel *model, gint index);
const gchar        *e_select_names_model_get_string          (ESelectNamesModel *model, gint index);

gboolean      e_select_names_model_contains       (ESelectNamesModel *model, const EDestination *dest);

void          e_select_names_model_insert         (ESelectNamesModel *model, gint index, EDestination *dest);
void          e_select_names_model_append         (ESelectNamesModel *model, EDestination *dest);
void          e_select_names_model_replace        (ESelectNamesModel *model, gint index, EDestination *dest);
void          e_select_names_model_delete         (ESelectNamesModel *model, gint index);
void          e_select_names_model_delete_all     (ESelectNamesModel *model);
void          e_select_names_model_overwrite_copy (ESelectNamesModel *dest, ESelectNamesModel *src);
void          e_select_names_model_merge          (ESelectNamesModel *dest, ESelectNamesModel *src);

void          e_select_names_model_clean      (ESelectNamesModel *model, gboolean clean_last_entry);

void          e_select_names_model_name_pos (ESelectNamesModel *model, gint seplen, gint index, gint *pos, gint *length);
void          e_select_names_model_text_pos (ESelectNamesModel *model, gint seplen, gint pos, gint *index, gint *start_pos, gint *length);

void          e_select_names_model_load_contacts  (ESelectNamesModel *model);
void          e_select_names_cancel_contacts_load (ESelectNamesModel *model);

/* This is a mildly annoying freeze/thaw pair, in that it only applies to the 'changed'
   signal and not to 'resized'.  This could cause unexpected results in some cases. */
void          e_select_names_model_freeze (ESelectNamesModel *model);
void          e_select_names_model_thaw   (ESelectNamesModel *model);


#endif /* ! __E_SELECT_NAMES_MODEL_H__ */
