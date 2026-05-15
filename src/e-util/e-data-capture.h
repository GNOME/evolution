/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_DATA_CAPTURE_H
#define E_DATA_CAPTURE_H

#include <gio/gio.h>

/* Standard GObject macros */
#define E_TYPE_DATA_CAPTURE \
	(e_data_capture_get_type ())
#define E_DATA_CAPTURE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DATA_CAPTURE, EDataCapture))
#define E_DATA_CAPTURE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DATA_CAPTURE, EDataCaptureClass))
#define E_IS_DATA_CAPTURE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DATA_CAPTURE))
#define E_IS_DATA_CAPTURE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DATA_CAPTURE))
#define E_DATA_CAPTURE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DATA_CAPTURE, EDataCaptureClass))

G_BEGIN_DECLS

typedef struct _EDataCapture EDataCapture;
typedef struct _EDataCaptureClass EDataCaptureClass;
typedef struct _EDataCapturePrivate EDataCapturePrivate;

/**
 * EDataCapture:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EDataCapture {
	GObject parent;
	EDataCapturePrivate *priv;
};

struct _EDataCaptureClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*finished)		(EDataCapture *capture,
						 GBytes *data);
};

GType		e_data_capture_get_type		(void) G_GNUC_CONST;
EDataCapture *	e_data_capture_new		(GMainContext *main_context);
GMainContext *	e_data_capture_ref_main_context	(EDataCapture *data_capture);

G_END_DECLS

#endif /* E_DATA_CAPTURE_H */

