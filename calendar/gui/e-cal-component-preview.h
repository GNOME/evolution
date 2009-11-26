/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CAL_COMPONENT_PREVIEW_H
#define E_CAL_COMPONENT_PREVIEW_H

#include <gtk/gtk.h>
#include <libecal/e-cal.h>
#include <misc/e-web-view.h>

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
	void (* selection_changed) (ECalComponentPreview *preview, gint n_selected);
};

GType		e_cal_component_preview_get_type(void);
GtkWidget *	e_cal_component_preview_new	(void);
icaltimezone *	e_cal_component_preview_get_default_timezone
						(ECalComponentPreview *preview);
void		e_cal_component_preview_set_default_timezone
						(ECalComponentPreview *preview,
						 icaltimezone *zone);
void		e_cal_component_preview_display	(ECalComponentPreview *preview,
						 ECal *ecal,
						 ECalComponent *comp);
void		e_cal_component_preview_clear	(ECalComponentPreview *preview);

G_END_DECLS

#endif /* E_CAL_COMPONENT_PREVIEW_H */
