/*
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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EAB_CONTACT_DISPLAY_H
#define EAB_CONTACT_DISPLAY_H

#include <libebook/libebook.h>

#include <e-util/e-util.h>

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
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EAB_TYPE_CONTACT_DISPLAY, EABContactDisplayClass))

G_BEGIN_DECLS

typedef struct _EABContactDisplay EABContactDisplay;
typedef struct _EABContactDisplayClass EABContactDisplayClass;
typedef struct _EABContactDisplayPrivate EABContactDisplayPrivate;

/**
 * EABContactDisplayMode:
 * @EAB_CONTACT_DISPLAY_RENDER_NORMAL:
 *   For use in the preview pane.
 * @EAB_CONTACT_DISPLAY_RENDER_COMPACT:
 *   For use with embedded vcards.
 **/
typedef enum {
	EAB_CONTACT_DISPLAY_RENDER_NORMAL,
	EAB_CONTACT_DISPLAY_RENDER_COMPACT
} EABContactDisplayMode;

struct _EABContactDisplay {
	EWebView parent;
	EABContactDisplayPrivate *priv;
};

struct _EABContactDisplayClass {
	EWebViewClass parent_class;

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
GtkOrientation	eab_contact_display_get_orientation
						(EABContactDisplay *display);
void		eab_contact_display_set_orientation
						(EABContactDisplay *display,
						 GtkOrientation orientation);
gboolean	eab_contact_display_get_show_maps
						(EABContactDisplay *display);
void		eab_contact_display_set_show_maps
						(EABContactDisplay *display,
						 gboolean display_maps);

G_END_DECLS

#endif /* EAB_CONTACT_DISPLAY_H */
