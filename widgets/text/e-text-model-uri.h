/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* ETextModelURI - A Text Model w/ clickable URIs
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Jon Trowbridge <trow@gnu.org>
 *
 */

#ifndef E_TEXT_MODEL_URI_H
#define E_TEXT_MODEL_URI_H

#include <gnome.h>
#include <gal/e-text/e-text-model.h>

BEGIN_GNOME_DECLS

#define E_TYPE_TEXT_MODEL_URI            (e_text_model_get_type ())
#define E_TEXT_MODEL_URI(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_TEXT_MODEL_URI, ETextModelURI))
#define E_TEXT_MODEL_URI_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TEXT_MODEL_URI, ETextModelURIClass))
#define E_IS_TEXT_MODEL_URI(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_TEXT_MODEL_URI))
#define E_IS_TEXT_MODEL_URI_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_TEXT_MODEL_URI))

typedef struct _ETextModelURI ETextModelURI;
typedef struct _ETextModelURIClass ETextModelURIClass;

struct _ETextModelURI {
  ETextModel item;
  GList *uris;
};

struct _ETextModelURIClass {
  ETextModelClass parent_class;
};

GtkType e_text_model_uri_get_type (void);
ETextModel *e_text_model_uri_new (void);

END_GNOME_DECLS

#endif
