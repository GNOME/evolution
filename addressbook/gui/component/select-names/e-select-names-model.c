/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@ximian.com>
 *   Jon Trowbidge   <trow@ximian.com>
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtksignal.h>

#include <gal/util/e-util.h>

#include "e-select-names-model.h"
#include "e-select-names-marshal.h"
#include "addressbook/backend/ebook/e-contact.h"

#define MAX_LENGTH 2047


enum {
	E_SELECT_NAMES_MODEL_CHANGED,
	E_SELECT_NAMES_MODEL_RESIZED,
	E_SELECT_NAMES_MODEL_LAST_SIGNAL
};

static guint e_select_names_model_signals[E_SELECT_NAMES_MODEL_LAST_SIGNAL] = { 0 };

/* Object argument IDs */
enum {
	ARG_0,
	ARG_CONTACT,
};

struct _ESelectNamesModelPrivate {
	gchar *id;
	gchar *title;

	GList *data;  /* of EABDestination */

	gint limit;

	gint freeze_count;
	gboolean pending_changed;
};

static GObjectClass *parent_class = NULL;

static void e_select_names_model_init (ESelectNamesModel *model);
static void e_select_names_model_class_init (ESelectNamesModelClass *klass);

static void e_select_names_model_dispose (GObject *object);

GType
e_select_names_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ESelectNamesModelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_select_names_model_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESelectNamesModel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_select_names_model_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, "ESelectNamesModel", &info, 0);
	}

	return type;
}

static void
e_select_names_model_class_init (ESelectNamesModelClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	parent_class = g_type_class_peek_parent (klass);

	e_select_names_model_signals[E_SELECT_NAMES_MODEL_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESelectNamesModelClass, changed),
			      NULL, NULL,
			      e_select_names_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_select_names_model_signals[E_SELECT_NAMES_MODEL_RESIZED] =
		g_signal_new ("resized",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESelectNamesModelClass, resized),
			      NULL, NULL,
			      e_select_names_marshal_NONE__INT_INT_INT,
			      G_TYPE_NONE, 3,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_INT);

	klass->changed = NULL;

	object_class->dispose = e_select_names_model_dispose;
}

/**
 * e_select_names_model_init:
 */
static void
e_select_names_model_init (ESelectNamesModel *model)
{
	model->priv = g_new0 (struct _ESelectNamesModelPrivate, 1);

	model->priv->limit = -1;
}

static void
e_select_names_model_dispose (GObject *object)
{
	ESelectNamesModel *model = E_SELECT_NAMES_MODEL (object);

	if (model->priv) {
		g_free (model->priv->title);
		g_free (model->priv->id);

		g_list_foreach (model->priv->data, (GFunc) g_object_unref, NULL);
		g_list_free (model->priv->data);

		g_free (model->priv);
		model->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
e_select_names_model_changed (ESelectNamesModel *model)
{
	if (model->priv->freeze_count > 0) {
		model->priv->pending_changed = TRUE;
	} else {
		g_signal_emit (model, e_select_names_model_signals[E_SELECT_NAMES_MODEL_CHANGED], 0);
		model->priv->pending_changed = FALSE;
	}
}

static void
destination_changed_proxy (EABDestination *dest, gpointer closure)
{
	e_select_names_model_changed (E_SELECT_NAMES_MODEL (closure));
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

ESelectNamesModel *
e_select_names_model_new (void)
{
	ESelectNamesModel *model;
	model = g_object_new (E_TYPE_SELECT_NAMES_MODEL, NULL);
	return model;
}

ESelectNamesModel *
e_select_names_model_duplicate (ESelectNamesModel *old)
{
	ESelectNamesModel *model = e_select_names_model_new ();
	GList *iter;

	model->priv->id = g_strdup (old->priv->id);
	model->priv->title = g_strdup (old->priv->title);
	
	for (iter = old->priv->data; iter != NULL; iter = g_list_next (iter)) {
		EABDestination *dup = eab_destination_copy (EAB_DESTINATION (iter->data));
		e_select_names_model_append (model, dup);
	}

	model->priv->limit = old->priv->limit;

	return model;
}

gchar *
e_select_names_model_get_textification (ESelectNamesModel *model, const char *separator)
{
	gchar *text;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (separator && *separator, NULL);

	if (model->priv->data == NULL) {
		
		text = g_strdup ("");

	} else {
		gchar **strv = g_new0 (gchar *, g_list_length (model->priv->data)+1);
		gint i = 0;
		GList *iter = model->priv->data;
		
		while (iter) {
			EABDestination *dest = EAB_DESTINATION (iter->data);
			strv[i] = (gchar *) eab_destination_get_textrep (dest, FALSE);
			++i;
			iter = g_list_next (iter);
		}
		
		text = g_strjoinv (separator, strv);

		if (g_utf8_strlen(text, -1) > MAX_LENGTH) {
			char *p = g_utf8_offset_to_pointer (text, MAX_LENGTH);
			*p = '\0';
			text = g_realloc (text, p - text + 1);
		}
		
		g_free (strv);
		
	}

	return text;
}

gchar *
e_select_names_model_get_address_text (ESelectNamesModel *model, const char *separator)
{
	gchar *addr_text;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (separator && *separator, NULL);

	if (model->priv->data == NULL) {

		addr_text = g_strdup ("");
		
	} else {
		gchar **strv = g_new0 (gchar *, g_list_length (model->priv->data)+1);
		gint i = 0;
		GList *iter = model->priv->data;

		while (iter) {
			EABDestination *dest = EAB_DESTINATION (iter->data);
			strv[i] = (gchar *) eab_destination_get_address (dest);
			if (strv[i])
				++i;
			iter = g_list_next (iter);
		}
		
		addr_text = g_strjoinv (separator, strv);
		
		g_free (strv);
		
	}

	return addr_text;
}

gint
e_select_names_model_count (ESelectNamesModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), 0);

	return g_list_length (model->priv->data);
}

gint
e_select_names_model_get_limit (ESelectNamesModel *model)
{
	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), 0);

	return model->priv->limit;
}

void
e_select_names_model_set_limit (ESelectNamesModel *model, gint limit)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));

	model->priv->limit = MAX (limit, -1);
}

gboolean
e_select_names_model_at_limit (ESelectNamesModel *model)
{
	g_return_val_if_fail (model != NULL, TRUE);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), TRUE);

	return model->priv->limit >= 0 && g_list_length (model->priv->data) >= model->priv->limit;
}

const EABDestination *
e_select_names_model_get_destination (ESelectNamesModel *model, gint index)
{
	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (0 <= index, NULL);
	g_return_val_if_fail (index < g_list_length (model->priv->data), NULL);

	return EAB_DESTINATION (g_list_nth_data (model->priv->data, index));
}

gchar *
e_select_names_model_export_destinationv (ESelectNamesModel *model)
{
	EABDestination **destv;
	gchar *str;
	gint i, len = 0;
	GList *j;
	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);

	len = g_list_length (model->priv->data);
	destv = g_new0 (EABDestination *, len+1);
	
	for (i=0, j = model->priv->data; j != NULL; j = g_list_next (j)) {
		EABDestination *dest = EAB_DESTINATION (j->data);

		if (dest)
			destv[i++] = dest;
	}

	str = eab_destination_exportv (destv);
	g_free (destv);

	return str;
}

static void send_changed (EABDestination *dest, EContact *contact, gpointer closure)
{
	ESelectNamesModel *model = closure;
	e_select_names_model_changed (model);
}

void
e_select_names_model_import_destinationv (ESelectNamesModel *model,
					  gchar *destinationv)
{
	EABDestination **destv;
	gint i;

	g_return_if_fail (model && E_IS_SELECT_NAMES_MODEL (model));

	destv = eab_destination_importv (destinationv);

	e_select_names_model_delete_all (model);

	if (destv == NULL)
		return;

	for (i = 0; destv[i]; i++) {
		eab_destination_use_contact (destv[i], send_changed, model);
		e_select_names_model_append (model, destv[i]);
	}
	g_free (destv);
}

EContact *
e_select_names_model_get_contact (ESelectNamesModel *model, gint index)
{
	const EABDestination *dest;

	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (0 <= index, NULL);
	g_return_val_if_fail (index < g_list_length (model->priv->data), NULL);

	dest = e_select_names_model_get_destination (model, index);
	return dest ? eab_destination_get_contact (dest) : NULL;

}

const gchar *
e_select_names_model_get_string (ESelectNamesModel *model, gint index)
{
	const EABDestination *dest;

	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (0 <= index, NULL);
	g_return_val_if_fail (index < g_list_length (model->priv->data), NULL);

	dest = e_select_names_model_get_destination (model, index);
	
	return dest ? eab_destination_get_textrep (dest, FALSE) : "";
}

static void
connect_destination (ESelectNamesModel *model, EABDestination *dest)
{
	g_signal_connect (dest,
			  "changed",
			  G_CALLBACK (destination_changed_proxy),
			  model);
}

static void
disconnect_destination (ESelectNamesModel *model, EABDestination *dest)
{
	g_signal_handlers_disconnect_by_func (dest, destination_changed_proxy, model);
}

gboolean
e_select_names_model_contains (ESelectNamesModel *model, const EABDestination *dest)
{
	GList *iter;

	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), FALSE);
	g_return_val_if_fail (EAB_IS_DESTINATION (dest), FALSE);

	for (iter = model->priv->data; iter != NULL; iter = g_list_next (iter)) {
		if (iter->data != NULL && eab_destination_equal (dest, EAB_DESTINATION (iter->data)))
			return TRUE;
	}

	return FALSE;
}

void
e_select_names_model_insert (ESelectNamesModel *model, gint index, EABDestination *dest)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (0 <= index && index <= g_list_length (model->priv->data));
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));

	if (e_select_names_model_at_limit (model)) {
		/* FIXME: This is bad. */
		g_object_unref (dest);
		return;
	}

	connect_destination (model, dest);

	model->priv->data = g_list_insert (model->priv->data, dest, index);
	
	g_object_ref (dest);

	e_select_names_model_changed (model);
}

void
e_select_names_model_append (ESelectNamesModel *model, EABDestination *dest)
{
	g_return_if_fail (model && E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));

	if (e_select_names_model_at_limit (model)) {
		/* FIXME: This is bad. */
		g_object_unref (dest);
		return;
	}

	connect_destination (model, dest);

	model->priv->data = g_list_append (model->priv->data, dest);

	g_object_ref  (dest);

	e_select_names_model_changed (model);
}

void
e_select_names_model_replace (ESelectNamesModel *model, gint index, EABDestination *dest)
{
	GList *node;
	const gchar *new_str, *old_str;
	gint old_strlen=0, new_strlen=0;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (model->priv->data == NULL || (0 <= index && index < g_list_length (model->priv->data)));
	g_return_if_fail (dest && EAB_IS_DESTINATION (dest));
	
	new_str = eab_destination_get_textrep (dest, FALSE);
	new_strlen = new_str ? strlen (new_str) : 0;

	if (model->priv->data == NULL) {

		connect_destination (model, dest);

		model->priv->data = g_list_append (model->priv->data, dest);
		g_object_ref (dest);

	} else {
		
		node = g_list_nth (model->priv->data, index);

		if (node->data != dest) {

			disconnect_destination (model, EAB_DESTINATION (node->data));
			connect_destination (model, dest);

			old_str = eab_destination_get_textrep (EAB_DESTINATION (node->data), FALSE);
			old_strlen = old_str ? strlen (old_str) : 0;

			g_object_unref (node->data);

			node->data = dest;
			g_object_ref (dest);
		}
	}

	e_select_names_model_changed (model);

	g_signal_emit (model, e_select_names_model_signals[E_SELECT_NAMES_MODEL_RESIZED], 0,
		       index, old_strlen, new_strlen);
}

void
e_select_names_model_delete (ESelectNamesModel *model, gint index)
{
	GList *node;
	EABDestination *dest;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (0 <= index && index < g_list_length (model->priv->data));
	
	node = g_list_nth (model->priv->data, index);
	dest = EAB_DESTINATION (node->data);

	disconnect_destination (model, dest);
	g_object_unref (dest);

	model->priv->data = g_list_remove_link (model->priv->data, node);
	g_list_free_1 (node);

	e_select_names_model_changed (model);
}

void
e_select_names_model_clean (ESelectNamesModel *model, gboolean clean_last_entry)
{
	GList *iter, *next;
	gboolean changed = FALSE;

	g_return_if_fail (model != NULL && E_IS_SELECT_NAMES_MODEL (model));

	iter = model->priv->data;

	while (iter) {
		EABDestination *dest;

		next = g_list_next (iter);

		if (next == NULL && !clean_last_entry)
			break;
		
		dest = iter->data ? EAB_DESTINATION (iter->data) : NULL;

		if (dest == NULL || eab_destination_is_empty (dest)) {
			if (dest) {
				disconnect_destination (model, dest);
				g_object_unref (dest);
			}
			model->priv->data = g_list_remove_link (model->priv->data, iter);
			g_list_free_1 (iter);
			changed = TRUE;
		}
		
		iter = next;
	}

	if (changed)
		e_select_names_model_changed (model);
}

static void
delete_all_iter (gpointer data, gpointer closure)
{
	disconnect_destination (E_SELECT_NAMES_MODEL (closure), EAB_DESTINATION (data));
	g_object_unref (data);
}

void
e_select_names_model_delete_all (ESelectNamesModel *model)
{
	g_return_if_fail (model != NULL && E_IS_SELECT_NAMES_MODEL (model));

	g_list_foreach (model->priv->data, delete_all_iter, model);
	g_list_free (model->priv->data);
	model->priv->data = NULL;

	e_select_names_model_changed (model);
}

void
e_select_names_model_overwrite_copy (ESelectNamesModel *dest, ESelectNamesModel *src)
{
	gint i, len;

	g_return_if_fail (dest && E_IS_SELECT_NAMES_MODEL (dest));
	g_return_if_fail (src && E_IS_SELECT_NAMES_MODEL (src));

	if (src == dest)
		return;

	e_select_names_model_delete_all (dest);
	len = e_select_names_model_count (src);
	for (i = 0; i < len; ++i) {
		const EABDestination *d = e_select_names_model_get_destination (src, i);
		if (d)
			e_select_names_model_append (dest, eab_destination_copy (d));
	}
}

void
e_select_names_model_merge (ESelectNamesModel *dest, ESelectNamesModel *src)
{
	gint i, len;

	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (dest));
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (src));

	if (src == dest)
		return;

	len = e_select_names_model_count (src);
	for (i = 0; i < len; ++i) {
		const EABDestination *d = e_select_names_model_get_destination (src, i);
		if (d && !e_select_names_model_contains (dest, d))
			e_select_names_model_append (dest, eab_destination_copy (d));
	}
}

void
e_select_names_model_name_pos (ESelectNamesModel *model, gint seplen, gint index, gint *pos, gint *length)
{
	gint rp = 0, i, len = 0;
	GList *iter;
	const gchar *str;

	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (seplen > 0);

	i = 0;
	iter = model->priv->data;
	while (iter && i <= index) {
		rp += len + (i > 0 ? seplen : 0);
		str = eab_destination_get_textrep (EAB_DESTINATION (iter->data), FALSE);
		len = str ? g_utf8_strlen (str, -1) : 0;
		++i;
		iter = g_list_next (iter);
	}
	
	if (i <= index) {
		rp = -1;
		len = 0;
	}
	
	if (pos)
		*pos = rp;
	if (length)
		*length = len;
}

void
e_select_names_model_text_pos (ESelectNamesModel *model, gint seplen, gint pos, gint *index, gint *start_pos, gint *length)
{
	GList *iter;
	const gchar *str;
	gint len = 0, i = 0, sp = 0, adj = 0;

	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (seplen > 0);

	iter = model->priv->data;

	while (iter != NULL) {
		str = eab_destination_get_textrep (EAB_DESTINATION (iter->data), FALSE);
		len = str ? g_utf8_strlen (str, -1) : 0;

		if (sp <= pos && pos <= sp + len + adj) {
			break;
		}

		sp += len + adj + 1;
		adj = seplen-1;
		++i;

		iter = g_list_next (iter);
	}

	if (i != 0)
		sp += seplen-1; /* skip past "magic space" */

	if (iter == NULL) {
#if 0
		g_print ("text_pos ended NULL\n");
#endif
		i = -1;
		sp = -1;
		len = 0;
	} else {
#if 0
		g_print ("text_pos got index %d\n", i);
#endif
	}

	if (index)
		*index = i;
	if (start_pos)
		*start_pos = sp;
	if (length)
		*length = len;
}

void
e_select_names_model_load_all_contacts (ESelectNamesModel *model, EBook *book)
{
	GList *iter;

	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (book == NULL || E_IS_BOOK (book));

	for (iter = model->priv->data; iter != NULL; iter = g_list_next (iter)) {
		EABDestination *dest = EAB_DESTINATION (iter->data);
		if (!eab_destination_is_empty (dest)) {

			eab_destination_load_contact (dest, book);
		}
	}
}

void
e_select_names_model_cancel_all_contact_load (ESelectNamesModel *model)
{
	GList *iter;

	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));

	for (iter = model->priv->data; iter != NULL; iter = g_list_next (iter)) {
		EABDestination *dest = EAB_DESTINATION (iter->data);
		if (!eab_destination_is_empty (dest)) {

			eab_destination_cancel_contact_load (dest);
		}
	}
}

void
e_select_names_model_freeze (ESelectNamesModel *model)
{
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	
	++model->priv->freeze_count;
}

void
e_select_names_model_thaw (ESelectNamesModel *model)
{
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (model->priv->freeze_count > 0);

	--model->priv->freeze_count;
	if (model->priv->pending_changed)
		e_select_names_model_changed (model);
}
