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
 *	Federico Mena Quintero <federico@ximian.com>
 *	Damon Chaplin <damon@ximian.com>
 *	Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MEMO_PREVIEW_H
#define E_MEMO_PREVIEW_H

#include <gtk/gtk.h>
#include <libecal/e-cal.h>
#include <gtkhtml/gtkhtml.h>

/* Standard GObject macros */
#define E_TYPE_MEMO_PREVIEW \
	(e_memo_preview_get_type ())
#define E_MEMO_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEMO_PREVIEW, EMemoPreview))
#define E_MEMO_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_INSTANCE_CAST_CLASS \
	((cls), E_TYPE_MEMO_PREVIEW, EMemoPreviewClass))
#define E_IS_MEMO_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEMO_PREVIEW))
#define E_IS_MEMO_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MEMO_PREVIEW))
#define E_MEMO_PREVIEW_GET_CLASS \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEMO_PREVIEW, EMemoPreviewClass))

G_BEGIN_DECLS

typedef struct _EMemoPreview EMemoPreview;
typedef struct _EMemoPreviewClass EMemoPreviewClass;
typedef struct _EMemoPreviewPrivate EMemoPreviewPrivate;

struct _EMemoPreview {
	GtkHTML parent;
	EMemoPreviewPrivate *priv;
};

struct _EMemoPreviewClass {
	GtkHTMLClass parent_class;

	/* Notification signals */
	void (*selection_changed) (EMemoPreview *preview, int n_selected);
};


GType		e_memo_preview_get_type		(void);
GtkWidget *	e_memo_preview_new		(void);
icaltimezone *	e_memo_preview_get_default_timezone
						(EMemoPreview *preview);
void		e_memo_preview_set_default_timezone
						(EMemoPreview *preview,
						 icaltimezone *zone);
void		e_memo_preview_display		(EMemoPreview *preview,
						 ECal *ecal,
						 ECalComponent *comp);
void		e_memo_preview_clear		(EMemoPreview *preview);

G_END_DECLS

#endif /* E_MEMO_PREVIEW_H */
