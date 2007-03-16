/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text-model-uri.h - a text model w/ clickable URIs
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

#ifndef E_TEXT_MODEL_URI_H
#define E_TEXT_MODEL_URI_H

#include <text/e-text-model.h>

G_BEGIN_DECLS

#define E_TYPE_TEXT_MODEL_URI            (e_text_model_uri_get_type ())
#define E_TEXT_MODEL_URI(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TEXT_MODEL_URI, ETextModelURI))
#define E_TEXT_MODEL_URI_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TEXT_MODEL_URI, ETextModelURIClass))
#define E_IS_TEXT_MODEL_URI(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TEXT_MODEL_URI))
#define E_IS_TEXT_MODEL_URI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_TEXT_MODEL_URI))

typedef struct _ETextModelURI ETextModelURI;
typedef struct _ETextModelURIClass ETextModelURIClass;

struct _ETextModelURI {
	ETextModel item;
	GList *uris;
	
	guint objectify_idle;
};

struct _ETextModelURIClass {
  ETextModelClass parent_class;
};

GtkType e_text_model_uri_get_type (void);
ETextModel *e_text_model_uri_new (void);

G_END_DECLS

#endif
