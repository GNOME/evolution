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

#include <gtk/gtk.h>

typedef struct _PASBackendCardSExpPrivate PASBackendCardSExpPrivate;

typedef struct {
	GtkObject parent_object;
	PASBackendCardSExpPrivate *priv;
} PASBackendCardSExp;

typedef struct {
	GtkObjectClass parent_class;
} PASBackendCardSExpClass;

PASBackendCardSExp *pas_backend_card_sexp_new      (const char *text);
GtkType             pas_backend_card_sexp_get_type (void);

gboolean            pas_backend_card_sexp_match_vcard (PASBackendCardSExp *sexp, const char *vcard);

#define PAS_BACKEND_CARD_SEXP_TYPE        (pas_backend_card_sexp_get_type ())
#define PAS_BACKEND_CARD_SEXP(o)          (GTK_CHECK_CAST ((o), PAS_BACKEND_CARD_SEXP_TYPE, PASBackendCardSExp))
#define PAS_BACKEND_CARD_SEXP_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendCardSExpClass))
#define PAS_IS_BACKEND_CARD_SEXP(o)       (GTK_CHECK_TYPE ((o), PAS_BACKEND_CARD_SEXP_TYPE))
#define PAS_IS_BACKEND_CARD_SEXP_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_BACKEND_CARD_SEXP_TYPE))

#endif /* __PAS_BACKEND_CARD_SEXP_H__ */
