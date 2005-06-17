/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef __GAL_A11Y_E_TABLE_CLICK_TO_ADD_H__
#define __GAL_A11Y_E_TABLE_CLICK_TO_ADD_H__

#include <glib-object.h>
#include <table/e-table-item.h>
#include <atk/atkgobjectaccessible.h>

#define GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD            (gal_a11y_e_table_click_to_add_get_type ())
#define GAL_A11Y_E_TABLE_CLICK_TO_ADD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD, GalA11yETableClickToAdd))
#define GAL_A11Y_E_TABLE_CLICK_TO_ADD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD, GalA11yETableClickToAddClass))
#define GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD))
#define GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD))

typedef struct _GalA11yETableClickToAdd GalA11yETableClickToAdd;
typedef struct _GalA11yETableClickToAddClass GalA11yETableClickToAddClass;
typedef struct _GalA11yETableClickToAddPrivate GalA11yETableClickToAddPrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yETableClickToAddPrivate comes right after the parent class structure.
 **/
struct _GalA11yETableClickToAdd {
	AtkGObjectAccessible parent;
};

struct _GalA11yETableClickToAddClass {
	AtkGObjectAccessibleClass parent_class;
};

/* Standard Glib function */
GType      gal_a11y_e_table_click_to_add_get_type  (void);
AtkObject *gal_a11y_e_table_click_to_add_new       (GObject *widget);

void       gal_a11y_e_table_click_to_add_init      (void);
#endif /* ! __GAL_A11Y_E_TABLE_CLICK_TO_ADD_H__ */
