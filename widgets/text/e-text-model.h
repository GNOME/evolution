/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ETextModel - Text item for evolution.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Chris Lahey <clahey@umich.edu>
 *
 * A majority of code taken from:
 *
 * Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx> */


#ifndef E_TEXT_MODEL_H
#define E_TEXT_MODEL_H

#include <gnome.h>


BEGIN_GNOME_DECLS

#define E_TYPE_TEXT_MODEL            (e_text_model_get_type ())
#define E_TEXT_MODEL(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_TEXT_MODEL, ETextModel))
#define E_TEXT_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TEXT_MODEL, ETextModelClass))
#define E_IS_TEXT_MODEL(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_TEXT_MODEL))
#define E_IS_TEXT_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_TEXT_MODEL))

typedef struct _ETextModel ETextModel;
typedef struct _ETextModelClass ETextModelClass;

struct _ETextModelPrivate;

typedef gint (*ETextModelReposFn) (gint, gpointer);

struct _ETextModel {
	GtkObject item;

	struct _ETextModelPrivate *priv;
};

struct _ETextModelClass {
	GtkObjectClass parent_class;

	/* Signal */
	void  (* changed)          (ETextModel *model);
	void  (* reposition)       (ETextModel *model, ETextModelReposFn fn, gpointer repos_fn_data);
	void  (* object_activated) (ETextModel *model, gint obj_num);

	/* Virtual methods */

	gint  (* validate_pos) (ETextModel *model, gint pos);

	const char *(* get_text)      (ETextModel *model);
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

GtkType     e_text_model_get_type (void);

ETextModel *e_text_model_new (void);

void        e_text_model_changed (ETextModel *model);

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







END_GNOME_DECLS

#endif
