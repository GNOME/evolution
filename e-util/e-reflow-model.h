/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#ifndef E_REFLOW_MODEL_H
#define E_REFLOW_MODEL_H

#include <libgnomecanvas/libgnomecanvas.h>

/* Standard GObject macros */
#define E_TYPE_REFLOW_MODEL \
	(e_reflow_model_get_type ())
#define E_REFLOW_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_REFLOW_MODEL, EReflowModel))
#define E_REFLOW_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_REFLOW_MODEL, EReflowModelClass))
#define E_IS_REFLOW_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_REFLOW_MODEL))
#define E_IS_REFLOW_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_REFLOW_MODEL))
#define E_REFLOW_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_REFLOW_MODEL, EReflowModelClass))

G_BEGIN_DECLS

typedef struct _EReflowModel EReflowModel;
typedef struct _EReflowModelClass EReflowModelClass;

struct _EReflowModel {
	GObject parent;
};

struct _EReflowModelClass {
	GObjectClass parent_class;

	/* Method */
	void		(*set_width)		(EReflowModel *reflow_model,
						 gint width);
	gint		(*count)		(EReflowModel *reflow_model);
	gint		(*height)		(EReflowModel *reflow_model,
						 gint n,
						 GnomeCanvasGroup *parent);
	GnomeCanvasItem *
			(*incarnate)		(EReflowModel *reflow_model,
						 gint n,
						 GnomeCanvasGroup *parent);
	GHashTable *	(*create_cmp_cache)	(EReflowModel *reflow_model);
	gint		(*compare)		(EReflowModel *reflow_model,
						 gint n1,
						 gint n2,
						 GHashTable *cmp_cache);
	void		(*reincarnate)		(EReflowModel *reflow_model,
						 gint n,
						 GnomeCanvasItem *item);

	/* Signals
	 *
	 * These all come after the change has been made.
	 * Major structural changes: model_changed
	 * Changes to the sorting of elements: comparison_changed
	 * Changes only in an item: item_changed
	 */
	void		(*model_changed)	(EReflowModel *reflow_model);
	void		(*comparison_changed)	(EReflowModel *reflow_model);
	void		(*model_items_inserted)	(EReflowModel *reflow_model,
						 gint position,
						 gint count);
	void		(*model_item_removed)	(EReflowModel *reflow_model,
						 gint position);
	void		(*model_item_changed)	(EReflowModel *reflow_model,
						 gint n);
};

GType		e_reflow_model_get_type		(void) G_GNUC_CONST;
void		e_reflow_model_set_width	(EReflowModel *reflow_model,
						 gint width);
gint		e_reflow_model_count		(EReflowModel *reflow_model);
gint		e_reflow_model_height		(EReflowModel *reflow_model,
						 gint n,
						 GnomeCanvasGroup *parent);
GnomeCanvasItem *
		e_reflow_model_incarnate	(EReflowModel *reflow_model,
						 gint n,
						 GnomeCanvasGroup *parent);
GHashTable *	e_reflow_model_create_cmp_cache	(EReflowModel *reflow_model);
gint		e_reflow_model_compare		(EReflowModel *reflow_model,
						 gint n1,
						 gint n2,
						 GHashTable *cmp_cache);
void		e_reflow_model_reincarnate	(EReflowModel *reflow_model,
						 gint n,
						 GnomeCanvasItem *item);
void		e_reflow_model_changed		(EReflowModel *reflow_model);
void		e_reflow_model_comparison_changed
						(EReflowModel *reflow_model);
void		e_reflow_model_items_inserted	(EReflowModel *reflow_model,
						 gint position,
						 gint count);
void		e_reflow_model_item_removed	(EReflowModel *reflow_model,
						 gint n);
void		e_reflow_model_item_changed	(EReflowModel *reflow_model,
						 gint n);

G_END_DECLS

#endif /* E_REFLOW_MODEL_H */

