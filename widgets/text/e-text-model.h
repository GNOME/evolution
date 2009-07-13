/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_TEXT_MODEL_H
#define E_TEXT_MODEL_H

#include <glib-object.h>

G_BEGIN_DECLS

#define E_TYPE_TEXT_MODEL            (e_text_model_get_type ())
#define E_TEXT_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TEXT_MODEL, ETextModel))
#define E_TEXT_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TEXT_MODEL, ETextModelClass))
#define E_IS_TEXT_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TEXT_MODEL))
#define E_IS_TEXT_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_TEXT_MODEL))
#define E_TEXT_MODEL_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), E_TYPE_TEXT_MODEL_TYPE, ETextModelClass))

typedef struct _ETextModel ETextModel;
typedef struct _ETextModelClass ETextModelClass;
typedef struct _ETextModelPrivate ETextModelPrivate;

typedef gint (*ETextModelReposFn) (gint, gpointer);

struct _ETextModel {
	GObject item;

	ETextModelPrivate *priv;
};

struct _ETextModelClass {
	GObjectClass parent_class;

	/* Signal */
	void  (* changed)           (ETextModel *model);
	void  (* reposition)        (ETextModel *model, ETextModelReposFn fn, gpointer repos_fn_data);
	void  (* object_activated)  (ETextModel *model, gint obj_num);
	void  (* cancel_completion) (ETextModel *model);

	/* Virtual methods */

	gint  (* validate_pos) (ETextModel *model, gint pos);

	const gchar *(* get_text)      (ETextModel *model);
	gint        (* get_text_len)  (ETextModel *model);
	void        (* set_text)      (ETextModel *model, const gchar *text);
	void        (* insert)        (ETextModel *model, gint position, const gchar *text);
	void        (* insert_length) (ETextModel *model, gint position, const gchar *text, gint length);
	void        (* delete)        (ETextModel *model, gint position, gint length);

	void         (* objectify)          (ETextModel *model);
	gint         (* obj_count)          (ETextModel *model);
	const gchar *(* get_nth_obj)        (ETextModel *model, gint n, gint *len);
	gint         (* obj_at_offset)      (ETextModel *model, gint offset);
};

GType       e_text_model_get_type (void);

ETextModel *e_text_model_new (void);

void        e_text_model_changed (ETextModel *model);
void        e_text_model_cancel_completion (ETextModel *model);

void        e_text_model_reposition        (ETextModel *model, ETextModelReposFn fn, gpointer repos_data);
gint        e_text_model_validate_position (ETextModel *model, gint pos);

/* Functions for manipulating the underlying text. */

const gchar *e_text_model_get_text        (ETextModel *model);
gint         e_text_model_get_text_length (ETextModel *model);
void         e_text_model_set_text        (ETextModel *model, const gchar *text);
void         e_text_model_insert          (ETextModel *model, gint position, const gchar *text);
void         e_text_model_insert_length   (ETextModel *model, gint position, const gchar *text, gint length);
void         e_text_model_prepend         (ETextModel *model, const gchar *text);
void         e_text_model_append          (ETextModel *model, const gchar *text);
void         e_text_model_delete          (ETextModel *model, gint position, gint length);

/* Functions for accessing embedded objects. */

gint         e_text_model_object_count          (ETextModel *model);
const gchar *e_text_model_get_nth_object        (ETextModel *model, gint n, gint *len);
gchar       *e_text_model_strdup_nth_object     (ETextModel *model, gint n);
void         e_text_model_get_nth_object_bounds (ETextModel *model, gint n, gint *start_pos, gint *end_pos);
gint         e_text_model_get_object_at_offset  (ETextModel *model, gint offset);
gint         e_text_model_get_object_at_pointer (ETextModel *model, const gchar *c);
void         e_text_model_activate_nth_object   (ETextModel *model, gint n);

G_END_DECLS

#endif
