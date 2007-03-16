/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_CELL_H__
#define __GAL_A11Y_E_CELL_H__

#include <glib-object.h>
#include <table/e-table-item.h>
#include <table/e-cell.h>

#define GAL_A11Y_TYPE_E_CELL            (gal_a11y_e_cell_get_type ())
#define GAL_A11Y_E_CELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL, GalA11yECell))
#define GAL_A11Y_E_CELL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_CELL, GalA11yECellClass))
#define GAL_A11Y_IS_E_CELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL))
#define GAL_A11Y_IS_E_CELL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL))

typedef struct _GalA11yECell GalA11yECell;
typedef struct _GalA11yECellClass GalA11yECellClass;
typedef struct _GalA11yECellPrivate GalA11yECellPrivate;
typedef struct _ActionInfo ActionInfo;
typedef void (*ACTION_FUNC) (GalA11yECell *cell);


/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yECellPrivate comes right after the parent class structure.
 **/
struct _GalA11yECell {
	AtkObject object;

	ETableItem *item;
	ECellView  *cell_view;
	AtkObject  *parent;
	int         model_col;
	int         view_col;
	int         row;
	AtkStateSet *state_set;
	GList       *action_list;
	gint         action_idle_handler;
	ACTION_FUNC  action_func;
};

struct _GalA11yECellClass {
	AtkObjectClass parent_class;
};

struct _ActionInfo {
	gchar *name;
	gchar *description;
	gchar *keybinding;
	ACTION_FUNC do_action_func;
};



/* Standard Glib function */
GType      gal_a11y_e_cell_get_type   (void);
AtkObject *gal_a11y_e_cell_new        (ETableItem *item,
				       ECellView  *cell_view,
				       AtkObject  *parent,
				       int         model_col,
				       int         view_col,
				       int         row);
void       gal_a11y_e_cell_construct  (AtkObject  *object,
				       ETableItem *item,
				       ECellView  *cell_view,
				       AtkObject  *parent,
				       int         model_col,
				       int         view_col,
				       int         row);

void	gal_a11y_e_cell_type_add_action_interface (GType type);
                                                                                
gboolean gal_a11y_e_cell_add_action	(GalA11yECell	*cell,
				         const gchar     *action_name,
					 const gchar     *action_description,
					 const gchar     *action_keybinding,
					 ACTION_FUNC     action_func);
                                                                                
gboolean gal_a11y_e_cell_remove_action	(GalA11yECell	*cell,
                                         gint           action_id);
                                                                                
gboolean gal_a11y_e_cell_remove_action_by_name (GalA11yECell        *cell,
                                          	const gchar     *action_name);

gboolean gal_a11y_e_cell_add_state     (GalA11yECell *cell,
					AtkStateType state_type,
					gboolean     emit_signal);

gboolean gal_a11y_e_cell_remove_state  (GalA11yECell *cell,
					AtkStateType state_type,
					gboolean     emit_signal);


#endif /* ! __GAL_A11Y_E_CELL_H__ */
