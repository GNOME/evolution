/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 *    GNOME_Evolution_Addressbook_Book interface.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __PAS_BACKEND_H__
#define __PAS_BACKEND_H__

#include <glib.h>
#include <glib-object.h>
#include <pas/addressbook.h>

#define PAS_TYPE_BACKEND         (pas_backend_get_type ())
#define PAS_BACKEND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BACKEND, PASBackend))
#define PAS_BACKEND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PAS_TYPE_BACKEND, PASBackendClass))
#define PAS_IS_BACKEND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BACKEND))
#define PAS_IS_BACKEND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BACKEND))
#define PAS_BACKEND_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((k), PAS_TYPE_BACKEND, PASBackendClass))

typedef struct _PASBackend        PASBackend;
typedef struct _PASBackendPrivate PASBackendPrivate;

#include <pas/pas-book.h>

struct _PASBackend {
	GObject parent_object;
	PASBackendPrivate *priv;
};

typedef struct {
	GObjectClass parent_class;

	/* Virtual methods */
	GNOME_Evolution_Addressbook_BookListener_CallStatus (*load_uri) (PASBackend *backend, const char *uri);
	const char *(* get_uri) (PASBackend *backend);
	gboolean (*add_client) (PASBackend *backend, GNOME_Evolution_Addressbook_BookListener listener);
	void (*remove_client) (PASBackend *backend, PASBook *book);
        char *(*get_static_capabilities) (PASBackend *backend);

	/* Notification signals */
	void (* last_client_gone) (PASBackend *backend);
} PASBackendClass;

typedef PASBackend * (*PASBackendFactoryFn) (void);

gboolean    pas_backend_construct                (PASBackend             *backend);

GNOME_Evolution_Addressbook_BookListener_CallStatus
            pas_backend_load_uri                 (PASBackend             *backend,
						  const char             *uri);
const char *pas_backend_get_uri                  (PASBackend             *backend);

gboolean    pas_backend_add_client               (PASBackend             *backend,
						  GNOME_Evolution_Addressbook_BookListener  listener);
void        pas_backend_remove_client            (PASBackend             *backend,
						  PASBook                *book);
char       *pas_backend_get_static_capabilities  (PASBackend             *backend);

void        pas_backend_last_client_gone         (PASBackend             *backend);

GType     pas_backend_get_type                 (void);

#endif /* ! __PAS_BACKEND_H__ */

