/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_DEFINE_VIEWS_MODEL_H_
#define _GAL_DEFINE_VIEWS_MODEL_H_

#include <gal/e-table/e-table-model.h>
#include "gal-view.h"

#define GAL_DEFINE_VIEWS_MODEL_TYPE        (gal_define_views_model_get_type ())
#define GAL_DEFINE_VIEWS_MODEL(o)          (GTK_CHECK_CAST ((o), GAL_DEFINE_VIEWS_MODEL_TYPE, GalDefineViewsModel))
#define GAL_DEFINE_VIEWS_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_DEFINE_VIEWS_MODEL_TYPE, GalDefineViewsModelClass))
#define GAL_IS_DEFINE_VIEWS_MODEL(o)       (GTK_CHECK_TYPE ((o), GAL_DEFINE_VIEWS_MODEL_TYPE))
#define GAL_IS_DEFINE_VIEWS_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_DEFINE_VIEWS_MODEL_TYPE))

/* Virtual Column list:
   0   Email
   1   Full Name
   2   Street
   3   Phone
*/

typedef struct {
	ETableModel parent;

	/* item specific fields */
	GalView **data;
	int data_count;

	guint editable : 1;
} GalDefineViewsModel;


typedef struct {
	ETableModelClass parent_class;
} GalDefineViewsModelClass;


GtkType      gal_define_views_model_get_type  (void);
ETableModel *gal_define_views_model_new       (void);

void         gal_define_views_model_append    (GalDefineViewsModel *model,
					       GalView             *view);

#endif /* _GAL_DEFINE_VIEWS_MODEL_H_ */
