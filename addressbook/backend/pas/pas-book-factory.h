/*
 * Copyright 2000, Ximian, Inc.
 */

#include <pas/pas-backend.h>
#include <bonobo/bonobo-object.h>

#ifndef __PAS_BOOK_FACTORY_H__
#define __PAS_BOOK_FACTORY_H__

G_BEGIN_DECLS

#define PAS_TYPE_BOOK_FACTORY        (pas_book_factory_get_type ())
#define PAS_BOOK_FACTORY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BOOK_FACTORY, PASBookFactory))
#define PAS_BOOK_FACTORY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_TYPE_BOOK_FACTORY, PASBookFactoryClass))
#define PAS_IS_BOOK_FACTORY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BOOK_FACTORY))
#define PAS_IS_BOOK_FACTORY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BOOK_FACTORY))
#define PAS_BOOK_FACTORY_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BOOK_FACTORY, PASBookFactoryClass))

typedef struct _PASBookFactoryPrivate PASBookFactoryPrivate;

typedef struct {
	BonoboObject            parent_object;
	PASBookFactoryPrivate *priv;
} PASBookFactory;

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Addressbook_BookFactory__epv epv;

	/* Notification signals */

	void (* last_book_gone) (PASBookFactory *factory);
} PASBookFactoryClass;

PASBookFactory *pas_book_factory_new              (void);

void            pas_book_factory_register_backend (PASBookFactory               *factory,
						   const char                   *proto,
						   PASBackendFactoryFn           backend_factory);

int             pas_book_factory_get_n_backends   (PASBookFactory               *factory);

void            pas_book_factory_dump_active_backends (PASBookFactory *factory);

gboolean        pas_book_factory_activate         (PASBookFactory               *factory, const char *iid);

GType           pas_book_factory_get_type (void);

G_END_DECLS

#endif /* ! __PAS_BOOK_FACTORY_H__ */
