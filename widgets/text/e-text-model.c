/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ETextModel - Text item model for evolution.
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

#undef  PARANOID_DEBUGGING

#include <config.h>
#include <ctype.h>
#include "e-text-model-repos.h"
#include "e-text-model.h"

#define CLASS(obj) (E_TEXT_MODEL_CLASS (GTK_OBJECT (obj)->klass))

enum {
	E_TEXT_MODEL_CHANGED,
	E_TEXT_MODEL_REPOSITION,
	E_TEXT_MODEL_OBJECT_ACTIVATED,
	E_TEXT_MODEL_LAST_SIGNAL
};

static guint e_text_model_signals[E_TEXT_MODEL_LAST_SIGNAL] = { 0 };

struct _ETextModelPrivate {
	gchar   *text;
	gint     len;
};

static void e_text_model_class_init (ETextModelClass *class);
static void e_text_model_init       (ETextModel *model);
static void e_text_model_destroy    (GtkObject *object);

static gint         e_text_model_real_validate_position (ETextModel *, gint pos);
static const gchar *e_text_model_real_get_text          (ETextModel *model);
static gint         e_text_model_real_get_text_length   (ETextModel *model);
static void         e_text_model_real_set_text          (ETextModel *model, const gchar *text);
static void         e_text_model_real_insert            (ETextModel *model, gint postion, const gchar *text);
static void         e_text_model_real_insert_length     (ETextModel *model, gint postion, const gchar *text, gint length);
static void         e_text_model_real_delete            (ETextModel *model, gint postion, gint length);

static GtkObject *parent_class;



/**
 * e_text_model_get_type:
 * @void: 
 * 
 * Registers the &ETextModel class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ETextModel class.
 **/
GtkType
e_text_model_get_type (void)
{
	static GtkType model_type = 0;

	if (!model_type) {
		GtkTypeInfo model_info = {
			"ETextModel",
			sizeof (ETextModel),
			sizeof (ETextModelClass),
			(GtkClassInitFunc) e_text_model_class_init,
			(GtkObjectInitFunc) e_text_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		model_type = gtk_type_unique (gtk_object_get_type (), &model_info);
	}

	return model_type;
}

/* Class initialization function for the text item */
static void
e_text_model_class_init (ETextModelClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	e_text_model_signals[E_TEXT_MODEL_CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETextModelClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_text_model_signals[E_TEXT_MODEL_REPOSITION] =
		gtk_signal_new ("reposition",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETextModelClass, reposition),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_POINTER, GTK_TYPE_POINTER);

	e_text_model_signals[E_TEXT_MODEL_OBJECT_ACTIVATED] =
		gtk_signal_new ("object_activated",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETextModelClass, object_activated),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class, e_text_model_signals, E_TEXT_MODEL_LAST_SIGNAL);
	
	/* No default signal handlers. */
	klass->changed          = NULL;
	klass->reposition       = NULL;
	klass->object_activated = NULL;
	
	klass->validate_pos  = e_text_model_real_validate_position;

	klass->get_text      = e_text_model_real_get_text;
	klass->get_text_len  = e_text_model_real_get_text_length;
	klass->set_text      = e_text_model_real_set_text;
	klass->insert        = e_text_model_real_insert;
	klass->insert_length = e_text_model_real_insert_length;
	klass->delete        = e_text_model_real_delete;

	/* We explicitly don't define default handlers for these. */
	klass->objectify        = NULL;
	klass->obj_count        = NULL;
	klass->get_nth_obj      = NULL;
	
	object_class->destroy = e_text_model_destroy;
}

/* Object initialization function for the text item */
static void
e_text_model_init (ETextModel *model)
{
	model->priv = g_new0 (struct _ETextModelPrivate, 1);
	model->priv->text = g_strdup ("");
	model->priv->len  = 0;
}

/* Destroy handler for the text item */
static void
e_text_model_destroy (GtkObject *object)
{
	ETextModel *model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (object));

	model = E_TEXT_MODEL (object);

	g_free (model->priv->text);

	g_free (model->priv);
	model->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static gint
e_text_model_real_validate_position (ETextModel *model, gint pos)
{
	gint len;

	if (pos < 0)
		pos = 0;
	else if (pos > ( len = e_text_model_get_text_length (model) ))
		pos = len;

	return pos;
}

static const gchar *
e_text_model_real_get_text (ETextModel *model)
{
	if (model->priv->text)
		return model->priv->text;
	else
		return "";
}

static gint
e_text_model_real_get_text_length (ETextModel *model)
{
	if (model->priv->len < 0)
		model->priv->len = strlen (e_text_model_get_text (model));

	return model->priv->len;
}

static void
e_text_model_real_set_text (ETextModel *model, const gchar *text)
{
	EReposAbsolute repos;
	gboolean changed = FALSE;

	if (text == NULL) {

		changed = (model->priv->text != NULL);

		g_free (model->priv->text);
		model->priv->text = NULL;
		model->priv->len = -1;

	} else if (model->priv->text == NULL || strcmp (model->priv->text, text)) {
		
		g_free (model->priv->text);
		model->priv->text = g_strdup (text);
		model->priv->len = -1;

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
e_text_model_real_insert (ETextModel *model, gint position, const gchar *text)
{
	EReposInsertShift repos;
	gchar *new_text;
	gint ins_len;

	new_text = g_strdup_printf ("%.*s%s%s", position, model->priv->text, text, model->priv->text + position);
	ins_len = strlen (text);

	if (model->priv->text)
		g_free (model->priv->text);

	model->priv->text = new_text;
	
	if (model->priv->len >= 0)
		model->priv->len += ins_len;
	
	e_text_model_changed (model);

	repos.model = model;
	repos.pos = position;
	repos.len = ins_len;

	e_text_model_reposition (model, e_repos_insert_shift, &repos);
}

static void
e_text_model_real_insert_length (ETextModel *model, gint position, const gchar *text, gint length)
{
	EReposInsertShift repos;
	gchar *new_text = g_strdup_printf ("%.*s%.*s%s", position, model->priv->text, length, text, model->priv->text + position);

	if (model->priv->text)
		g_free (model->priv->text);
	model->priv->text = new_text;

	if (model->priv->len >= 0)
		model->priv->len += length;

	e_text_model_changed (model);

	repos.model = model;
	repos.pos   = position;
	repos.len   = length;

	e_text_model_reposition (model, e_repos_insert_shift, &repos);
}

static void
e_text_model_real_delete (ETextModel *model, gint position, gint length)
{
	EReposDeleteShift repos;
 
	memmove (model->priv->text + position, model->priv->text + position + length, strlen (model->priv->text + position + length) + 1);
	
	if (model->priv->len >= 0)
		model->priv->len -= length;

	e_text_model_changed (model);

	repos.model = model;
	repos.pos   = position;
	repos.len   = length;

	e_text_model_reposition (model, e_repos_delete_shift, &repos);
}

void
e_text_model_changed (ETextModel *model)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	/*
	  Objectify before emitting any signal.
	  While this method could, in theory, do pretty much anything, it is meant
	  for scanning objects and converting substrings into embedded objects.
	*/
	if (CLASS (model)->objectify)
		CLASS (model)->objectify (model);

	gtk_signal_emit (GTK_OBJECT (model),
			 e_text_model_signals[E_TEXT_MODEL_CHANGED]);
}

void
e_text_model_reposition (ETextModel *model, ETextModelReposFn fn, gpointer repos_data)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (fn != NULL);

	gtk_signal_emit (GTK_OBJECT (model),
			 e_text_model_signals[E_TEXT_MODEL_REPOSITION],
			 fn, repos_data);
}

gint
e_text_model_validate_position (ETextModel *model, gint pos)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), 0);

	if (CLASS (model)->validate_pos)
		pos = CLASS (model)->validate_pos (model, pos);

	return pos;
}

const gchar *
e_text_model_get_text (ETextModel *model)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);

	if (CLASS (model)->get_text)
		return CLASS (model)->get_text (model);

	return "";
}

gint
e_text_model_get_text_length (ETextModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), 0);

	if (CLASS (model)->get_text_len (model)) {

		gint len = CLASS (model)->get_text_len (model);

#ifdef PARANOID_DEBUGGING
		const gchar *str = e_text_model_get_text (model);
		gint len2 = str ? strlen (str) : 0;
		if (len != len)
			g_error ("\"%s\" length reported as %d, not %d.", str, len, len2);
#endif

		return len;

	} else {
		/* Calculate length the old-fashioned way... */
		const gchar *str = e_text_model_get_text (model);
		return str ? strlen (str) : 0;
	}
}

void
e_text_model_set_text (ETextModel *model, const gchar *text)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if (CLASS (model)->set_text)
		CLASS (model)->set_text (model, text);
}

void
e_text_model_insert (ETextModel *model, gint position, const gchar *text)
{ 
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if (text == NULL)
		return;

	if (CLASS (model)->insert)
		CLASS (model)->insert (model, position, text);
}

void
e_text_model_insert_length (ETextModel *model, gint position, const gchar *text, gint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (length >= 0);


	if (text == NULL || length == 0)
		return;

	if (CLASS (model)->insert_length)
		CLASS (model)->insert_length (model, position, text, length);
}

void
e_text_model_prepend (ETextModel *model, const gchar *text)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if (text == NULL)
		return;

	e_text_model_insert (model, 0, text);
}

void
e_text_model_append (ETextModel *model, const gchar *text)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if (text == NULL)
		return;

	e_text_model_insert (model, e_text_model_get_text_length (model), text);
}

void
e_text_model_delete (ETextModel *model, gint position, gint length)
{
	gint txt_len;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (length >= 0);

	txt_len = e_text_model_get_text_length (model);
	if (position + length > txt_len)
		length = txt_len - position;

	if (length <= 0)
		return;

	if (CLASS (model)->delete)
		CLASS (model)->delete (model, position, length);
}

gint
e_text_model_object_count (ETextModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), 0);

	if (CLASS (model)->obj_count)
		return CLASS (model)->obj_count (model);

	return 0;
}

const gchar *
e_text_model_get_nth_object (ETextModel *model, gint n, gint *len)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);

	if (n < 0 || n >= e_text_model_object_count (model))
		return NULL;

	if (CLASS (model)->get_nth_obj)
		return CLASS (model)->get_nth_obj (model, n, len);

	return NULL;
}

gchar *
e_text_model_strdup_nth_object (ETextModel *model, gint n)
{
	const gchar *obj;
	gint len = 0;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);

	obj = e_text_model_get_nth_object (model, n, &len);
	
	return obj ? g_strndup (obj, n) : NULL;
}

void
e_text_model_get_nth_object_bounds (ETextModel *model, gint n, gint *start, gint *end)
{
	const gchar *txt = NULL, *obj = NULL;
	gint len = 0;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	txt = e_text_model_get_text (model);
	obj = e_text_model_get_nth_object (model, n, &len);

	g_return_if_fail (obj != NULL);

	if (start)
		*start = obj - txt;
	if (end)
		*end = obj - txt + len;
}

gint
e_text_model_get_object_at_offset (ETextModel *model, gint offset)
{
	g_return_val_if_fail (model != NULL, -1);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), -1);

	if (offset < 0 || offset >= e_text_model_get_text_length (model))
		return -1;

	/* If an optimized version has been provided, we use it. */
	if (CLASS (model)->obj_at_offset) {

		return CLASS (model)->obj_at_offset (model, offset);

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

gint
e_text_model_get_object_at_pointer (ETextModel *model, const gchar *s)
{
	g_return_val_if_fail (model != NULL, -1);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), -1);
	g_return_val_if_fail (s != NULL, -1);

	return e_text_model_get_object_at_offset (model, s - e_text_model_get_text (model));
}

void
e_text_model_activate_nth_object (ETextModel *model, gint n)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (n >= 0);
	g_return_if_fail (n < e_text_model_object_count (model));

	gtk_signal_emit (GTK_OBJECT (model), e_text_model_signals[E_TEXT_MODEL_OBJECT_ACTIVATED], n);
}

ETextModel *
e_text_model_new (void)
{
	ETextModel *model = gtk_type_new (e_text_model_get_type ());
	return model;
}
