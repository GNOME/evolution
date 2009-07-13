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

#ifndef _GAL_DEFINE_VIEWS_MODEL_H_
#define _GAL_DEFINE_VIEWS_MODEL_H_

#include <table/e-table-model.h>
#include <widgets/menus/gal-view.h>
#include <widgets/menus/gal-view-collection.h>

G_BEGIN_DECLS

#define GAL_DEFINE_VIEWS_MODEL_TYPE        (gal_define_views_model_get_type ())
#define GAL_DEFINE_VIEWS_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_DEFINE_VIEWS_MODEL_TYPE, GalDefineViewsModel))
#define GAL_DEFINE_VIEWS_MODEL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_DEFINE_VIEWS_MODEL_TYPE, GalDefineViewsModelClass))
#define GAL_IS_DEFINE_VIEWS_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_DEFINE_VIEWS_MODEL_TYPE))
#define GAL_IS_DEFINE_VIEWS_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_DEFINE_VIEWS_MODEL_TYPE))

typedef struct {
	ETableModel parent;

	/* item specific fields */
	GalViewCollection *collection;

	guint editable : 1;
} GalDefineViewsModel;

typedef struct {
	ETableModelClass parent_class;
} GalDefineViewsModelClass;

GType        gal_define_views_model_get_type     (void);
ETableModel *gal_define_views_model_new          (void);

void         gal_define_views_model_append       (GalDefineViewsModel *model,
						  GalView             *view);
GalView     *gal_define_views_model_get_view     (GalDefineViewsModel *model,
						  gint                  i);
void         gal_define_views_model_delete_view  (GalDefineViewsModel *model,
						  gint                  i);
void         gal_define_views_model_copy_view    (GalDefineViewsModel *model,
						  gint                  i);

G_END_DECLS

#endif /* _GAL_DEFINE_VIEWS_MODEL_H_ */
