/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#undef  PARANOID_DEBUGGING

#include "evolution-config.h"

#include "e-text-model.h"

#include <ctype.h>
#include <string.h>

#include <gtk/gtk.h>

#include "e-marshal.h"
#include "e-text-model-repos.h"

enum {
	E_TEXT_MODEL_CHANGED,
	E_TEXT_MODEL_REPOSITION,
	E_TEXT_MODEL_LAST_SIGNAL
};

static guint signals[E_TEXT_MODEL_LAST_SIGNAL] = { 0 };

struct _ETextModelPrivate {
	GString *text;
};

static gint	e_text_model_real_validate_position
						(ETextModel *, gint pos);
static const gchar *
		e_text_model_real_get_text	(ETextModel *model);
static gint	e_text_model_real_get_text_length
						(ETextModel *model);
static void	e_text_model_real_set_text	(ETextModel *model,
						 const gchar *text);
static void	e_text_model_real_insert	(ETextModel *model,
						 gint postion,
						 const gchar *text);
static void	e_text_model_real_insert_length	(ETextModel *model,
						 gint postion,
						 const gchar *text,
						 gint length);
static void	e_text_model_real_delete	(ETextModel *model,
						 gint postion,
						 gint length);

G_DEFINE_TYPE_WITH_PRIVATE (ETextModel, e_text_model, G_TYPE_OBJECT)

static void
e_text_model_finalize (GObject *object)
{
	ETextModel *self = E_TEXT_MODEL (object);

	g_string_free (self->priv->text, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_text_model_parent_class)->finalize (object);
}

static void
e_text_model_class_init (ETextModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_text_model_finalize;

	signals[E_TEXT_MODEL_CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETextModelClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[E_TEXT_MODEL_REPOSITION] = g_signal_new (
		"reposition",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETextModelClass, reposition),
		NULL, NULL,
		e_marshal_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_POINTER);

	/* No default signal handlers. */
	class->changed = NULL;
	class->reposition = NULL;

	class->validate_pos = e_text_model_real_validate_position;

	class->get_text = e_text_model_real_get_text;
	class->get_text_len = e_text_model_real_get_text_length;
	class->set_text = e_text_model_real_set_text;
	class->insert = e_text_model_real_insert;
	class->insert_length = e_text_model_real_insert_length;
	class->delete = e_text_model_real_delete;

	/* We explicitly don't define default handlers for these. */
	class->objectify = NULL;
	class->obj_count = NULL;
	class->get_nth_obj = NULL;
}

static void
e_text_model_init (ETextModel *model)
{
	model->priv = e_text_model_get_instance_private (model);
	model->priv->text = g_string_new ("");
}

static gint
e_text_model_real_validate_position (ETextModel *model,
                                     gint pos)
{
	gint len = e_text_model_get_text_length (model);

	if (pos < 0)
		pos = 0;
	else if (pos > len)
		pos = len;

	return pos;
}

static const gchar *
e_text_model_real_get_text (ETextModel *model)
{
	if (model->priv->text)
		return model->priv->text->str;
	else
		return "";
}

static gint
e_text_model_real_get_text_length (ETextModel *model)
{
	return g_utf8_strlen (model->priv->text->str, -1);
}

static void
e_text_model_real_set_text (ETextModel *model,
                            const gchar *text)
{
	EReposAbsolute repos;
	gboolean changed = FALSE;

	if (text == NULL) {
		changed = (*model->priv->text->str != '\0');

		g_string_set_size (model->priv->text, 0);

	} else if (*model->priv->text->str == '\0' ||
		strcmp (model->priv->text->str, text)) {

		g_string_assign (model->priv->text, text);

		changed = TRUE;
	}

	if (changed) {
		e_text_model_changed (model);
		repos.model = model;
		repos.pos = -1;
		e_text_model_reposition (model, e_repos_absolute, &repos);
	}
}

static void
e_text_model_real_insert (ETextModel *model,
                          gint position,
                          const gchar *text)
{
	e_text_model_insert_length (model, position, text, strlen (text));
}

static void
e_text_model_real_insert_length (ETextModel *model,
                                 gint position,
                                 const gchar *text,
                                 gint length)
{
	EReposInsertShift repos;
	gint model_len = e_text_model_real_get_text_length (model);
	gchar *offs;
	const gchar *p;
	gint byte_length, l;

	if (position > model_len)
		return;

	offs = g_utf8_offset_to_pointer (model->priv->text->str, position);

	for (p = text, l = 0;
	     l < length;
	     p = g_utf8_next_char (p), l++);

	byte_length = p - text;

	g_string_insert_len (
		model->priv->text,
		offs - model->priv->text->str,
		text, byte_length);

	e_text_model_changed (model);

	repos.model = model;
	repos.pos = position;
	repos.len = length;

	e_text_model_reposition (model, e_repos_insert_shift, &repos);
}

static void
e_text_model_real_delete (ETextModel *model,
                          gint position,
                          gint length)
{
	EReposDeleteShift repos;
	gint byte_position, byte_length;
	gchar *offs, *p;
	gint l;

	offs = g_utf8_offset_to_pointer (model->priv->text->str, position);
	byte_position = offs - model->priv->text->str;

	for (p = offs, l = 0;
	     l < length;
	     p = g_utf8_next_char (p), l++);

	byte_length = p - offs;

	g_string_erase (
		model->priv->text,
		byte_position, byte_length);

	e_text_model_changed (model);

	repos.model = model;
	repos.pos = position;
	repos.len = length;

	e_text_model_reposition (model, e_repos_delete_shift, &repos);
}

void
e_text_model_changed (ETextModel *model)
{
	ETextModelClass *class;

	g_return_if_fail (E_IS_TEXT_MODEL (model));

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);

	/*
	  Objectify before emitting any signal.
	  While this method could, in theory, do pretty much anything, it is meant
	  for scanning objects and converting substrings into embedded objects.
	*/
	if (class->objectify != NULL)
		class->objectify (model);

	g_signal_emit (model, signals[E_TEXT_MODEL_CHANGED], 0);
}

void
e_text_model_reposition (ETextModel *model,
                         ETextModelReposFn fn,
                         gpointer repos_data)
{
	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (fn != NULL);

	g_signal_emit (
		model, signals[E_TEXT_MODEL_REPOSITION], 0, fn, repos_data);
}

gint
e_text_model_validate_position (ETextModel *model,
                                gint pos)
{
	ETextModelClass *class;

	g_return_val_if_fail (E_IS_TEXT_MODEL (model), 0);

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, 0);

	if (class->validate_pos != NULL)
		pos = class->validate_pos (model, pos);

	return pos;
}

const gchar *
e_text_model_get_text (ETextModel *model)
{
	ETextModelClass *class;

	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, NULL);

	if (class->get_text == NULL)
		return "";

	return class->get_text (model);
}

gint
e_text_model_get_text_length (ETextModel *model)
{
	ETextModelClass *class;

	g_return_val_if_fail (E_IS_TEXT_MODEL (model), 0);

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, 0);

	if (class->get_text_len (model)) {

		gint len = class->get_text_len (model);

#ifdef PARANOID_DEBUGGING
		const gchar *str = e_text_model_get_text (model);
		gint len2 = str ? g_utf8_strlen (str, -1) : 0;
		if (len != len)
			g_error ("\"%s\" length reported as %d, not %d.", str, len, len2);
#endif

		return len;

	} else {
		/* Calculate length the old-fashioned way... */
		const gchar *str = e_text_model_get_text (model);
		return str ? g_utf8_strlen (str, -1) : 0;
	}
}

void
e_text_model_set_text (ETextModel *model,
                       const gchar *text)
{
	ETextModelClass *class;

	g_return_if_fail (E_IS_TEXT_MODEL (model));

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);

	if (class->set_text != NULL)
		class->set_text (model, text);
}

void
e_text_model_insert (ETextModel *model,
                     gint position,
                     const gchar *text)
{
	ETextModelClass *class;

	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if (text == NULL)
		return;

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);

	if (class->insert != NULL)
		class->insert (model, position, text);
}

void
e_text_model_insert_length (ETextModel *model,
                            gint position,
                            const gchar *text,
                            gint length)
{
	ETextModelClass *class;

	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (length >= 0);

	if (text == NULL || length == 0)
		return;

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);

	if (class->insert_length != NULL)
		class->insert_length (model, position, text, length);
}

void
e_text_model_delete (ETextModel *model,
                     gint position,
                     gint length)
{
	ETextModelClass *class;
	gint txt_len;

	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (length >= 0);

	txt_len = e_text_model_get_text_length (model);
	if (position + length > txt_len)
		length = txt_len - position;

	if (length <= 0)
		return;

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);

	if (class->delete != NULL)
		class->delete (model, position, length);
}

gint
e_text_model_object_count (ETextModel *model)
{
	ETextModelClass *class;

	g_return_val_if_fail (E_IS_TEXT_MODEL (model), 0);

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, 0);

	if (class->obj_count == NULL)
		return 0;

	return class->obj_count (model);
}

const gchar *
e_text_model_get_nth_object (ETextModel *model,
                             gint n,
                             gint *len)
{
	ETextModelClass *class;

	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);

	if (n < 0 || n >= e_text_model_object_count (model))
		return NULL;

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, NULL);

	if (class->get_nth_obj == NULL)
		return NULL;

	return class->get_nth_obj (model, n, len);
}

void
e_text_model_get_nth_object_bounds (ETextModel *model,
                                    gint n,
                                    gint *start,
                                    gint *end)
{
	const gchar *txt = NULL, *obj = NULL;
	gint len = 0;

	g_return_if_fail (E_IS_TEXT_MODEL (model));

	txt = e_text_model_get_text (model);
	obj = e_text_model_get_nth_object (model, n, &len);

	g_return_if_fail (obj != NULL);

	if (start)
		*start = g_utf8_pointer_to_offset (txt, obj);
	if (end)
		*end = (start ? *start : 0) + len;
}

gint
e_text_model_get_object_at_offset (ETextModel *model,
                                   gint offset)
{
	ETextModelClass *class;

	g_return_val_if_fail (E_IS_TEXT_MODEL (model), -1);

	if (offset < 0 || offset >= e_text_model_get_text_length (model))
		return -1;

	class = E_TEXT_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, -1);

	/* If an optimized version has been provided, we use it. */
	if (class->obj_at_offset != NULL) {
		return class->obj_at_offset (model, offset);

	} else {
		/* If not, we fake it.*/

		gint i, N, pos0, pos1;

		N = e_text_model_object_count (model);

		for (i = 0; i < N; ++i) {
			e_text_model_get_nth_object_bounds (model, i, &pos0, &pos1);
			if (pos0 <= offset && offset < pos1)
				return i;
		}

	}

	return -1;
}

ETextModel *
e_text_model_new (void)
{
	ETextModel *model = g_object_new (E_TYPE_TEXT_MODEL, NULL);
	return model;
}
