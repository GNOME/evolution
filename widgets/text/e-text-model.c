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

#include <config.h>
#include <ctype.h>
#include "e-text-model.h"

enum {
	E_TEXT_MODEL_CHANGED,
	E_TEXT_MODEL_POSITION,
	E_TEXT_MODEL_LAST_SIGNAL
};

static guint e_text_model_signals[E_TEXT_MODEL_LAST_SIGNAL] = { 0 };

static void e_text_model_class_init (ETextModelClass *class);
static void e_text_model_init (ETextModel *model);
static void e_text_model_destroy (GtkObject *object);

static gchar *e_text_model_real_get_text(ETextModel *model);
static void e_text_model_real_set_text(ETextModel *model, gchar *text);
static void e_text_model_real_insert(ETextModel *model, gint position, gchar *text);
static void e_text_model_real_insert_length(ETextModel *model, gint position, gchar *text, gint length);
static void e_text_model_real_delete(ETextModel *model, gint position, gint length);

static gint e_text_model_real_object_count(ETextModel *model);
static const gchar *e_text_model_real_get_nth_object(ETextModel *model, gint n);
static void e_text_model_real_activate_nth_object(ETextModel *mode, gint n);


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

	e_text_model_signals[E_TEXT_MODEL_POSITION] =
		gtk_signal_new ("position",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETextModelClass, position),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class, e_text_model_signals, E_TEXT_MODEL_LAST_SIGNAL);
	
	klass->changed = NULL;
	klass->get_text = e_text_model_real_get_text;
	klass->set_text = e_text_model_real_set_text;
	klass->insert = e_text_model_real_insert;
	klass->insert_length = e_text_model_real_insert_length;
	klass->delete = e_text_model_real_delete;
	klass->obj_count = e_text_model_real_object_count;
	klass->get_nth_obj = e_text_model_real_get_nth_object;
	klass->activate_nth_obj = e_text_model_real_activate_nth_object;
	
	object_class->destroy = e_text_model_destroy;
}

/* Object initialization function for the text item */
static void
e_text_model_init (ETextModel *model)
{
	model->text = NULL;
}

/* Destroy handler for the text item */
static void
e_text_model_destroy (GtkObject *object)
{
	ETextModel *model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (object));

	model = E_TEXT_MODEL (object);

	if (model->text)
		g_free (model->text);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static gchar *
e_text_model_real_get_text(ETextModel *model)
{
	if (model->text)
		return model->text;
	else
		return "";
}

static void
e_text_model_real_set_text(ETextModel *model, gchar *text)
{
	if (model->text)
		g_free(model->text);
	model->text = g_strdup(text);
	e_text_model_changed(model);
}

static void
e_text_model_real_insert(ETextModel *model, gint position, gchar *text)
{
	gchar *temp;

	g_return_if_fail (0<= position && position <= strlen (model->text));

	temp = g_strdup_printf("%.*s%s%s", position, model->text, text, model->text + position);

	if (model->text)
		g_free(model->text);
	model->text = temp;
	e_text_model_changed(model);

	e_text_model_suggest_position (model, position + strlen(text));
}

static void
e_text_model_real_insert_length(ETextModel *model, gint position, gchar *text, gint length)
{
	gchar *temp;

	g_return_if_fail (0 <= position && position <= strlen (model->text));

	temp = g_strdup_printf("%.*s%.*s%s", position, model->text, length, text, model->text + position);

	if (model->text)
		g_free(model->text);
	model->text = temp;
	e_text_model_changed(model);

	e_text_model_suggest_position (model, position + length);
}

static void
e_text_model_real_delete(ETextModel *model, gint position, gint length)
{
	g_return_if_fail (0 <= position && position <= strlen (model->text));
	
	memmove(model->text + position, model->text + position + length, strlen(model->text + position + length) + 1);
	e_text_model_changed(model);
}

static gint
e_text_model_real_object_count(ETextModel *model)
{
	gint count = 0;
	gchar *c = model->text;

	if (c) {
		while (*c) {
			if (*c == '\1')
				++count;
			++c;
		}
	}
	return count;
}

static const gchar *
e_text_model_real_get_nth_object(ETextModel *model, gint n)
{
	return "";
}

static void
e_text_model_real_activate_nth_object(ETextModel *model, gint n)
{
	/* By default, do nothing */
}

void
e_text_model_changed(ETextModel *model)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model),
			 e_text_model_signals [E_TEXT_MODEL_CHANGED]);
}

gchar *
e_text_model_get_text(ETextModel *model)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->get_text )
		return E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->get_text(model);
	else
		return "";
}

void
e_text_model_set_text(ETextModel *model, gchar *text)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->set_text )
		E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->set_text(model, text);
}

void
e_text_model_insert(ETextModel *model, gint position, gchar *text)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->insert )
		E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->insert(model, position, text);
}

void
e_text_model_insert_length(ETextModel *model, gint position, gchar *text, gint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->insert_length )
		E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->insert_length(model, position, text, length);
}

void
e_text_model_delete(ETextModel *model, gint position, gint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->delete )
		E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->delete(model, position, length);
}

void
e_text_model_suggest_position(ETextModel *model, gint position)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (0 <= position);
	g_return_if_fail (position <= strlen (model->text));

	gtk_signal_emit (GTK_OBJECT (model), e_text_model_signals[E_TEXT_MODEL_POSITION], position);
}

gint
e_text_model_object_count(ETextModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), 0);

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->obj_count)
		return E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->obj_count(model);
	else
		return 0;
}

const gchar *
e_text_model_get_nth_object(ETextModel *model, gint n)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);
	g_return_val_if_fail (n >= 0, NULL);

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->get_nth_obj )
		return E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->get_nth_obj(model, n);
	else
		return "";
}

void
e_text_model_activate_nth_object(ETextModel *model, gint n)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (model));
	g_return_if_fail (n >= 0);

	if ( E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->activate_nth_obj )
		E_TEXT_MODEL_CLASS(GTK_OBJECT(model)->klass)->activate_nth_obj(model, n);
}

gchar *
e_text_model_strdup_expanded_text(ETextModel *model)
{
	gint len = 0, i, N;
	gchar *expanded, *dest;
	const gchar *src;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TEXT_MODEL (model), NULL);

	if (model->text == NULL)
		return NULL;

	N = e_text_model_object_count (model);
	if (N == 0)
		return g_strdup (model->text);

	/* First, compute the length of the expanded string. */

	len = strlen (model->text);
	len -= N; /* Subtract out the \1s that signify the objects. */

	for (i=0; i<N; ++i)
		len += strlen (e_text_model_get_nth_object (model, i));

	/* Next, allocate and build the expanded string. */
	expanded = g_new0 (gchar, len+2);

	src = model->text;
	dest = expanded;
	i = 0;
	while (*src) {
		if (*src == '\1') {
			const gchar *src_obj;
			
			g_assert (i < N);
			src_obj = e_text_model_get_nth_object (model, i);

			if (src_obj) {
				while (*src_obj) {
					*dest = *src_obj;
					++src_obj;
					++dest;
				}
			}
			
			++src;
			++i;

		} else {

			*dest = *src;
			++src;
			++dest;

		}
	}

	return expanded;
}

ETextModel *
e_text_model_new(void)
{
	ETextModel *model = gtk_type_new (e_text_model_get_type ());
	model->text = g_strdup("");
	return model;
}
