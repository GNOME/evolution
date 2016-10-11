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
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __GAL_A11Y_E_TEXT_H__
#define __GAL_A11Y_E_TEXT_H__

#include <atk/atk.h>

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

#endif /* __GAL_A11Y_E_TEXT_H__ */
