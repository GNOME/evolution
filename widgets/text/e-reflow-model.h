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

#ifndef _E_REFLOW_MODEL_H_
#define _E_REFLOW_MODEL_H_

#include <glib-object.h>
#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

#define E_REFLOW_MODEL_TYPE        (e_reflow_model_get_type ())
#define E_REFLOW_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_REFLOW_MODEL_TYPE, EReflowModel))
#define E_REFLOW_MODEL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_REFLOW_MODEL_TYPE, EReflowModelClass))
#define E_IS_REFLOW_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_REFLOW_MODEL_TYPE))
#define E_IS_REFLOW_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_REFLOW_MODEL_TYPE))
#define E_REFLOW_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_REFLOW_MODEL_TYPE, EReflowModelClass))

typedef struct {
	GObject   base;
} EReflowModel;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	void             (*set_width)      (EReflowModel *etm, gint width);

	gint              (*count)          (EReflowModel *etm);
	gint              (*height)         (EReflowModel *etm, gint n, GnomeCanvasGroup *parent);
	GnomeCanvasItem *(*incarnate)      (EReflowModel *etm, gint n, GnomeCanvasGroup *parent);
	gint              (*compare)        (EReflowModel *etm, gint n1, gint n2);
	void             (*reincarnate)    (EReflowModel *etm, gint n, GnomeCanvasItem *item);

	/*
	 * Signals
	 */

	/*
	 * These all come after the change has been made.
	 * Major structural changes: model_changed
	 * Changes to the sorting of elements: comparison_changed
	 * Changes only in an item: item_changed
	 */
	void        (*model_changed)       (EReflowModel *etm);
	void        (*comparison_changed)  (EReflowModel *etm);
	void        (*model_items_inserted) (EReflowModel *etm, gint position, gint count);
	void        (*model_item_removed)  (EReflowModel *etm, gint position);
	void        (*model_item_changed)  (EReflowModel *etm, gint n);
} EReflowModelClass;

GType            e_reflow_model_get_type        (void);

/**/
void             e_reflow_model_set_width       (EReflowModel     *e_reflow_model,
						 gint               width);
gint              e_reflow_model_count           (EReflowModel     *e_reflow_model);
gint              e_reflow_model_height          (EReflowModel     *e_reflow_model,
						 gint               n,
						 GnomeCanvasGroup *parent);
GnomeCanvasItem *e_reflow_model_incarnate       (EReflowModel     *e_reflow_model,
						 gint               n,
						 GnomeCanvasGroup *parent);
gint              e_reflow_model_compare         (EReflowModel     *e_reflow_model,
						 gint               n1,
						 gint               n2);
void             e_reflow_model_reincarnate     (EReflowModel     *e_reflow_model,
						 gint               n,
						 GnomeCanvasItem  *item);

/*
 * Routines for emitting signals on the e_reflow
 */
void             e_reflow_model_changed            (EReflowModel     *e_reflow_model);
void             e_reflow_model_comparison_changed (EReflowModel     *e_reflow_model);
void             e_reflow_model_items_inserted     (EReflowModel     *e_reflow_model,
						    gint               position,
						    gint               count);
void             e_reflow_model_item_removed       (EReflowModel     *e_reflow_model,
						    gint               n);
void             e_reflow_model_item_changed       (EReflowModel     *e_reflow_model,
						    gint               n);

G_END_DECLS

#endif /* _E_REFLOW_MODEL_H_ */
