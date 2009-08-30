/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EAB_CONTACT_DISPLAY_H
#define EAB_CONTACT_DISPLAY_H

#include <gtkhtml/gtkhtml.h>
#include <libebook/e-contact.h>
#include <libebook/e-destination.h>

/* Standard GObject macros */
#define EAB_TYPE_CONTACT_DISPLAY \
	(eab_contact_display_get_type ())
#define EAB_CONTACT_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EAB_TYPE_CONTACT_DISPLAY, EABContactDisplay))
#define EAB_CONTACT_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EAB_TYPE_CONTACT_DISPLAY, EABContactDisplayClass))
#define EAB_IS_CONTACT_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EAB_TYPE_CONTACT_DISPLAY))
#define EAB_IS_CONTACT_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EAB_TYPE_CONTACT_DISPLAY))
#define EAB_CONTACT_DISPLAY_GET_CLASS(obj) \
	(G_TYPE_ISNTANCE_GET_CLASS \
	((obj), EAB_TYPE_CONTACT_DISPLAY, EABContactDisplayClass))

G_BEGIN_DECLS

typedef struct _EABContactDisplay EABContactDisplay;
typedef struct _EABContactDisplayClass EABContactDisplayClass;
typedef struct _EABContactDisplayPrivate EABContactDisplayPrivate;

typedef enum {
	EAB_CONTACT_DISPLAY_RENDER_NORMAL,  /* for use in the preview pane */
	EAB_CONTACT_DISPLAY_RENDER_COMPACT  /* for use with embedded vcards (e.g, the EABVCardControl) */
} EABContactDisplayMode;

struct _EABContactDisplay {
	GtkHTML parent;
	EABContactDisplayPrivate *priv;
};

struct _EABContactDisplayClass {
	GtkHTMLClass parent_class;

	/* Signals */
	void	(*send_message)			(EABContactDisplay *display,
						 EDestination *destination);
};

GType		eab_contact_display_get_type	(void);
GtkWidget *	eab_contact_display_new		(void);

EContact *	eab_contact_display_get_contact	(EABContactDisplay *display);
void		eab_contact_display_set_contact	(EABContactDisplay *display,
						 EContact *contact);
EABContactDisplayMode
		eab_contact_display_get_mode	(EABContactDisplay *display);
void		eab_contact_display_set_mode	(EABContactDisplay *display,
						 EABContactDisplayMode mode);

G_END_DECLS

#endif /* EAB_CONTACT_DISPLAY_H */
