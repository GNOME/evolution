/*
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __PAS_BACKEND_FILE_H__
#define __PAS_BACKEND_FILE_H__

#include <libgnome/gnome-defs.h>
#include "pas-backend.h"

typedef struct _PASBackendFilePrivate PASBackendFilePrivate;

typedef struct {
	PASBackend             parent_object;
	PASBackendFilePrivate *priv;
} PASBackendFile;

typedef struct {
	PASBackendClass parent_class;
} PASBackendFileClass;

PASBackend *pas_backend_file_new      (void);
GtkType     pas_backend_file_get_type (void);

#define PAS_BACKEND_FILE_TYPE        (pas_backend_file_get_type ())
#define PAS_BACKEND_FILE(o)          (GTK_CHECK_CAST ((o), PAS_BACKEND_FILE_TYPE, PASBackendFile))
#define PAS_BACKEND_FILE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendFileClass))
#define PAS_IS_BACKEND_FILE(o)       (GTK_CHECK_TYPE ((o), PAS_BACKEND_FILE_TYPE))
#define PAS_IS_BACKEND_FILE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_BACKEND_FILE_TYPE))

#endif /* ! __PAS_BACKEND_FILE_H__ */

