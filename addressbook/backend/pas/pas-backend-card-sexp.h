/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * pas-backend-card-sexp.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#ifndef __PAS_BACKEND_CARD_SEXP_H__
#define __PAS_BACKEND_CARD_SEXP_H__

#include <glib.h>
#include <glib-object.h>
#include <ebook/e-contact.h>
#include <pas/pas-types.h>

#define PAS_TYPE_BACKEND_CARD_SEXP        (pas_backend_card_sexp_get_type ())
#define PAS_BACKEND_CARD_SEXP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BACKEND_CARD_SEXP, PASBackendCardSExp))
#define PAS_BACKEND_CARD_SEXP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendCardSExpClass))
#define PAS_IS_BACKEND_CARD_SEXP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BACKEND_CARD_SEXP))
#define PAS_IS_BACKEND_CARD_SEXP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BACKEND_CARD_SEXP))
#define PAS_BACKEND_CARD_SEXP_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BACKEND_CARD_SEXP, PASBackendCardSExpClass))

typedef struct _PASBackendCardSExpPrivate PASBackendCardSExpPrivate;

struct _PASBackendCardSExp {
	GObject parent_object;
	PASBackendCardSExpPrivate *priv;
};

struct _PASBackendCardSExpClass {
	GObjectClass parent_class;
};

PASBackendCardSExp *pas_backend_card_sexp_new      (const char *text);
GType               pas_backend_card_sexp_get_type (void);

gboolean            pas_backend_card_sexp_match_vcard (PASBackendCardSExp *sexp, const char *vcard);
gboolean            pas_backend_card_sexp_match_contact (PASBackendCardSExp *sexp, EContact *contact);

#endif /* __PAS_BACKEND_CARD_SEXP_H__ */
