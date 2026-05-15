/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Federico Mena Quintero <federico@ximian.com>
 * SPDX-FileContributor: Damon Chaplin <damon@ximian.com>
 */

#ifndef E_CAL_COMPONENT_PREVIEW_H
#define E_CAL_COMPONENT_PREVIEW_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CAL_COMPONENT_PREVIEW \
	(e_cal_component_preview_get_type ())
#define E_CAL_COMPONENT_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_COMPONENT_PREVIEW, ECalComponentPreview))
#define E_CAL_COMPONENT_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_INSTANCE_CAST_CLASS \
	((cls), E_TYPE_CAL_COMPONENT_PREVIEW, ECalComponentPreviewClass))
#define E_IS_CAL_COMPONENT_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_COMPONENT_PREVIEW))
#define E_IS_CAL_COMPONENT_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_COMPONENT_PREVIEW))
#define E_CAL_COMPONENT_PREVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_COMPONENT_PREVIEW, ECalComponentPreviewClass))

G_BEGIN_DECLS

typedef struct _ECalComponentPreview ECalComponentPreview;
typedef struct _ECalComponentPreviewClass ECalComponentPreviewClass;
typedef struct _ECalComponentPreviewPrivate ECalComponentPreviewPrivate;

struct _ECalComponentPreview {
	EWebView parent;
	ECalComponentPreviewPrivate *priv;
};

struct _ECalComponentPreviewClass {
	EWebViewClass parent_class;

	/* Notification signals */
	void	(*selection_changed)		(ECalComponentPreview *preview,
						 gint n_selected);
};

GType		e_cal_component_preview_get_type (void);
GtkWidget *	e_cal_component_preview_new	(void);
void		e_cal_component_preview_display	(ECalComponentPreview *preview,
						 ECalClient *client,
						 ECalComponent *comp,
						 ICalTimezone *zone,
						 gboolean use_24_hour_format);
void		e_cal_component_preview_clear	(ECalComponentPreview *preview);
void		e_cal_component_preview_set_attachment_store
						(ECalComponentPreview *preview,
						 EAttachmentStore *store);
EAttachmentStore *
		e_cal_component_preview_get_attachment_store
						(ECalComponentPreview *preview);

G_END_DECLS

#endif /* E_CAL_COMPONENT_PREVIEW_H */
