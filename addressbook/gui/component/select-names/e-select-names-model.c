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
#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include "e-select-names-model.h"
#include "e-select-names-marshal.h"
#include "eab-book-util.h"

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

	GList *data;  /* of EDestination */

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
		EDestination *dup = e_destination_copy (E_DESTINATION (iter->data));
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
			EDestination *dest = E_DESTINATION (iter->data);
			strv[i] = (gchar *) e_destination_get_textrep (dest, FALSE);
			++i;
			iter = g_list_next (iter);
		}
		
		text = g_strjoinv (separator, strv);
		
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
			EDestination *dest = E_DESTINATION (iter->data);
			strv[i] = (gchar *) e_destination_get_address (dest);
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

const EDestination *
e_select_names_model_get_destination (ESelectNamesModel *model, gint index)
{
	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (0 <= index, NULL);
	g_return_val_if_fail (index < g_list_length (model->priv->data), NULL);

	return E_DESTINATION (g_list_nth_data (model->priv->data, index));
}

gchar *
e_select_names_model_export_destinationv (ESelectNamesModel *model)
{
	EDestination **destv;
	gchar *str;
	gint i, len = 0;
	GList *j;
	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);

	len = g_list_length (model->priv->data);
	destv = g_new0 (EDestination *, len+1);
	
	for (i=0, j = model->priv->data; j != NULL; j = g_list_next (j)) {
		EDestination *dest = E_DESTINATION (j->data);

		if (dest)
			destv[i++] = dest;
	}

	str = e_destination_exportv (destv);
	g_free (destv);

	return str;
}

void
e_select_names_model_import_destinationv (ESelectNamesModel *model,
					  gchar *destinationv)
{
	EDestination **destv;
	gint i;

	g_return_if_fail (model && E_IS_SELECT_NAMES_MODEL (model));

	destv = e_destination_importv (destinationv);

	e_select_names_model_delete_all (model);

	if (destv == NULL)
		return;

	for (i = 0; destv[i]; i++) {
		e_select_names_model_append (model, destv[i]);
	}
	g_free (destv);
}

EContact *
e_select_names_model_get_contact (ESelectNamesModel *model, gint index)
{
	const EDestination *dest;

	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (0 <= index, NULL);
	g_return_val_if_fail (index < g_list_length (model->priv->data), NULL);

	dest = e_select_names_model_get_destination (model, index);
	return dest ? e_destination_get_contact (dest) : NULL;

}

const gchar *
e_select_names_model_get_string (ESelectNamesModel *model, gint index)
{
	const EDestination *dest;

	g_return_val_if_fail (model && E_IS_SELECT_NAMES_MODEL (model), NULL);
	g_return_val_if_fail (0 <= index, NULL);
	g_return_val_if_fail (index < g_list_length (model->priv->data), NULL);

	dest = e_select_names_model_get_destination (model, index);
	
	return dest ? e_destination_get_textrep (dest, FALSE) : "";
}

gboolean
e_select_names_model_contains (ESelectNamesModel *model, const EDestination *dest)
{
	GList *iter;

	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), FALSE);
	g_return_val_if_fail (E_IS_DESTINATION (dest), FALSE);

	for (iter = model->priv->data; iter != NULL; iter = g_list_next (iter)) {
		if (iter->data != NULL && e_destination_equal (dest, E_DESTINATION (iter->data)))
			return TRUE;
	}

	return FALSE;
}

void
e_select_names_model_insert (ESelectNamesModel *model, gint index, EDestination *dest)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (0 <= index && index <= g_list_length (model->priv->data));
	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	if (e_select_names_model_at_limit (model)) {
		/* FIXME: This is bad. */
		g_object_unref (dest);
		return;
	}

	model->priv->data = g_list_insert (model->priv->data, dest, index);
	
	g_object_ref (dest);

	e_select_names_model_changed (model);
}

void
e_select_names_model_append (ESelectNamesModel *model, EDestination *dest)
{
	g_return_if_fail (model && E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	if (e_select_names_model_at_limit (model)) {
		/* FIXME: This is bad. */
		g_object_unref (dest);
		return;
	}

	model->priv->data = g_list_append (model->priv->data, dest);

	g_object_ref  (dest);

	e_select_names_model_changed (model);
}

void
e_select_names_model_replace (ESelectNamesModel *model, gint index, EDestination *dest)
{
	GList *node;
	const gchar *new_str, *old_str;
	gint old_strlen=0, new_strlen=0;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (model->priv->data == NULL || (0 <= index && index < g_list_length (model->priv->data)));
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	
	new_str = e_destination_get_textrep (dest, FALSE);
	new_strlen = new_str ? strlen (new_str) : 0;

	if (model->priv->data == NULL) {

		model->priv->data = g_list_append (model->priv->data, dest);
		g_object_ref (dest);

	} else {
		
		node = g_list_nth (model->priv->data, index);

		if (node->data != dest) {

			old_str = e_destination_get_textrep (E_DESTINATION (node->data), FALSE);
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
	EDestination *dest;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));
	g_return_if_fail (0 <= index && index < g_list_length (model->priv->data));
	
	node = g_list_nth (model->priv->data, index);
	dest = E_DESTINATION (node->data);

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
		EDestination *dest;

		next = g_list_next (iter);

		if (next == NULL && !clean_last_entry)
			break;
		
		dest = iter->data ? E_DESTINATION (iter->data) : NULL;

		if (dest == NULL || e_destination_empty (dest)) {
			if (dest)
				g_object_unref (dest);
			model->priv->data = g_list_remove_link (model->priv->data, iter);
			g_list_free_1 (iter);
			changed = TRUE;
		}
		
		iter = next;
	}

	if (changed)
		e_select_names_model_changed (model);
}

void
e_select_names_model_delete_all (ESelectNamesModel *model)
{
	g_return_if_fail (model != NULL && E_IS_SELECT_NAMES_MODEL (model));

	g_list_foreach (model->priv->data, (GFunc)g_object_unref, model);
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
		const EDestination *d = e_select_names_model_get_destination (src, i);
		if (d)
			e_select_names_model_append (dest, e_destination_copy (d));
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
		const EDestination *d = e_select_names_model_get_destination (src, i);
		if (d && !e_select_names_model_contains (dest, d))
			e_select_names_model_append (dest, e_destination_copy (d));
	}
}

typedef struct {
	EDestination *dest;
	ESelectNamesModel *model;
} ModelDestClosure;

static void
name_and_email_simple_query_cb (EBook *book, EBookStatus status, GList *contacts, gpointer closure)
{
	ModelDestClosure *c = closure;
	EDestination *dest = c->dest;
	ESelectNamesModel *model = c->model;
	EContact *contact;
	int num_non_list_contacts = 0;
	GList *l;

	g_free (c);

	if (status == E_BOOK_ERROR_OK) {
		for (l = contacts; l; l = l->next) {
			EContact *c = E_CONTACT (l->data);
			if (!e_contact_get (c, E_CONTACT_IS_LIST)) {
				num_non_list_contacts++;
				contact = c;
			}
		}

		if (num_non_list_contacts == 1) {
			const char *email = e_destination_get_email (dest);
			int email_num = 0;
		
			if (email && *email) {
				GList *email_list = e_contact_get (contact, E_CONTACT_EMAIL);
				GList *l;

				for (l = email_list; l; l = l->next) {
					if (!g_ascii_strcasecmp (email, l->data))
						break;
					email_num++;
				}
				if (l == NULL)
					email_num = -1;
			}

			if (email_num >= 0) {
				e_destination_set_contact (dest, contact, email_num);
				e_select_names_model_changed (model);
			}
		}
	}
	
	
	g_object_unref (dest);
	g_object_unref (model);
	g_object_unref (book);
}

static void
book_opened (EBook *book, EBookStatus status, gpointer closure)
{
	ESelectNamesModel *model = closure;
	GList *iter;

	for (iter = model->priv->data; iter != NULL; iter = g_list_next (iter)) {
		ModelDestClosure *c = g_new (ModelDestClosure, 1);

		c->dest = g_object_ref (E_DESTINATION (iter->data));
		c->model = g_object_ref (model);

		if (e_destination_is_evolution_list (c->dest))
			continue;
	
		if (e_destination_get_contact (c->dest))
			continue;

		g_object_ref (book);

		eab_name_and_email_query (book,
					  e_destination_get_name (c->dest),
					  e_destination_get_email (c->dest),
					  name_and_email_simple_query_cb,
					  c);
	}


	g_object_unref (model);
	g_object_unref (book);
}

void
e_select_names_model_load_contacts (ESelectNamesModel *model)
{
	EBook *book;

	g_return_if_fail (E_IS_SELECT_NAMES_MODEL (model));

	if (model->priv->data) {
		g_object_ref (model);

		book = e_book_new_default_addressbook (NULL);

		e_book_async_open (book, TRUE, book_opened, model);
	}
}

void
e_select_names_cancel_contacts_load (ESelectNamesModel *model)
{
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
		str = e_destination_get_textrep (E_DESTINATION (iter->data), FALSE);
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
		str = e_destination_get_textrep (E_DESTINATION (iter->data), FALSE);
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
