/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_TABLE_H__
#define __GAL_A11Y_E_TABLE_H__

#include <glib-object.h>
#include <atk/atkobject.h>
#include <atk/atkcomponent.h>
#include <gtk/gtkaccessible.h>

#define GAL_A11Y_TYPE_E_TABLE            (gal_a11y_e_table_get_type ())
#define GAL_A11Y_E_TABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE, GalA11yETable))
#define GAL_A11Y_E_TABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE, GalA11yETableClass))
#define GAL_A11Y_IS_E_TABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE))
#define GAL_A11Y_IS_E_TABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE))

typedef struct _GalA11yETable GalA11yETable;
typedef struct _GalA11yETableClass GalA11yETableClass;
typedef struct _GalA11yETablePrivate GalA11yETablePrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yETablePrivate comes right after the parent class structure.
 **/
struct _GalA11yETable {
	GtkAccessible object;
};

struct _GalA11yETableClass {
	GtkAccessibleClass parent_class;
};


/* Standard Glib function */
GType      gal_a11y_e_table_get_type  (void);
AtkObject *gal_a11y_e_table_new       (GObject *table);

void       gal_a11y_e_table_init (void);

#endif /* ! __GAL_A11Y_E_TABLE_H__ */
