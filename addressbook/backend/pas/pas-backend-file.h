/*
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __PAS_BACKEND_FILE_H__
#define __PAS_BACKEND_FILE_H__

#include "pas-backend-sync.h"

#define PAS_TYPE_BACKEND_FILE        (pas_backend_file_get_type ())
#define PAS_BACKEND_FILE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BACKEND_FILE, PASBackendFile))
#define PAS_BACKEND_FILE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendFileClass))
#define PAS_IS_BACKEND_FILE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BACKEND_FILE))
#define PAS_IS_BACKEND_FILE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BACKEND_FILE))
#define PAS_BACKEND_FILE_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BACKEND_FILE, PASBackendFileClass))

typedef struct _PASBackendFilePrivate PASBackendFilePrivate;

typedef struct {
	PASBackendSync         parent_object;
	PASBackendFilePrivate *priv;
} PASBackendFile;

typedef struct {
	PASBackendSyncClass parent_class;
} PASBackendFileClass;

PASBackend *pas_backend_file_new      (void);
GType       pas_backend_file_get_type (void);

#endif /* ! __PAS_BACKEND_FILE_H__ */

