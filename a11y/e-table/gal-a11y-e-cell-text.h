/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_CELL_TEXT_H__
#define __GAL_A11Y_E_CELL_TEXT_H__

#include <glib-object.h>
#include <gal/e-table/e-table-item.h>
#include <gal/e-table/e-cell-text.h>
#include <a11y/e-table/gal-a11y-e-cell.h>

#define GAL_A11Y_TYPE_E_CELL_TEXT            (gal_a11y_e_cell_text_get_type ())
#define GAL_A11Y_E_CELL_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_TEXT, GalA11yECellText))
#define GAL_A11Y_E_CELL_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_CELL_TEXT, GalA11yECellTextClass))
#define GAL_A11Y_IS_E_CELL_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_TEXT))
#define GAL_A11Y_IS_E_CELL_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_TEXT))

typedef struct _GalA11yECellText GalA11yECellText;
typedef struct _GalA11yECellTextClass GalA11yECellTextClass;
typedef struct _GalA11yECellTextPrivate GalA11yECellTextPrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yECellTextPrivate comes right after the parent class structure.
 **/
struct _GalA11yECellText {
	GalA11yECell object;
	gint inserted_id;
	gint deleted_id;
};

struct _GalA11yECellTextClass {
	GalA11yECellClass parent_class;
};


/* Standard Glib function */
GType      gal_a11y_e_cell_text_get_type   (void);
AtkObject *gal_a11y_e_cell_text_new        (ETableItem *item,
					    ECellView  *cell_view,
					    AtkObject  *parent,
					    int         model_col,
					    int         view_col,
					    int         row);

#endif /* ! __GAL_A11Y_E_CELL_TEXT_H__ */
