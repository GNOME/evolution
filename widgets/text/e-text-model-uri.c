/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* ETextModelURI - A Text Model w/ clickable URIs
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Jon Trowbridge <trow@gnu.org>
 *
 */

#include <config.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include "e-text-model-uri.h"

static void e_text_model_uri_class_init (ETextModelURIClass *class);
static void e_text_model_uri_init (ETextModelURI *model);
static void e_text_model_uri_destroy (GtkObject *object);

static void objectify_uris (ETextModelURI *model, gint position);

static void e_text_model_uri_set_text (ETextModel *model, gchar *text);
static void e_text_model_uri_insert (ETextModel *model, gint position, gchar *text);
static void e_text_model_uri_insert_length (ETextModel *model, gint position, gchar *text, gint length);
static void e_text_model_uri_delete (ETextModel *model, gint position, gint length);

static const gchar *e_text_model_uri_get_nth_object (ETextModel *model, gint);
static void e_text_model_uri_activate_nth_object (ETextModel *model, gint);

static GtkObject *parent_class;

GtkType
e_text_model_uri_get_type (void)
{
	static GtkType model_uri_type = 0;
	
	if (!model_uri_type) {
		GtkTypeInfo model_uri_info = {
			"ETextModelURI",
			sizeof (ETextModelURI),
			sizeof (ETextModelURIClass),
			(GtkClassInitFunc) e_text_model_uri_class_init,
			(GtkObjectInitFunc) e_text_model_uri_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};
		
		model_uri_type = gtk_type_unique (e_text_model_get_type (), &model_uri_info);
	}
	
	return model_uri_type;
}

static void
e_text_model_uri_class_init (ETextModelURIClass *klass)
{
	GtkObjectClass *object_class;
	ETextModelClass *model_class;

	object_class = (GtkObjectClass *) klass;
	model_class = E_TEXT_MODEL_CLASS (klass);

	parent_class = gtk_type_class (e_text_model_get_type ());

	object_class->destroy = e_text_model_uri_destroy;

	model_class->set_text = e_text_model_uri_set_text;
	model_class->insert = e_text_model_uri_insert;
	model_class->insert_length = e_text_model_uri_insert_length;
	model_class->delete = e_text_model_uri_delete;

	model_class->get_nth_obj = e_text_model_uri_get_nth_object;
	model_class->activate_nth_obj = e_text_model_uri_activate_nth_object;
}

static void
e_text_model_uri_init (ETextModelURI *model)
{

}

static void
e_text_model_uri_destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);

}

/* URL regexps taken from gnome-terminal */
static const gchar *url_regexs[3] = {
	"(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp))[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
	"(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp))[-A-Za-z0-9\\.]+[-A-Za-z0-9](:[0-9]*)?",
	"mailto:[A-Za-z0-9_]+@[-A-Za-z0-9_]+\\.[-A-Za-z0-9\\.]+[-A-Za-z0-9]"
};

static void
objectify_uris (ETextModelURI *model_uri, gint position)
{
	static gboolean initialized = FALSE;
	static regex_t re_full, re_part, re_mail;

	ETextModel *model = E_TEXT_MODEL (model_uri);
	regmatch_t match;
	GList *uris = NULL, *iter;
	gint i, j, offset, last, len, objN;
	gchar *in_str, *new_str;
	gint new_position = position, pos_adjust=0;

	if (model->text == NULL)
		return;

	if (!initialized) {

		if (regcomp (&re_full, url_regexs[0], REG_EXTENDED))
			g_error ("Regex compile failed: %s", url_regexs[0]);
		if (regcomp (&re_part, url_regexs[1], REG_EXTENDED))
			g_error ("Regex compile failed: %s", url_regexs[1]);
		if (regcomp (&re_mail, url_regexs[2], REG_EXTENDED))
			g_error ("Regex compile failed: %s", url_regexs[2]);


		initialized = TRUE;
	}

	/*** Expand objects in string, keeping track of position shifts ***/

	objN = e_text_model_object_count (model);
	if (objN == 0)
		in_str = g_strdup (model->text);
	else {
		gchar *src, *dest;

		/* Calculate length of expanded string. */
		len = strlen (model->text) - objN;
		for (i=0; i<objN; ++i)
			len += strlen (e_text_model_get_nth_object (model, i));

		in_str = g_new0 (gchar, len+1);

		src = model->text;
		dest = in_str;
		i = 0; /* object numbers */
		j = 0; /* string position */
		while (*src) {
			if (*src == '\1') {
				const gchar *src_obj;

				src_obj = e_text_model_get_nth_object (model, i);

				if (j < position) 
					new_position += strlen (src_obj)-1;

				if (src_obj) {
					while (*src_obj) {
						*dest = *src_obj;
						++dest;
						++src_obj;
					}
				}

				++src;
				++i;
				++j;

			} else {

				*dest = *src;
				++src;
				++dest;
				++j;
			}
		}
	}

	len = strlen (in_str);
	new_str = g_new0 (gchar, len+1);

	offset = 0;
	j = 0;
	last = 0;
	while (offset < len
	       && (regexec (&re_full, in_str+offset, 1, &match, 0) == 0
		   || regexec (&re_part, in_str+offset, 1, &match, 0) == 0
		   || regexec (&re_mail, in_str+offset, 1, &match, 0) == 0)) {

		gchar *uri_txt;

		if (offset+match.rm_so <= new_position
		    && new_position <= offset+match.rm_eo) {
			
			/* We don't do transformations that straddle our cursor. */
			new_str[j] = in_str[offset];
			++j;
			++offset;

		} else {
	
			for (i=offset; i<offset+match.rm_so; ++i) {
				new_str[j] = in_str[i];
				++j;
			}
			
			new_str[j] = '\1';
			++j;
			
			uri_txt = g_strndup (in_str+offset+match.rm_so,
					     match.rm_eo - match.rm_so);
			uris = g_list_append (uris, uri_txt);
			
			if (offset+match.rm_so < new_position)
				pos_adjust += match.rm_eo - match.rm_so - 1;
			
			offset += match.rm_eo;
		}
	}
	new_position -= pos_adjust;

	/* Copy the last bit over. */
	while (offset < len) {
		new_str[j] = in_str[offset];
		++j;
		++offset;
	}

#if 0
	for (i=0; i<strlen(new_str); ++i) {
		if (i == new_position)
			putchar ('#');
		if (new_str[i] == '\1')
			g_print ("[\1]");
		else
			putchar (new_str[i]);
	}
	putchar ('\n');
#endif

	for (iter = model_uri->uris; iter != NULL; iter = g_list_next (iter))
		g_free (iter->data);
	g_list_free (model_uri->uris);
	model_uri->uris = uris;
			
	g_free (in_str);

	if (E_TEXT_MODEL_CLASS(parent_class)->set_text)
		E_TEXT_MODEL_CLASS(parent_class)->set_text (model, new_str);

	g_free (new_str);

	
	if (new_position != position) 
		e_text_model_suggest_position (model, new_position);
}

static void
e_text_model_uri_set_text (ETextModel *model, gchar *text)
{
	if (E_TEXT_MODEL_CLASS(parent_class)->set_text)
		E_TEXT_MODEL_CLASS(parent_class)->set_text (model, text);

	objectify_uris (E_TEXT_MODEL_URI (model), 0);
}

static void
e_text_model_uri_insert (ETextModel *model, gint position, gchar *text)
{
	if (E_TEXT_MODEL_CLASS(parent_class)->insert)
		E_TEXT_MODEL_CLASS(parent_class)->insert (model, position, text);

	objectify_uris (E_TEXT_MODEL_URI (model), position + strlen (text));
}

static void
e_text_model_uri_insert_length (ETextModel *model, gint position, gchar *text, gint length)
{

	if (E_TEXT_MODEL_CLASS(parent_class)->insert_length)
		E_TEXT_MODEL_CLASS(parent_class)->insert_length (model, position, text, length);

	objectify_uris (E_TEXT_MODEL_URI (model), position + length);
}

static void
e_text_model_uri_delete (ETextModel *model, gint position, gint length)
{
	if (E_TEXT_MODEL_CLASS(parent_class)->delete)
		E_TEXT_MODEL_CLASS(parent_class)->delete (model, position, length);

	objectify_uris (E_TEXT_MODEL_URI (model), position);
}

static const gchar *
e_text_model_uri_get_nth_object (ETextModel *model, gint i)
{
	return (const gchar *) g_list_nth_data (E_TEXT_MODEL_URI (model)->uris, i);
}

static void
e_text_model_uri_activate_nth_object (ETextModel *model, gint i)
{
	const gchar *obj_str;

	obj_str = e_text_model_get_nth_object (model, i);
	gnome_url_show (obj_str);
}

ETextModel *
e_text_model_uri_new (void)
{
	return E_TEXT_MODEL (gtk_type_new (e_text_model_uri_get_type ()));
}


/* $Id$ */
