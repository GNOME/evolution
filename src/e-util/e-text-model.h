/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TEXT_MODEL_H
#define E_TEXT_MODEL_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_TEXT_MODEL \
	(e_text_model_get_type ())
#define E_TEXT_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TEXT_MODEL, ETextModel))
#define E_TEXT_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TEXT_MODEL, ETextModelClass))
#define E_IS_TEXT_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TEXT_MODEL))
#define E_IS_TEXT_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TEXT_MODEL))
#define E_TEXT_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TEXT_MODEL_TYPE, ETextModelClass))

G_BEGIN_DECLS

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
	void		(*changed)		(ETextModel *model);
	void		(*reposition)		(ETextModel *model,
						 ETextModelReposFn fn,
						 gpointer repos_fn_data);

	/* Virtual methods */

	gint		(*validate_pos)		(ETextModel *model,
						 gint pos);

	const gchar *	(*get_text)		(ETextModel *model);
	gint		(*get_text_len)		(ETextModel *model);
	void		(*set_text)		(ETextModel *model,
						 const gchar *text);
	void		(*insert)		(ETextModel *model,
						 gint position,
						 const gchar *text);
	void		(*insert_length)	(ETextModel *model,
						 gint position,
						 const gchar *text,
						 gint length);
	void		(*delete)		(ETextModel *model,
						 gint position,
						 gint length);

	void		(*objectify)		(ETextModel *model);
	gint		(*obj_count)		(ETextModel *model);
	const gchar *	(*get_nth_obj)		(ETextModel *model,
						 gint n,
						 gint *len);
	gint		(*obj_at_offset)	(ETextModel *model,
						 gint offset);
};

GType		e_text_model_get_type		(void) G_GNUC_CONST;
ETextModel *	e_text_model_new		(void);
void		e_text_model_changed		(ETextModel *model);
void		e_text_model_reposition		(ETextModel *model,
						 ETextModelReposFn fn,
						 gpointer repos_data);
gint		e_text_model_validate_position	(ETextModel *model,
						 gint pos);
const gchar *	e_text_model_get_text		(ETextModel *model);
gint		e_text_model_get_text_length	(ETextModel *model);
void		e_text_model_set_text		(ETextModel *model,
						 const gchar *text);
void		e_text_model_insert		(ETextModel *model,
						 gint position,
						 const gchar *text);
void		e_text_model_insert_length	(ETextModel *model,
						 gint position,
						 const gchar *text,
						 gint length);
void		e_text_model_delete		(ETextModel *model,
						 gint position,
						 gint length);
gint		e_text_model_object_count	(ETextModel *model);
const gchar *	e_text_model_get_nth_object	(ETextModel *model,
						 gint n,
						 gint *len);
void		e_text_model_get_nth_object_bounds
						(ETextModel *model,
						 gint n,
						 gint *start_pos,
						 gint *end_pos);
gint		e_text_model_get_object_at_offset
						(ETextModel *model,
						 gint offset);

G_END_DECLS

#endif /* E_TEXT_MODEL_H */
