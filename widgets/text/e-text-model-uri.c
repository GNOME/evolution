/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* ETextModelURI - A Text Model w/ clickable URIs
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Jon Trowbridge <trow@gnu.org>
 *
 */

#include <config.h>
#include <ctype.h>
#include "e-text-model-uri.h"

static void e_text_model_uri_class_init (ETextModelURIClass *class);
static void e_text_model_uri_init (ETextModelURI *model);
static void e_text_model_uri_destroy (GtkObject *object);

static void objectify_uris (ETextModelURI *model);

static void e_text_model_uri_set_text (ETextModel *model, gchar *text);
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

static gchar *
extract_uri (gchar **in_str)
{
	gchar *s = *in_str;
	if (strncmp (s, "http://", 7) == 0) {
		gint periods=0;
		gchar *uri;

		s += 7;

		while (*s && (isalnum((gint) *s) || (*s == '.' && periods < 2))) {
			if (*s == '.')
				++periods;
			++s;
		}

		uri = g_strndup (*in_str, s - *in_str);
		*in_str = s;
		return uri;

	} else {
		*in_str = s+1;
		return NULL;
	}
}

static void
objectify_uris (ETextModelURI *model_uri)
{
	ETextModel *model = E_TEXT_MODEL (model_uri);
	gchar *new_text;
	gchar *src, *dest;
	GList *uris = NULL;

	if (model->text == NULL)
		return;

	new_text = g_new0 (gchar, strlen (model->text)+1);

	src = model->text;
	dest = new_text;

	while (*src) {
		gchar *uri_str;
		gchar *next = src;
		if ( (uri_str = extract_uri (&next)) ) {
			uris = g_list_append (uris, uri_str);
			*dest = '\1';
		} else {
			*dest = *src;
		}
		++dest;
		src = next;
	}

	g_free (model->text);
	model->text = new_text;

	/* Leaking old list */
	model_uri->uris = uris;
}

static void
e_text_model_uri_set_text (ETextModel *model, gchar *text)
{
	if (E_TEXT_MODEL_CLASS(parent_class)->set_text)
		E_TEXT_MODEL_CLASS(parent_class)->set_text (model, text);

	objectify_uris (E_TEXT_MODEL_URI (model));
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
