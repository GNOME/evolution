/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_FILE_REQUEST_H
#define E_FILE_REQUEST_H

#include <e-util/e-content-request.h>

/* Standard GObject macros */
#define E_TYPE_FILE_REQUEST \
	(e_file_request_get_type ())
#define E_FILE_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILE_REQUEST, EFileRequest))
#define E_FILE_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILE_REQUEST, EFileRequestClass))
#define E_IS_FILE_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILE_REQUEST))
#define E_IS_FILE_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILE_REQUEST))
#define E_FILE_REQUEST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILE_REQUEST, EFileRequestClass))

G_BEGIN_DECLS

typedef struct _EFileRequest EFileRequest;
typedef struct _EFileRequestClass EFileRequestClass;
typedef struct _EFileRequestPrivate EFileRequestPrivate;

struct _EFileRequest {
	GObject parent;
	EFileRequestPrivate *priv;
};

struct _EFileRequestClass {
	GObjectClass parent;
};

GType		e_file_request_get_type		(void) G_GNUC_CONST;
EContentRequest *
		e_file_request_new		(void);

G_END_DECLS

#endif /* E_FILE_REQUEST_H */
