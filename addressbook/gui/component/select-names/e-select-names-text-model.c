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
#include <libgnome/gnome-i18n.h>

#include <addressbook/gui/contact-editor/e-contact-editor.h>
#include "e-select-names-text-model.h"
#include "eab-gui-util.h"

static FILE *out = NULL; /* stream for debugging spew */

/* Object argument IDs */
enum {
	PROP_0,
	PROP_SOURCE,
};

static void e_select_names_text_model_class_init (ESelectNamesTextModelClass *klass);
static void e_select_names_text_model_init       (ESelectNamesTextModel *model);
static void e_select_names_text_model_dispose      (GObject *object);
static void e_select_names_text_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_select_names_text_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void e_select_names_text_model_set_source (ESelectNamesTextModel *model, ESelectNamesModel *source);

static const gchar *e_select_names_text_model_get_text      (ETextModel *model);
static void         e_select_names_text_model_set_text      (ETextModel *model, const gchar *text);
static void         e_select_names_text_model_insert        (ETextModel *model, gint position, const gchar *text);
static void         e_select_names_text_model_insert_length (ETextModel *model, gint position, const gchar *text, gint length);
static void         e_select_names_text_model_delete        (ETextModel *model, gint position, gint length);

static gint         e_select_names_text_model_obj_count   (ETextModel *model);
static const gchar *e_select_names_text_model_get_nth_obj (ETextModel *model, gint n, gint *len);
static void         e_select_names_text_model_activate_obj (ETextModel *model, gint n);


static ETextModelClass *parent_class;
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
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ESelectNamesTextModelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_select_names_text_model_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESelectNamesTextModel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_select_names_text_model_init,
		};

		type = g_type_register_static (PARENT_TYPE, "ESelectNamesTextModel", &info, 0);
	}

	return type;
}

static void
e_select_names_text_model_class_init (ESelectNamesTextModelClass *klass)
{
	GObjectClass *object_class;
	ETextModelClass *text_model_class;

	object_class = G_OBJECT_CLASS(klass);
	text_model_class = E_TEXT_MODEL_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_select_names_text_model_dispose;
	object_class->get_property = e_select_names_text_model_get_property;
	object_class->set_property = e_select_names_text_model_set_property;

	g_object_class_install_property (object_class, PROP_SOURCE, 
					 g_param_spec_object ("source",
							      _("Source"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_SELECT_NAMES_MODEL,
							      G_PARAM_READWRITE));

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
			 e_select_names_model_get_contact (model, i) ? "<contact>" : "");
	fprintf (out, "\n");
}

static void
e_select_names_text_model_init (ESelectNamesTextModel *model)
{
	const gchar *default_sep;

	model->last_magic_comma_pos = -1;

	if (getenv ("EVOLUTION_DISABLE_MAGIC_COMMA"))
		default_sep = ",";
	else
		default_sep = ", ";

	e_select_names_text_model_set_separator (model, default_sep);
}

static void
e_select_names_text_model_dispose (GObject *object)
{
	ESelectNamesTextModel *model;
	
	model = E_SELECT_NAMES_TEXT_MODEL (object);

	if (model->text) {
		g_free (model->text);
		model->text = NULL;
	}
	if (model->sep) {
		g_free (model->sep);
		model->sep = NULL;
	}
	
	e_select_names_text_model_set_source (model, NULL);
	
	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
e_select_names_text_model_set_property (GObject *object, guint prop_id,
					 const GValue *value, GParamSpec *pspec)
{
	ESelectNamesTextModel *model;
	
	model = E_SELECT_NAMES_TEXT_MODEL (object);

	switch (prop_id) {
	case PROP_SOURCE:
		e_select_names_text_model_set_source(model, E_SELECT_NAMES_MODEL (g_value_get_object(value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_select_names_text_model_get_property (GObject *object, guint prop_id,
					 GValue *value, GParamSpec *pspec)
{
	ESelectNamesTextModel *model;

	model = E_SELECT_NAMES_TEXT_MODEL (object);

	switch (prop_id) {
	case PROP_SOURCE:
		g_value_set_object (value, model->source);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
resize_cb (ESelectNamesModel *source, gint index, gint old_len, gint new_len, ETextModel *model)
{
	EReposDeleteShift repos_del;
	EReposInsertShift repos_ins;
	gint pos;
	gint seplen = E_SELECT_NAMES_TEXT_MODEL (model)->seplen;

	e_select_names_model_name_pos (source, seplen, index, &pos, NULL);

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
changed_cb (ESelectNamesModel *source, ETextModel *model)
{
	ESelectNamesTextModel *text_model = E_SELECT_NAMES_TEXT_MODEL (model);
	
	g_free (text_model->text);
	text_model->text = NULL;

	e_text_model_changed (model);
}


static void
e_select_names_text_model_set_source (ESelectNamesTextModel *model,
				      ESelectNamesModel     *source)
{
	if (source == model->source)
		return;
	
	if (model->source) {
		g_signal_handler_disconnect (model->source, model->source_changed_id);
		g_signal_handler_disconnect (model->source, model->source_resize_id);
		g_object_unref (model->source);
	}

	model->source = source;

	if (model->source) {
		g_object_ref (model->source);
		model->source_changed_id = g_signal_connect (model->source,
							     "changed",
							     G_CALLBACK (changed_cb),
							     model);
		model->source_resize_id = g_signal_connect (model->source,
							    "resized",
							    G_CALLBACK (resize_cb),
							    model);
	}
}

ETextModel *
e_select_names_text_model_new (ESelectNamesModel *source)
{
	ETextModel *model = g_object_new (E_TYPE_SELECT_NAMES_TEXT_MODEL, NULL);
	e_select_names_text_model_set_source (E_SELECT_NAMES_TEXT_MODEL (model), source);
	return model;
}

void
e_select_names_text_model_set_separator (ESelectNamesTextModel *model, const char *sep)
{
	g_return_if_fail (E_IS_SELECT_NAMES_TEXT_MODEL (model));
	g_return_if_fail (sep && *sep);

	g_free (model->sep);
	model->sep = g_strdup (sep);
	model->seplen = g_utf8_strlen (sep, -1);
}

static const gchar *
e_select_names_text_model_get_text (ETextModel *model)
{
	ESelectNamesTextModel *snm = E_SELECT_NAMES_TEXT_MODEL(model);

	if (snm == NULL)
		return "";
	else if (snm->text == NULL)
		snm->text = e_select_names_model_get_textification (snm->source, snm->sep);

	return snm->text;
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
	e_select_names_text_model_insert_length (model, position, text, g_utf8_strlen (text, -1));
}

static void
e_select_names_text_model_insert_length (ETextModel *model, gint pos, const gchar *text, gint length)
{
	ESelectNamesTextModel *text_model = E_SELECT_NAMES_TEXT_MODEL (model);
	ESelectNamesModel *source = text_model->source;
	const char *t;
	gchar *tmp;

	if (out) {
		tmp = g_strndup (text, length);
		fprintf (out, ">> insert  \"%s\" (len=%d) at %d\n", tmp, length, pos);
		g_free (tmp);
	}

	tmp = e_select_names_model_get_textification (source, text_model->sep);
	pos = CLAMP (pos, 0, g_utf8_strlen (tmp, -1));
	g_free (tmp);

	/* We want to control all cursor motions ourselves, rather than taking hints
	   from the ESelectNamesModel. */
	g_signal_handler_block (source, text_model->source_resize_id);

	/* We handle this one character at a time. */

	for (t = text; length >= 0; t = g_utf8_next_char (t), length--) {
		gint index, start_pos, text_len;
		gboolean inside_quote = FALSE;
		gunichar ut = g_utf8_get_char (t);

		if (ut == 0)
			break;

		text_model->last_magic_comma_pos = -1;

		if (out) 
			fprintf (out, "processing [%d]\n", ut);

		e_select_names_model_text_pos (source, text_model->seplen, pos, &index, &start_pos, &text_len);

		if (out) 
			fprintf (out, "index=%d start_pos=%d text_len=%d\n", index, start_pos, text_len);

		/* Is this a quoted or an unquoted separator we are dealing with? */
		if (ut == g_utf8_get_char(text_model->sep) && index >= 0) {
			const EDestination *dest = e_select_names_model_get_destination (source, index);
			if (dest) {
				const gchar *str = e_destination_get_textrep (dest, FALSE);
				int j;
				const char *jp;

				if (out)
					fprintf (out, "str=%s pos=%d\n", str, pos);

				for (jp = str, j = 0; j<pos-start_pos && *jp; jp = g_utf8_next_char (jp), ++j) {
					if (*jp == '"') {
						inside_quote = !inside_quote;
						if (out)
							fprintf (out, "flip to %d at %d\n", start_pos+j, inside_quote);
					}
				}
			}
			if (out)
				fprintf (out, inside_quote ? "inside quote\n" : "not inside quote\n");
		}


		if (ut == g_utf8_get_char (text_model->sep) && !inside_quote) {

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
					repos.len = text_model->seplen;
					e_text_model_reposition (model, e_repos_insert_shift, &repos);
					pos += text_model->seplen;
				} 

			} else {
				EReposInsertShift repos;
				gint offset = MAX (pos - start_pos, 0);
				const gchar *str = e_select_names_model_get_string (source, index);
				gchar *str1 = g_strndup (str, offset);
				gchar *str2 = g_strdup (str+offset);
				EDestination *d1 = e_destination_new (), *d2 = e_destination_new ();

				e_destination_set_raw (d1, str1);
				e_destination_set_raw (d2, str2);

				e_select_names_model_replace (source, index, d1);
				e_select_names_model_insert (source, index+1, d2);

				g_free (str1);
				g_free (str2);

				repos.model = model;
				repos.pos = pos;
				repos.len = text_model->seplen;
				e_text_model_reposition (model, e_repos_insert_shift, &repos);
				pos += text_model->seplen;
			}

			if (text_model->seplen > 1)
				text_model->last_magic_comma_pos = pos;

		} else {
			EReposInsertShift repos;
			gint offset = MAX (pos - start_pos, 0);
			const gchar *str;
			GString *new_str = g_string_new (NULL);
			gint this_length = 1;
			gboolean whitespace = g_unichar_isspace (ut);

			str = index >= 0 ? e_select_names_model_get_string (source, index) : NULL;
			if (str && *str) {
				if (pos <= start_pos) {
					if (whitespace) {
						/* swallow leading whitespace */
						this_length = 0;
					} else {
						/* Adjust for our "magic white space" */
						/* FIXME: This code does the wrong thing if seplen > 2 */
						g_string_append_unichar (new_str, ut);
						g_string_append (new_str, pos < start_pos ? " " : "");
						g_string_append (new_str, str);
						if (pos < start_pos)
							++this_length;
					}
				} else {
					const char *u;
					int n;
					for (u = str, n = 0; n < offset; u = g_utf8_next_char (u), n++) 
						g_string_append_unichar (new_str, g_utf8_get_char (u));
					g_string_append_unichar (new_str, ut);
					g_string_append (new_str, u);
				}
			} else {
				if (whitespace) {
					/* swallow leading whitespace */
					this_length = 0;
				} else {
					g_string_append_unichar (new_str, ut);
				}
			}

			if (new_str->len) {

				EDestination *dest;
				dest = e_destination_new ();
				e_destination_set_raw (dest, new_str->str);
				e_select_names_model_replace (source, index, dest);

				if (this_length > 0) {
					repos.model = model;
					repos.pos = pos;
					repos.len = this_length;
					e_text_model_reposition (model, e_repos_insert_shift, &repos);

					pos += this_length;
				}
			}
			g_string_free (new_str, TRUE);
		}
	}

	dump_model (text_model);

	g_signal_handler_unblock (source, text_model->source_resize_id);
}


static void
e_select_names_text_model_delete (ETextModel *model, gint pos, gint length)
{
	ESelectNamesTextModel *text_model = E_SELECT_NAMES_TEXT_MODEL (model);
	ESelectNamesModel *source = text_model->source;
	gint index, start_pos, text_len, offset;
		
	if (out) {
		const gchar *str = e_select_names_model_get_textification (source, text_model->sep);
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

	if (text_model->last_magic_comma_pos == pos+1 && length == 1) {
		pos -= text_model->seplen-1;
		if (pos >= 0)
			length = text_model->seplen;
		text_model->last_magic_comma_pos = -1;
	}

	e_select_names_model_text_pos (source, text_model->seplen, pos, &index, &start_pos, &text_len);

	if (out) 
		fprintf (out, "index=%d, start_pos=%d, text_len=%d\n", index, start_pos, text_len);

	/* We want to control all cursor motions ourselves, rather than taking hints
	   from the ESelectNamesModel. */
	g_signal_handler_block (source, E_SELECT_NAMES_TEXT_MODEL (model)->source_resize_id);

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
				new_str = g_strdup_printf ("%s%s%s", str1, text_model->sep+1, str2);
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
			e_destination_set_raw (new_dest, new_str);
			e_select_names_model_replace (source, index, new_dest);
			g_free (new_str);

			repos.model = model;
			repos.pos = pos;
			repos.len = text_model->seplen;

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
			repos.len = text_len + text_model->seplen;
		
			e_text_model_reposition (model, e_repos_delete_shift, &repos);

			length -= text_len + text_model->seplen;
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

		if (str) {
			const char *p;
			char *np;
			int i;
			EReposDeleteShift repos;
			EDestination *dest;

			new_str = g_new0 (char, strlen (str) * 6 + 1); /* worse case it can't be any longer than this */

			/* copy the region before the deletion */
			for (p = str, i = 0, np = new_str; i < offset; i++) {
				gunichar ch;

				ch = g_utf8_get_char (p);
				g_unichar_to_utf8 (ch, np);

				np = g_utf8_next_char (np);
				p = g_utf8_next_char (p);
			}

			/* skip the deleted segment */
			for (i = 0; i < length; i++)
				p = g_utf8_next_char (p);

			/* copy the region after the deletion */
			for (; *p; p = g_utf8_next_char (p)) {
				gunichar ch;

				ch = g_utf8_get_char (p);
				g_unichar_to_utf8 (ch, np);

				np = g_utf8_next_char (np);
			}

			dest = e_destination_new ();
			e_destination_set_raw (dest, new_str);
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
			repos.len = text_model->seplen;
			
			e_text_model_reposition (model, e_repos_delete_shift, &repos);
		}
	}

 finished:
	E_SELECT_NAMES_TEXT_MODEL (model)->last_magic_comma_pos = -1;
	g_signal_handler_unblock (source, E_SELECT_NAMES_TEXT_MODEL (model)->source_resize_id);
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
		if (e_destination_get_contact (dest) == NULL)
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
		if (e_destination_get_contact (dest))
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
	ESelectNamesTextModel *text_model = E_SELECT_NAMES_TEXT_MODEL (model);
	ESelectNamesModel *source = text_model->source;
	gint i, pos;

	i = nth_obj_index (source, n);
	if (i < 0)
		return NULL;
	
	e_select_names_model_name_pos (source, text_model->seplen, i, &pos,  len);
	if (pos < 0)
		return NULL;
	
	if (text_model->text == NULL)
		text_model->text = e_select_names_model_get_textification (source, text_model->sep);
	return g_utf8_offset_to_pointer (text_model->text, pos);
}

static void
e_select_names_text_model_activate_obj (ETextModel *model, gint n)
{
	ESelectNamesModel *source = E_SELECT_NAMES_TEXT_MODEL (model)->source;
	EContact *contact;
	EBook *book;
	gint i;

	i = nth_obj_index (source, n);
	g_return_if_fail (i >= 0);

	contact = e_select_names_model_get_contact (source, i);
	g_return_if_fail (contact != NULL);

	/* XXX unfortunately we don't have an e_contact_get_book call
	   anymore, so we can't query for the addressbook that the
	   contact came from.  however, since we're bringing up a
	   read-only contact editor, it really doesn't matter what we
	   show in the source option menu, does it? */
	book = e_book_new_system_addressbook (NULL);
	g_return_if_fail (book != NULL);

	/* present read-only contact editor when someone double clicks from here */
	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		EContactListEditor *ce;
		ce = eab_show_contact_list_editor (book, contact, FALSE, FALSE);
		eab_editor_raise (EAB_EDITOR (ce));
	}
	else {
		EContactEditor *ce;
		ce = eab_show_contact_editor (book, contact, FALSE, FALSE);
		eab_editor_raise (EAB_EDITOR (ce));
	}

	g_object_unref (book);
}



