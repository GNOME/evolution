/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_REFLOW_MODEL_H_
#define _E_REFLOW_MODEL_H_

#include <gtk/gtkobject.h>
#include <libgnomeui/gnome-canvas.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_REFLOW_MODEL_TYPE        (e_reflow_model_get_type ())
#define E_REFLOW_MODEL(o)          (GTK_CHECK_CAST ((o), E_REFLOW_MODEL_TYPE, EReflowModel))
#define E_REFLOW_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_REFLOW_MODEL_TYPE, EReflowModelClass))
#define E_IS_REFLOW_MODEL(o)       (GTK_CHECK_TYPE ((o), E_REFLOW_MODEL_TYPE))
#define E_IS_REFLOW_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_REFLOW_MODEL_TYPE))

typedef struct {
	GtkObject   base;
} EReflowModel;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	void             (*set_width)      (EReflowModel *etm, int width);

	int              (*count)          (EReflowModel *etm);
	int              (*height)         (EReflowModel *etm, int n, GnomeCanvasGroup *parent);
	GnomeCanvasItem *(*incarnate)      (EReflowModel *etm, int n, GnomeCanvasGroup *parent);
	int              (*compare)        (EReflowModel *etm, int n1, int n2);
	void             (*reincarnate)    (EReflowModel *etm, int n, GnomeCanvasItem *item);
	
	/*
	 * Signals
	 */

	/*
	 * These all come after the change has been made.
	 * Major structural changes: model_changed
	 * Changes only in an item: item_changed
	 */
	void        (*model_changed)       (EReflowModel *etm);
	void        (*model_items_inserted) (EReflowModel *etm, int position, int count);
	void        (*model_item_changed)  (EReflowModel *etm, int n);
} EReflowModelClass;

GtkType          e_reflow_model_get_type        (void);

/**/
void             e_reflow_model_set_width       (EReflowModel     *e_reflow_model,
						 int               width);
int              e_reflow_model_count           (EReflowModel     *e_reflow_model);
int              e_reflow_model_height          (EReflowModel     *e_reflow_model,
						 int               n,
						 GnomeCanvasGroup *parent);
GnomeCanvasItem *e_reflow_model_incarnate       (EReflowModel     *e_reflow_model,
						 int               n,
						 GnomeCanvasGroup *parent);
int              e_reflow_model_compare         (EReflowModel     *e_reflow_model,
						 int               n1,
						 int               n2);
void             e_reflow_model_reincarnate     (EReflowModel     *e_reflow_model,
						 int               n,
						 GnomeCanvasItem  *item);

/*
 * Routines for emitting signals on the e_reflow
 */
void             e_reflow_model_changed         (EReflowModel     *e_reflow_model);
void             e_reflow_model_items_inserted  (EReflowModel     *e_reflow_model,
						 int               position,
						 int               count);
void             e_reflow_model_item_changed    (EReflowModel     *e_reflow_model,
						 int               n);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_REFLOW_MODEL_H_ */
