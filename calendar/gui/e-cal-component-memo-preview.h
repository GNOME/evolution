/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-component-memo-preview.h
 *
 * Copyright (C) 2004  Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#ifndef _E_CAL_COMPONENT_MEMO_PREVIEW_H_
#define _E_CAL_COMPONENT_MEMO_PREVIEW_H_

#include <gtk/gtktable.h>
#include <libecal/e-cal.h>

#define E_TYPE_CAL_COMPONENT_MEMO_PREVIEW            (e_cal_component_memo_preview_get_type ())
#define E_CAL_COMPONENT_MEMO_PREVIEW(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW, ECalComponentMemoPreview))
#define E_CAL_COMPONENT_MEMO_PREVIEW_CLASS(klass)    (GTK_CHECK_CAST_CLASS ((klass), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW, \
				                 ECalComponentMemoPreviewClass))
#define E_IS_CAL_COMPONENT_MEMO_PREVIEW(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW))
#define E_IS_CAL_COMPONENT_MEMO_PREVIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_COMPONENT_MEMO_PREVIEW))

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
	void (* selection_changed) (ECalComponentMemoPreview *preview, int n_selected);
};


GtkType    e_cal_component_memo_preview_get_type        (void);
GtkWidget *e_cal_component_memo_preview_new             (void);

icaltimezone *e_cal_component_memo_preview_get_default_timezone (ECalComponentMemoPreview *preview);
void e_cal_component_memo_preview_set_default_timezone (ECalComponentMemoPreview *preview, icaltimezone *zone);

void e_cal_component_memo_preview_display             (ECalComponentMemoPreview *preview, ECal *ecal, ECalComponent *comp);
void e_cal_component_memo_preview_clear             (ECalComponentMemoPreview *preview);

#endif /* _E_CAL_COMPONENT_MEMO_PREVIEW_H_ */
