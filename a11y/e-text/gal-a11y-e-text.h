/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2001 Chris Lahey
 */

#ifndef __GAL_A11Y_E_TEXT_H__
#define __GAL_A11Y_E_TEXT_H__

#include <glib-object.h>
#include <gal/e-table/e-table-item.h>

#define GAL_A11Y_TYPE_E_TEXT            (gal_a11y_e_text_get_type ())
#define GAL_A11Y_E_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_TEXT, GalA11yEText))
#define GAL_A11Y_E_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_TEXT, GalA11yETextClass))
#define GAL_A11Y_IS_E_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_TEXT))
#define GAL_A11Y_IS_E_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_TEXT))

typedef struct _GalA11yEText GalA11yEText;
typedef struct _GalA11yETextClass GalA11yETextClass;
typedef struct _GalA11yETextPrivate GalA11yETextPrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yETextPrivate comes right after the parent class structure.
 **/
struct _GalA11yEText {
	AtkObject object;
};

struct _GalA11yETextClass {
	AtkObject parent_class;
};


/* Standard Glib function */
GType      gal_a11y_e_text_get_type  (void);

void       gal_a11y_e_text_init (void);

#endif /* ! __GAL_A11Y_E_TEXT_H__ */
