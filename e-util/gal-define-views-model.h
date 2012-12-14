/*
 *
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef GAL_DEFINE_VIEWS_MODEL_H
#define GAL_DEFINE_VIEWS_MODEL_H

#include <e-util/e-table-model.h>
#include <e-util/gal-view.h>
#include <e-util/gal-view-collection.h>

/* Standard GObject macros */
#define GAL_TYPE_DEFINE_VIEWS_MODEL \
	(gal_define_views_model_get_type ())
#define GAL_DEFINE_VIEWS_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_DEFINE_VIEWS_MODEL, GalDefineViewsModel))
#define GAL_DEFINE_VIEWS_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_DEFINE_VIEWS_MODEL, GalDefineViewsModelClass))
#define GAL_IS_DEFINE_VIEWS_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_DEFINE_VIEWS_MODEL))
#define GAL_IS_DEFINE_VIEWS_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_DEFINE_VIEWS_MODEL))
#define GAL_DEFINE_VIEWS_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_DEFINE_VIEWS_MODEL, GalDefineViewsModelClass))

G_BEGIN_DECLS

typedef struct _GalDefineViewsModel GalDefineViewsModel;
typedef struct _GalDefineViewsModelClass GalDefineViewsModelClass;

struct _GalDefineViewsModel {
	ETableModel parent;

	/* item specific fields */
	GalViewCollection *collection;

	guint editable : 1;
};

struct _GalDefineViewsModelClass {
	ETableModelClass parent_class;
};

GType		gal_define_views_model_get_type	(void) G_GNUC_CONST;
ETableModel *	gal_define_views_model_new	(void);
void		gal_define_views_model_append	(GalDefineViewsModel *model,
						 GalView *view);
GalView *	gal_define_views_model_get_view	(GalDefineViewsModel *model,
						 gint i);
void		gal_define_views_model_delete_view
						(GalDefineViewsModel *model,
						 gint i);
void		gal_define_views_model_copy_view
						(GalDefineViewsModel *model,
						 gint i);

G_END_DECLS

#endif /* GAL_DEFINE_VIEWS_MODEL_H */
