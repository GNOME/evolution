/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_TABLE_ITEM_H__
#define __GAL_A11Y_E_TABLE_ITEM_H__

#include <glib-object.h>
#include <gal/e-table/e-table-item.h>
#include <atk/atkgobjectaccessible.h>

#define GAL_A11Y_TYPE_E_TABLE_ITEM            (gal_a11y_e_table_item_get_type ())
#define GAL_A11Y_E_TABLE_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE_ITEM, GalA11yETableItem))
#define GAL_A11Y_E_TABLE_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE_ITEM, GalA11yETableItemClass))
#define GAL_A11Y_IS_E_TABLE_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE_ITEM))
#define GAL_A11Y_IS_E_TABLE_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE_ITEM))

typedef struct _GalA11yETableItem GalA11yETableItem;
typedef struct _GalA11yETableItemClass GalA11yETableItemClass;
typedef struct _GalA11yETableItemPrivate GalA11yETableItemPrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yETableItemPrivate comes right after the parent class structure.
 **/
struct _GalA11yETableItem {
	AtkGObjectAccessible parent;
};

struct _GalA11yETableItemClass {
	AtkGObjectAccessibleClass parent_class;
};


/* Standard Glib function */
GType      gal_a11y_e_table_item_get_type  (void);
AtkObject *gal_a11y_e_table_item_new       (ETableItem *item);

void gal_a11y_e_table_item_init (void);

#endif /* ! __GAL_A11Y_E_TABLE_ITEM_H__ */
