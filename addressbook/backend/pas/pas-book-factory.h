/*
 * Copyright 2000, Helix Code, Inc.
 */

#include <bonobo/gnome-object.h>
#include <libgnome/gnome-defs.h>

#include <pas-backend.h>

#ifndef __PAS_BOOK_FACTORY_H__
#define __PAS_BOOK_FACTORY_H__

BEGIN_GNOME_DECLS

typedef struct _PASBookFactoryPrivate PASBookFactoryPrivate;

typedef struct {
	GnomeObject            parent_object;
	PASBookFactoryPrivate *priv;
} PASBookFactory;

typedef struct {
	GnomeObjectClass parent_class;
} PASBookFactoryClass;

PASBookFactory *pas_book_factory_new              (void);

void            pas_book_factory_register_backend (PASBookFactory               *factory,
						   const char                   *proto,
						   PASBackendFactoryFn           backend_factory);

void            pas_book_factory_activate         (PASBookFactory               *factory);

GtkType         pas_book_factory_get_type (void);

#define PAS_BOOK_FACTORY_TYPE        (pas_book_factory_get_type ())
#define PAS_BOOK_FACTORY(o)          (GTK_CHECK_CAST ((o), PAS_BOOK_FACTORY_TYPE, PASBookFactory))
#define PAS_BOOK_FACTORY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_BOOK_FACTORY_TYPE, PASBookFactoryClass))
#define PAS_IS_BOOK_FACTORY(o)       (GTK_CHECK_TYPE ((o), PAS_BOOK_FACTORY_TYPE))
#define PAS_IS_BOOK_FACTORY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_BOOK_FACTORY_TYPE))

END_GNOME_DECLS

#endif /* ! __PAS_BOOK_FACTORY_H__ */
