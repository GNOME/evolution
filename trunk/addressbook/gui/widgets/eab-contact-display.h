/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _EAB_CONTACT_DISPLAY_H_
#define _EAB_CONTACT_DISPLAY_H_

#include <gtkhtml/gtkhtml.h>
#include <libebook/e-contact.h>

#define EAB_TYPE_CONTACT_DISPLAY        (eab_contact_display_get_type ())
#define EAB_CONTACT_DISPLAY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EAB_TYPE_CONTACT_DISPLAY, EABContactDisplay))
#define EAB_CONTACT_DISPLAY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EAB_TYPE_CONTACT_DISPLAY, EABContactDisplayClass))
#define IS_EAB_CONTACT_DISPLAY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAB_TYPE_CONTACT_DISPLAY))
#define IS_EAB_CONTACT_DISPLAY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EAB_TYPE_CONTACT_DISPLAY))

typedef struct _EABContactDisplay EABContactDisplay;
typedef struct _EABContactDisplayPrivate EABContactDisplayPrivate;
typedef struct _EABContactDisplayClass EABContactDisplayClass;

typedef enum {
	EAB_CONTACT_DISPLAY_RENDER_NORMAL,  /* for use in the preview pane */
	EAB_CONTACT_DISPLAY_RENDER_COMPACT  /* for use with embedded vcards (e.g, the EABVCardControl) */
} EABContactDisplayRenderMode;

struct _EABContactDisplay {
	GtkHTML parent;
	
	EABContactDisplayPrivate *priv;
};

struct _EABContactDisplayClass {
	GtkHTMLClass parent_class;
};

GtkType        eab_contact_display_get_type    (void);
GtkWidget *    eab_contact_display_new         (void);

void           eab_contact_display_render      (EABContactDisplay *display, EContact *contact,
						EABContactDisplayRenderMode render_mode);

#endif /* _EAB_CONTACT_DISPLAY_H_ */
