/* Evolution memo - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef E_CAL_MODEL_MEMOS_H
#define E_CAL_MODEL_MEMOS_H

#include "e-cal-model.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_MODEL_MEMOS            (e_cal_model_memos_get_type ())
#define E_CAL_MODEL_MEMOS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_MODEL_MEMOS, ECalModelMemo))
#define E_CAL_MODEL_MEMOS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_MODEL_MEMOS, ECalModelMemoClass))
#define E_IS_CAL_MODEL_MEMOS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_MODEL_MEMOS))
#define E_IS_CAL_MODEL_MEMOS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_MODEL_MEMOS))

typedef struct _ECalModelMemosPrivate ECalModelMemosPrivate;

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	   .etspec files for the tables using this model */
	E_CAL_MODEL_MEMOS_FIELD_LAST = E_CAL_MODEL_FIELD_LAST

} ECalModelMemoField;

typedef struct {
	ECalModel model;
	ECalModelMemosPrivate *priv;
} ECalModelMemos;

typedef struct {
	ECalModelClass parent_class;
} ECalModelMemosClass;

GType          e_cal_model_memos_get_type (void);
ECalModelMemos *e_cal_model_memos_new (void);

G_END_DECLS

#endif
