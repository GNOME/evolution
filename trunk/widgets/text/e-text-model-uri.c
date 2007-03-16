/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text-model-uri.c - a text model w/ clickable URIs
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Jon Trowbridge <trow@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

#include <gtk/gtkmain.h>
#include <libgnome/gnome-url.h>

#include "e-util/e-util.h"

#include "e-text-model-uri.h"

static void e_text_model_uri_class_init (ETextModelURIClass *class);
static void e_text_model_uri_init (ETextModelURI *model);
static void e_text_model_uri_dispose (GObject *object);

static void objectify_uris (ETextModelURI *model);

static void e_text_model_uri_objectify (ETextModel *model);
static gint e_text_model_uri_validate_pos (ETextModel *model, gint pos);
static gint e_text_model_uri_get_obj_count (ETextModel *model);
static const gchar *e_text_model_uri_get_nth_object (ETextModel *model, gint i, gint *len);
static void e_text_model_uri_activate_nth_object (ETextModel *model, gint);

typedef struct _ObjInfo ObjInfo;
struct _ObjInfo {
	gint offset, len;
};

#define PARENT_TYPE E_TYPE_TEXT_MODEL
static GtkObject *parent_class;

E_MAKE_TYPE (e_text_model_uri,
	     "ETextModelURI",
	     ETextModelURI,
	     e_text_model_uri_class_init,
	     e_text_model_uri_init,
	     PARENT_TYPE)
	     
static void
e_text_model_uri_class_init (ETextModelURIClass *klass)
{
	GObjectClass *object_class;
	ETextModelClass *model_class;

	object_class = (GObjectClass *) klass;
	model_class = E_TEXT_MODEL_CLASS (klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = e_text_model_uri_dispose;

	model_class->object_activated = e_text_model_uri_activate_nth_object;

	model_class->objectify = e_text_model_uri_objectify;
	model_class->validate_pos = e_text_model_uri_validate_pos;
	model_class->obj_count = e_text_model_uri_get_obj_count;
	model_class->get_nth_obj = e_text_model_uri_get_nth_object;

}

static void
e_text_model_uri_init (ETextModelURI *model)
{

}

static void
e_text_model_uri_dispose (GObject *object)
{
	ETextModelURI *model_uri = E_TEXT_MODEL_URI (object);
	GList *iter;

	if (model_uri->objectify_idle) {
		gtk_idle_remove (model_uri->objectify_idle);
		model_uri->objectify_idle = 0;
	}

	for (iter = model_uri->uris; iter != NULL; iter = g_list_next (iter))
		g_free (iter->data);
	g_list_free (model_uri->uris);
	model_uri->uris = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);

}

static const gchar *uri_regex[] = {
	"(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp))[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
        "(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp))[-A-Za-z0-9\\.]+[-A-Za-z0-9](:[0-9]*)?",
        "mailto:[A-Za-z0-9_]+@[-A-Za-z0-9_]+\\.[-A-Za-z0-9\\.]+[-A-Za-z0-9]",
	NULL
};
static gint regex_count = 0;
static regex_t *regex_compiled = NULL;

static void
regex_init (void)
{
	gint i;

	if (regex_count != 0)
		return;

	while (uri_regex[regex_count]) ++regex_count;

	regex_compiled = g_new0 (regex_t, regex_count);
	
	for (i=0; i<regex_count; ++i) {
		if (regcomp (&regex_compiled[i], uri_regex[i], REG_EXTENDED))
			g_error ("Bad regex?: %s", uri_regex[i]);
	}
}


static void
objectify_uris (ETextModelURI *model_uri)
{
	static gboolean objectifying = FALSE;

	ETextModel *model = E_TEXT_MODEL (model_uri);
	const gchar *txt;
	GList *iter, *old_uris;
	gint offset, len;
	gboolean found_match;
	regmatch_t match;
	gboolean changed;

	if (objectifying)
		return;

	objectifying = TRUE;

	if (regex_count == 0)
		regex_init ();

	txt = e_text_model_get_text (model);
	len  = e_text_model_get_text_length (model);

	old_uris = model_uri->uris;
	model_uri->uris = NULL;

	if (txt) {
		offset = 0;
		found_match = TRUE;

		while (offset < len && found_match) {
			
			gint i, so=-1, eo=-1;

			found_match = FALSE;
			
			for (i=0; i<regex_count; ++i) {
				
				if (regexec (&regex_compiled[i], txt+offset, 1, &match, 0) == 0) {

					/* Take earliest match possible.  In case of a tie, take the
					   largest possible match. */
					if (!found_match
					    || match.rm_so < so
					    || (match.rm_so == so && match.rm_eo > eo)) {
						so = match.rm_so;
						eo = match.rm_eo;
					}
					found_match = TRUE;
				}
			}
			
			if (found_match) {
				
				ObjInfo *info = g_new0 (ObjInfo, 1);
				info->offset = offset + so;
				info->len = eo - so;

				model_uri->uris = g_list_append (model_uri->uris, info);
				
				offset += eo;
			}
		}
	}

	changed = (g_list_length (old_uris) != g_list_length (model_uri->uris));

	if (!changed) {
		/* Check that there is a 1-1 correspondence between object positions. */
		GList *jter;

		for (iter = model_uri->uris; iter != NULL && !changed; iter = g_list_next (iter)) {
			ObjInfo *info = (ObjInfo *) iter->data;
			found_match = FALSE;
			for (jter = old_uris; jter != NULL && !found_match; jter = g_list_next (jter)) {
				ObjInfo *jnfo = (ObjInfo *) jter->data;

				if (info->offset == jnfo->offset && info->len == jnfo->len)
					found_match = TRUE;
			}
			changed = !found_match;
		}
	}

	if (changed)
		e_text_model_changed (model);

	/* Free old uris */
	for (iter = old_uris; iter != NULL; iter = g_list_next (iter))
		g_free (iter->data);
	g_list_free (old_uris);

	objectifying = FALSE;
}

static gboolean
objectify_idle_cb (gpointer ptr)
{
	ETextModelURI *model_uri = E_TEXT_MODEL_URI (ptr);

	g_assert (model_uri->objectify_idle);
	objectify_uris (model_uri);
	model_uri->objectify_idle = 0;

	return FALSE;
}

static void
e_text_model_uri_objectify (ETextModel *model)
{
	ETextModelURI *model_uri = E_TEXT_MODEL_URI (model);

	if (model_uri->objectify_idle == 0)
		model_uri->objectify_idle = gtk_idle_add (objectify_idle_cb, model);

	if (E_TEXT_MODEL_CLASS(parent_class)->objectify)
		E_TEXT_MODEL_CLASS(parent_class)->objectify (model);
}

static void
objectify_idle_flush (ETextModelURI *model_uri)
{
	if (model_uri->objectify_idle) {
		gtk_idle_remove (model_uri->objectify_idle);
		model_uri->objectify_idle = 0;
		objectify_uris (model_uri);
	}
}

static gint
e_text_model_uri_validate_pos (ETextModel *model, gint pos)
{
	gint obj_num;

	/* Cause us to skip over objects */

	obj_num = e_text_model_get_object_at_offset (model, pos);
	if (obj_num != -1) {
		gint pos0, pos1, mp;
		e_text_model_get_nth_object_bounds (model, obj_num, &pos0, &pos1);
		mp = (pos0 + pos1)/2;
		if (pos0 < pos && pos < mp)
			pos = pos1;
		else if (mp <= pos && pos < pos1)
			pos = pos0;
	}

	

	if (E_TEXT_MODEL_CLASS (parent_class)->validate_pos)
		pos = E_TEXT_MODEL_CLASS (parent_class)->validate_pos (model, pos);

	return pos;
}

static gint
e_text_model_uri_get_obj_count (ETextModel *model)
{
	ETextModelURI *model_uri = E_TEXT_MODEL_URI (model);

	objectify_idle_flush (model_uri);

	return g_list_length (model_uri->uris);
}

static const gchar *
e_text_model_uri_get_nth_object (ETextModel *model, gint i, gint *len)
{
	ETextModelURI *model_uri = E_TEXT_MODEL_URI (model);
	ObjInfo *info;
	const gchar *txt;

	objectify_idle_flush (model_uri);
	
	txt = e_text_model_get_text (model);

	info = (ObjInfo *) g_list_nth_data (model_uri->uris, i);
	g_return_val_if_fail (info != NULL, NULL);


	if (len)
		*len = info->len;
	return txt + info->offset;
}

static void
e_text_model_uri_activate_nth_object (ETextModel *model, gint i)
{
	gchar *obj_str;

	objectify_idle_flush (E_TEXT_MODEL_URI (model));
	
	obj_str = e_text_model_strdup_nth_object (model, i);
	gnome_url_show (obj_str, NULL);
	g_free (obj_str);
}

ETextModel *
e_text_model_uri_new (void)
{
	return E_TEXT_MODEL (g_object_new (E_TYPE_TEXT_MODEL_URI, NULL));
}


/* $Id$ */
