/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Rodrigo Moya <rodrigo@ximian.com>
 * SPDX-FileContributor: Nathan Owens <pianocomp81@yahoo.com>
 */

#ifndef E_CAL_MODEL_MEMOS_H
#define E_CAL_MODEL_MEMOS_H

#include "e-cal-model.h"

/* Standard GObject macros */
#define E_TYPE_CAL_MODEL_MEMOS \
	(e_cal_model_memos_get_type ())
#define E_CAL_MODEL_MEMOS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_MODEL_MEMOS, ECalModelMemo))
#define E_CAL_MODEL_MEMOS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_MODEL_MEMOS, ECalModelMemoClass))
#define E_IS_CAL_MODEL_MEMOS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_MODEL_MEMOS))
#define E_IS_CAL_MODEL_MEMOS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_MODEL_MEMOS))
#define E_CAL_MODEL_MEMOS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_MODEL_MEMOS, ECalModelMemoClass))

G_BEGIN_DECLS

typedef struct _ECalModelMemos ECalModelMemos;
typedef struct _ECalModelMemosClass ECalModelMemosClass;
typedef struct _ECalModelMemosPrivate ECalModelMemosPrivate;

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	 * .etspec files for the tables using this model */
	E_CAL_MODEL_MEMOS_FIELD_STATUS = E_CAL_MODEL_FIELD_LAST,
	E_CAL_MODEL_MEMOS_FIELD_LAST

} ECalModelMemoField;

struct _ECalModelMemos {
	ECalModel parent;
	ECalModelMemosPrivate *priv;
};

struct _ECalModelMemosClass {
	ECalModelClass parent_class;
};

GType		e_cal_model_memos_get_type	(void);
ECalModel *	e_cal_model_memos_new		(ECalDataModel *data_model,
						 ESourceRegistry *registry,
						 EShell *shell);

G_END_DECLS

#endif /* E_CAL_MODEL_MEMOS_H */
