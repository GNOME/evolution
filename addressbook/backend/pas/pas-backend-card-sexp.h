/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
