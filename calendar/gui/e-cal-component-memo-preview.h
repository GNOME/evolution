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
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CAL_COMPONENT_MEMO_PREVIEW_H_
#define _E_CAL_COMPONENT_MEMO_PREVIEW_H_

#include <gtk/gtk.h>
#include <libecal/e-cal.h>

#define E_TYPE_CAL_COMPONENT_MEMO_PREVIEW            (e_cal_component_memo_preview_get_type ())
#define E_CAL_COMPONENT_MEMO_PREVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW, ECalComponentMemoPreview))
#define E_CAL_COMPONENT_MEMO_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_INSTANCE_CAST_CLASS ((klass), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW, \
						 ECalComponentMemoPreviewClass))
#define E_IS_CAL_COMPONENT_MEMO_PREVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW))
#define E_IS_CAL_COMPONENT_MEMO_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW))

typedef struct _ECalComponentMemoPreview	ECalComponentMemoPreview;
typedef struct _ECalComponentMemoPreviewClass	ECalComponentMemoPreviewClass;
typedef struct _ECalComponentMemoPreviewPrivate	ECalComponentMemoPreviewPrivate;

struct _ECalComponentMemoPreview {
	GtkTable table;

	/* Private data */
	ECalComponentMemoPreviewPrivate *priv;
};

struct _ECalComponentMemoPreviewClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* selection_changed) (ECalComponentMemoPreview *preview, gint n_selected);
};

GType      e_cal_component_memo_preview_get_type        (void);
GtkWidget *e_cal_component_memo_preview_new             (void);

icaltimezone *e_cal_component_memo_preview_get_default_timezone (ECalComponentMemoPreview *preview);
void e_cal_component_memo_preview_set_default_timezone (ECalComponentMemoPreview *preview, icaltimezone *zone);

void e_cal_component_memo_preview_display             (ECalComponentMemoPreview *preview, ECal *ecal, ECalComponent *comp);
void e_cal_component_memo_preview_clear             (ECalComponentMemoPreview *preview);
GtkWidget *e_cal_component_memo_preview_get_html (ECalComponentMemoPreview *preview);

#endif /* _E_CAL_COMPONENT_MEMO_PREVIEW_H_ */
