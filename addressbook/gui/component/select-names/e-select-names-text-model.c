/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey    <clahey@ximian.com>
 *   Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gal/e-text/e-text-model-repos.h>

#include <addressbook/contact-editor/e-contact-editor.h>
#include "e-select-names-text-model.h"

static FILE *out = NULL; /* stream for debugging spew */

#define SEPLEN 2

/* Object argument IDs */
enum {
	ARG_0,
	ARG_SOURCE,
};

static void e_select_names_text_model_class_init (ESelectNamesTextModelClass *klass);
static void e_select_names_text_model_init       (ESelectNamesTextModel *model);
static void e_select_names_text_model_destroy    (GtkObject *object);
static void e_select_names_text_model_set_arg    (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_text_model_get_arg    (GtkObject *object, GtkArg *arg, guint arg_id);

static void e_select_names_text_model_set_source (ESelectNamesTextModel *model, ESelectNamesModel *source);

static const gchar *e_select_names_text_model_get_text      (ETextModel *model);
static void         e_select_names_text_model_set_text      (ETextModel *model, const gchar *text);
static void         e_select_names_text_model_insert        (ETextModel *model, gint position, const gchar *text);
static void         e_select_names_text_model_insert_length (ETextModel *model, gint position, const gchar *text, gint length);
static void         e_select_names_text_model_delete        (ETextModel *model, gint position, gint length);

static gint         e_select_names_text_model_obj_count   (ETextModel *model);
static const gchar *e_select_names_text_model_get_nth_obj (ETextModel *model, gint n, gint *len);
static void         e_select_names_text_model_activate_obj (ETextModel *model, gint n);


ETextModelClass *parent_class;
#define PARENT_TYPE e_text_model_get_type()

/**
 * e_select_names_text_model_get_type:
 * @void: 
 * 
 * Registers the &ESelectNamesTextModel class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ESelectNamesTextModel class.
 **/
GtkType
e_select_names_text_model_get_type (void)
{
	static GtkType model_type = 0;

	if (!model_type) {
		GtkTypeInfo model_info = {
			"ESelectNamesTextModel",
			sizeof (ESelectNamesTextModel),
			sizeof (ESelectNamesTextModelClass),
			(GtkClassInitFunc) e_select_names_text_model_class_init,
			(GtkObjectInitFunc) e_select_names_text_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		model_type = gtk_type_unique (PARENT_TYPE, &model_info);
	}

	return model_type;
}

static void
e_select_names_text_model_class_init (ESelectNamesTextModelClass *klass)
{
	GtkObjectClass *object_class;
	ETextModelClass *text_model_class;

	object_class = GTK_OBJECT_CLASS(klass);
	text_model_class = E_TEXT_MODEL_CLASS(klass);

	parent_class = gtk_type_class(PARENT_TYPE);

	gtk_object_add_arg_type ("ESelectNamesTextModel::source",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_SOURCE);

	object_class->destroy = e_select_names_text_model_destroy;
	object_class->get_arg = e_select_names_text_model_get_arg;
	object_class->set_arg = e_select_names_text_model_set_arg;

	text_model_class->get_text      = e_select_names_text_model_get_text;
	text_model_class->set_text      = e_select_names_text_model_set_text;
	text_model_class->insert        = e_select_names_text_model_insert;
	text_model_class->insert_length = e_select_names_text_model_insert_length;
	text_model_class->delete        = e_select_names_text_model_delete;

	text_model_class->obj_count     = e_select_names_text_model_obj_count;
	text_model_class->get_nth_obj   = e_select_names_text_model_get_nth_obj;
	text_model_class->object_activated = e_select_names_text_model_activate_obj;

	if (getenv ("EVO_DEBUG_SELECT_NAMES_TEXT_MODEL")) {
		out = fopen ("/tmp/evo-debug-select-names-text-model", "w");
		if (out)
			setvbuf (out, NULL, _IONBF, 0);
	}
}

static void
dump_model (ESelectNamesTextModel *text_model)
{
	ESelectNamesModel *model = text_model->source;
	gint i;

	if (out == NULL)
		return;
	
	fprintf (out, "\n*** Model State: count=%d\n", e_select_names_model_count (model));

	for (i=0; i<e_select_names_model_count (model); ++i)
		fprintf (out, "[%d] \"%s\" %s\n", i,
			 e_select_names_model_get_string (model, i),
			 e_select_names_model_get_card (model, i) ? "<card>" : "");
	fprintf (out, "\n");
}

static void
e_select_names_text_model_init (ESelectNamesTextModel *model)
{
}

static void
e_select_names_text_model_destroy (GtkObject *object)
{
	ESelectNamesTextModel *model;
	
	model = E_SELECT_NAMES_TEXT_MODEL (object);
	
	e_select_names_text_model_set_source (model, NULL);
	
	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

static void
e_select_names_text_model_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesTextModel *model;
	
	model = E_SELECT_NAMES_TEXT_MODEL (object);

	switch (arg_id) {
	case ARG_SOURCE:
		e_select_names_text_model_set_source(model, E_SELECT_NAMES_MODEL (GTK_VALUE_OBJECT (*arg)));
		break;
	default:
		return;
	}
}

static void
e_select_names_text_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesTextModel *model;

	model = E_SELECT_NAMES_TEXT_MODEL (object);

	switch (arg_id) {
	case ARG_SOURCE:
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(model->source);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
resize_cb (ESelectNamesModel *source, gint index, gint old_len, gint new_len, ETextModel *model)
{
	EReposDeleteShift repos_del;
	EReposInsertShift repos_ins;
	gint pos;

	e_select_names_model_name_pos (source, index, &pos, NULL);

	if (new_len < old_len) {

		repos_del.model = model;
		repos_del.pos = pos;
		repos_del.len = old_len - new_len;
		e_text_model_reposition (model, e_repos_delete_shift, &repos_del);

	} else if (old_len < new_len) {

		repos_ins.model = model;
		repos_ins.pos = pos;
		repos_ins.len = new_len - old_len;
		e_text_model_reposition (model, e_repos_insert_shift, &repos_ins);

	}
}


static void
e_select_names_text_model_set_source (ESelectNamesTextModel *model,
				      ESelectNamesModel     *source)
{
	if (source == model->source)
		return;
	
	if (model->source) {
		gtk_signal_disconnect (GTK_OBJECT (model->source), model->source_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (model->source), model->source_resize_id);
		gtk_object_unref (GTK_OBJECT (model->source));
	}

	model->source = source;

	if (model->source) {
		gtk_object_ref (GTK_OBJECT (model->source));
		model->source_changed_id = gtk_signal_connect_object (GTK_OBJECT(model->source),
								      "changed",
								      GTK_SIGNAL_FUNC (e_text_model_changed),
								      GTK_OBJECT (model));
		model->source_resize_id = gtk_signal_connect (GTK_OBJECT(model->source),
							      "resized",
							      GTK_SIGNAL_FUNC (resize_cb),
							      model);
	}
}

ETextModel *
e_select_names_text_model_new (ESelectNamesModel *source)
{
	ETextModel *model = E_TEXT_MODEL (gtk_type_new (e_select_names_text_model_get_type()));
	e_select_names_text_model_set_source (E_SELECT_NAMES_TEXT_MODEL (model), source);
	return model;
}

static const gchar *
e_select_names_text_model_get_text (ETextModel *model)
{
	ESelectNamesTextModel *snm = E_SELECT_NAMES_TEXT_MODEL(model);

	return snm ? e_select_names_model_get_textification (snm->source) : "";
}

static void
e_select_names_text_model_set_text (ETextModel *model, const gchar *text)
{
	ESelectNamesTextModel *snm = E_SELECT_NAMES_TEXT_MODEL(model);

	e_select_names_model_delete_all (snm->source);
	e_select_names_text_model_insert (model, 0, text);
}

static void
e_select_names_text_model_insert (ETextModel *model, gint position, const gchar *text)
{
	e_select_names_text_model_insert_length (model, position, text, strlen (text));
}

static void
e_select_names_text_model_insert_length (ETextModel *model, gint pos, const gchar *text, gint length)
{
	ESelectNamesModel *source = E_SELECT_NAMES_TEXT_MODEL (model)->source;
	gint i;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_TEXT_MODEL (model));

	if (out) {
		gchar *tmp = g_strndup (text, length);
		fprintf (out, ">> insert  \"%s\" (len=%d) at %d\n", tmp, length, pos);
		g_free (tmp);
	}

	pos = CLAMP (pos, 0, strlen (e_select_names_model_get_textification (source)));

	/* We want to control all cursor motions ourselves, rather than taking hints
	   from the ESelectNamesModel. */
	gtk_signal_handler_block (GTK_OBJECT (source), E_SELECT_NAMES_TEXT_MODEL (model)->source_resize_id);

	/* We handle this one character at a time. */

	for (i = 0; i < length && text[i]; ++i) {
		gint index, start_pos, text_len;
		gboolean inside_quote = FALSE;

		if (out) 
			fprintf (out, "processing [%c]\n", text[i]);

		e_select_names_model_text_pos (source, pos, &index, &start_pos, &text_len);

		if (out) 
			fprintf (out, "index=%d start_pos=%d text_len=%d\n", index, start_pos, text_len);

		if (text[i] == ',' && index >= 0) { /* Is this a quoted or an unquoted comma we are dealing with? */
			const EDestination *dest = e_select_names_model_get_destination (source, index);
			if (dest) {
				const gchar *str = e_destination_get_string (dest);
				gint j;
				if (out)
					fprintf (out, "str=%s pos=%d\n", str, pos);
				for (j=0; j<pos-start_pos && str[j]; ++j)
					if (str[j] == '"') {
						inside_quote = !inside_quote;
						if (out)
							fprintf (out, "flip to %d at %d\n", start_pos+j, inside_quote);
					}
			}
			if (out)
				fprintf (out, inside_quote ? "inside quote\n" : "not inside quote\n");
		}


		if (text[i] == ',' && !inside_quote) {

			/* This is the case of hitting , first thing in an empty entry */
			if (index == -1) {
				EReposAbsolute repos;

				e_select_names_model_insert (source, 0, e_destination_new ());
				e_select_names_model_insert (source, 0, e_destination_new ());

				repos.model = model;
				repos.pos = -1; /* At end */
				e_text_model_reposition (model, e_repos_absolute, &repos);
				

			} else if (pos <= start_pos || pos == start_pos + text_len) {
				EReposInsertShift repos;
				gint ins_point = index;

				if (text_len != 0 && pos == start_pos + text_len)
					++ins_point;
				
				/* Block adjacent blank cards. */
				if (! ((ins_point < e_select_names_model_count (source) &&
					(e_select_names_model_get_string (source, ins_point) == NULL)) 
				       || (ins_point > 0 && (e_select_names_model_get_string (source, ins_point-1) == NULL)))) {

					e_select_names_model_insert (source, ins_point, e_destination_new ());

					repos.model = model;
					repos.pos = pos;
					repos.len = SEPLEN;
					e_text_model_reposition (model, e_repos_insert_shift, &repos);
					pos += SEPLEN;
				}

			} else {
				EReposInsertShift repos;
				gint offset = MAX (pos - start_pos, 0);
				const gchar *str = e_select_names_model_get_string (source, index);
				gchar *str1 = g_strndup (str, offset);
				gchar *str2 = g_strdup (str+offset);
				EDestination *d1 = e_destination_new (), *d2 = e_destination_new ();

				e_destination_set_string (d1, str1);
				e_destination_set_string (d2, str2);

				e_select_names_model_replace (source, index, d1);
				e_select_names_model_insert (source, index+1, d2);

				g_free (str1);
				g_free (str2);

				repos.model = model;
				repos.pos = pos;
				repos.len = SEPLEN;
				e_text_model_reposition (model, e_repos_insert_shift, &repos);
				pos += SEPLEN;
			}

		} else {
			EReposInsertShift repos;
			gint offset = MAX (pos - start_pos, 0);
			const gchar *str;
			gchar *new_str = NULL;
			gint this_length = 1;
			gboolean whitespace = isspace ((gint) text[i]);

			str = index >= 0 ? e_select_names_model_get_string (source, index) : NULL;
			if (str) {
				if (pos <= start_pos) {
					if (whitespace) {
						/* swallow leading whitespace */
						this_length = 0;
					} else {
						/* Adjust for our "magic white space" */
						new_str = g_strdup_printf("%c%s%s", text[i], pos < start_pos ? " " : "", str);
						if (pos < start_pos)
							++this_length;
					}
				} else {
					new_str = g_strdup_printf ("%.*s%c%s", offset, str, text[i], str + offset);
				}
			} else {
				if (whitespace) {
					/* swallow leading whitespace */
					this_length = 0;
				} else {
					new_str = g_strdup_printf ("%c", text[i]);
				}
			}

			if (new_str) {

				EDestination *dest = e_destination_new ();
				e_destination_set_string (dest, new_str);

				e_select_names_model_replace (source, index, dest);

				if (this_length > 0) {
					repos.model = model;
					repos.pos = pos;
					repos.len = this_length;
					e_text_model_reposition (model, e_repos_insert_shift, &repos);

					pos += this_length;
				}

				g_free (new_str);
			}
		}
	}

	dump_model (E_SELECT_NAMES_TEXT_MODEL (model));

	gtk_signal_handler_unblock (GTK_OBJECT (source), E_SELECT_NAMES_TEXT_MODEL (model)->source_resize_id);
}


static void
e_select_names_text_model_delete (ETextModel *model, gint pos, gint length)
{
	ESelectNamesModel *source = E_SELECT_NAMES_TEXT_MODEL (model)->source;
	gint index, start_pos, text_len, offset;
		
	if (out) {
		const gchar *str = e_select_names_model_get_textification (source);
		gint i, len;

		fprintf (out, ">> delete %d at pos %d\n", length, pos);

		len = strlen (str);
		for (i=0; i<pos && i<len; ++i)
			fprintf (out, "%c", str[i]);
		fprintf (out, "[");
		for (i=pos; i<pos+length && i<len; ++i)
			fprintf (out, "%c", str[i]);
		fprintf (out, "]");
		for (i=pos+length; i<len; ++i)
			fprintf (out, "%c", str[i]);
		fprintf (out, "\n");
	}

	if (length < 0)
		return;

	e_select_names_model_text_pos (source, pos, &index, &start_pos, &text_len);

	if (out) 
		fprintf (out, "index=%d, start_pos=%d, text_len=%d\n", index, start_pos, text_len);

	/* We want to control all cursor motions ourselves, rather than taking hints
	   from the ESelectNamesModel. */
	gtk_signal_handler_block (GTK_OBJECT (source), E_SELECT_NAMES_TEXT_MODEL (model)->source_resize_id);

	/* First, we handle a few tricky cases. */

	if (pos < start_pos) {
		EReposAbsolute repos;

		repos.model = model;
		repos.pos = pos;
		e_text_model_reposition (model, e_repos_absolute, &repos);

		length -= start_pos - pos;

		if (length > 0)
			e_select_names_text_model_delete (model, start_pos, length);
		goto finished;
	}

	if (pos == start_pos + text_len) {
		/* We are positioned right at the end of an entry, possibly right in front of a comma. */

		if (index+1 < e_select_names_model_count (source)) {
			EReposDeleteShift repos;
			EDestination *new_dest;
			const gchar *str1 = e_select_names_model_get_string (source, index);
			const gchar *str2 = e_select_names_model_get_string (source, index+1);
			gchar *new_str;

			while (str1 && *str1 && isspace ((gint) *str1))
				++str1;
			while (str2 && *str2 && isspace ((gint) *str2))
				++str2;
			
			if (str1 && str2)
				new_str = g_strdup_printf ("%s %s", str1, str2);
			else if (str1)
				new_str = g_strdup (str1);
			else if (str2)
				new_str = g_strdup (str2);
			else
				new_str = g_strdup ("");

			if (out)
				fprintf (out, "joining \"%s\" and \"%s\" to \"%s\"\n", str1, str2, new_str);

			e_select_names_model_delete (source, index+1);

			new_dest = e_destination_new ();
			e_destination_set_string (new_dest, new_str);
			e_select_names_model_replace (source, index, new_dest);
			g_free (new_str);

			repos.model = model;
			repos.pos = pos;
			repos.len = SEPLEN - 1;

			e_text_model_reposition (model, e_repos_delete_shift, &repos);

			if (length > 1)
				e_select_names_text_model_delete (model, pos, length-1);
		} else {
			/* If we are at the end of the last entry (which we must be if we end up in this block),
			   we can just do nothing.  So this else-block is here just to give us a place to
			   put this comment. */
		}

		goto finished;
	}
	
	if (pos + length > start_pos + text_len) {
		/* Uh oh... our changes straddle two objects. */

		if (pos == start_pos) { /* Delete the whole thing */
			EReposDeleteShift repos;
		
			e_select_names_model_delete (source, index);

			if (out) 
				fprintf (out, "deleted all of %d\n", index);

			repos.model = model;
			repos.pos = pos;
			repos.len = text_len + SEPLEN;
		
			e_text_model_reposition (model, e_repos_delete_shift, &repos);

			length -= text_len + SEPLEN;
			if (length > 0)
				e_select_names_text_model_delete (model, pos, length);

		} else {
			/* Delete right up to the end, and then call e_select_names_text_model_delete again
			   to finish the job. */
			gint len1, len2;
			
			len1 = text_len - (pos - start_pos);
			len2 = length - len1;

			if (out) 
				fprintf (out, "two-stage delete: %d, %d\n", len1, len2);


			e_select_names_text_model_delete (model, pos, len1);
			e_select_names_text_model_delete (model, pos, len2);
		}

		goto finished;
	}

	/* Our changes are confined to just one entry. */
	if (length > 0) {
		const gchar *str;
		gchar *new_str;
		
		offset = pos - start_pos;
		
		str = e_select_names_model_get_string (source, index);
		new_str = str ? g_strdup_printf ("%.*s%s", offset, str, str + offset + length) : NULL;
		
		if (new_str) {
			EReposDeleteShift repos;
			EDestination *dest;

			dest = e_destination_new ();
			e_destination_set_string (dest, new_str);
			e_select_names_model_replace (source, index, dest);
			
			if (out)
				fprintf (out, "new_str: \"%s\"\n", new_str);

			g_free (new_str);
			
			repos.model = model;
			repos.pos = pos;
			repos.len = length;
			
			e_text_model_reposition (model, e_repos_delete_shift, &repos);
			
		} else {
			EReposDeleteShift repos;
			
			e_select_names_model_delete (source, index);
			
			if (out)
				fprintf (out, "deleted %d\n", index);

			
			repos.model = model;
			repos.pos = pos;
			repos.len = SEPLEN;
			
			e_text_model_reposition (model, e_repos_delete_shift, &repos);
		}
	}

 finished:
	gtk_signal_handler_unblock (GTK_OBJECT (source), E_SELECT_NAMES_TEXT_MODEL (model)->source_resize_id);
	dump_model (E_SELECT_NAMES_TEXT_MODEL (model));
}

static gint
e_select_names_text_model_obj_count (ETextModel *model)
{
	ESelectNamesModel *source = E_SELECT_NAMES_TEXT_MODEL (model)->source;
	gint i, count;

	count = i = e_select_names_model_count (source);
	while (i > 0) {
		const EDestination *dest;
		--i;
		dest = e_select_names_model_get_destination (source, i);
		if (e_destination_get_card (dest) == NULL)
			--count;
	}

	return count;
}

static gint
nth_obj_index (ESelectNamesModel *source, gint n)
{
	gint i, N;

	i = 0;
	N = e_select_names_model_count (source);
	
	do {
		const EDestination *dest = e_select_names_model_get_destination (source, i);
		if (e_destination_get_card (dest))
			--n;
		++i;
	} while (n >= 0 && i < N);

	if (i <= N)
		--i;
	else
		i = -1;

	return i;
}

static const gchar *
e_select_names_text_model_get_nth_obj (ETextModel *model, gint n, gint *len)
{
	ESelectNamesModel *source = E_SELECT_NAMES_TEXT_MODEL (model)->source;
	const gchar *txt;
	gint i, pos;

	i = nth_obj_index (source, n);
	if (i < 0)
		return NULL;
	
	e_select_names_model_name_pos (source, i, &pos,  len);
	if (pos < 0)
		return NULL;
	
	txt = e_select_names_model_get_textification (source);
	return txt + pos;
}

static void
e_select_names_text_model_activate_obj (ETextModel *model, gint n)
{
	ESelectNamesModel *source = E_SELECT_NAMES_TEXT_MODEL (model)->source;
	EContactEditor *contact_editor;
	ECard *card;
	gint i;

	i = nth_obj_index (source, n);
	g_return_if_fail (i >= 0);

	card = e_select_names_model_get_card (source, i);
	g_return_if_fail (card);
	
	/* present read-only contact editor when someone double clicks from here */
	contact_editor = e_contact_editor_new ((ECard *) card, FALSE, NULL, TRUE);
	e_contact_editor_raise (contact_editor);
}



