/*
 * An abstract class which defines the API to a given backend.
 * There will be one PASBackend object for every URI which is loaded.
 *
 * Two people will call into the PASBackend API:
 *
 * 1. The PASBookFactory, when it has been asked to load a book.
 *    It will create a new PASBackend if one is not already running
 *    for the requested URI.  It will call pas_backend_add_client to
 *    add a new client to an existing PASBackend server.
 *
 * 2. A PASBook, when a client has requested an operation on the
 *    Evolution_Book interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __PAS_BACKEND_H__
#define __PAS_BACKEND_H__

#include <libgnome/gnome-defs.h>
#include <gtk/gtkobject.h>
#include <pas/addressbook.h>

typedef struct _PASBackend        PASBackend;
typedef struct _PASBackendPrivate PASBackendPrivate;

#include <pas/pas-book.h>

struct _PASBackend {
	GtkObject parent_object;
	PASBackendPrivate *priv;
};

typedef struct {
	GtkObjectClass parent_class;

	/* Virtual methods */
	gboolean (*load_uri) (PASBackend *backend, const char *uri);
	const char *(* get_uri) (PASBackend *backend);
	gboolean (*add_client) (PASBackend *backend, Evolution_BookListener listener);
	void (*remove_client) (PASBackend *backend, PASBook *book);

	/* Notification signals */
	void (* last_client_gone) (PASBackend *backend);
} PASBackendClass;

typedef PASBackend * (*PASBackendFactoryFn) (void);

gboolean    pas_backend_construct          (PASBackend             *backend);
gboolean    pas_backend_load_uri           (PASBackend             *backend,
					    const char             *uri);
const char *pas_backend_get_uri            (PASBackend             *backend);
gboolean    pas_backend_add_client         (PASBackend             *backend,
					    Evolution_BookListener  listener);
void        pas_backend_remove_client      (PASBackend             *backend,
					    PASBook                *book);

void        pas_backend_last_client_gone   (PASBackend             *backend);

GtkType     pas_backend_get_type           (void);

#define PAS_BACKEND_TYPE        (pas_backend_get_type ())
#define PAS_BACKEND(o)          (GTK_CHECK_CAST ((o), PAS_BACKEND_TYPE, PASBackend))
#define PAS_BACKEND_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendClass))
#define PAS_IS_BACKEND(o)       (GTK_CHECK_TYPE ((o), PAS_BACKEND_TYPE))
#define PAS_IS_BACKEND_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_BACKEND_TYPE))

#endif /* ! __PAS_BACKEND_H__ */

