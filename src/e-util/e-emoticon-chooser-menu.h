/*
 * SPDX-FileCopyrightText: (C) 2008 Novell, Inc.
 * SPDX-FileCopyrightText: (C) 2012 Dan Vrátil <dvratil@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EMOTICON_CHOOSER_MENU_H
#define E_EMOTICON_CHOOSER_MENU_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_EMOTICON_CHOOSER_MENU \
	(e_emoticon_chooser_menu_get_type ())
#define E_EMOTICON_CHOOSER_MENU(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EMOTICON_CHOOSER_MENU, EEmoticonChooserMenu))
#define E_EMOTICON_CHOOSER_MENU_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EMOTICON_CHOOSER_MENU, EEmoticonChooserMenuClass))
#define E_IS_EMOTICON_CHOOSER_MENU(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EMOTICON_CHOOSER_MENU))
#define E_IS_EMOTICON_CHOOSER_MENU_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EMOTICON_CHOOSER_MENU))
#define E_EMOTICON_CHOOSER_MENU_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EMOTICON_CHOOSER_MENU, EEmoticonChooserMenuClass))

G_BEGIN_DECLS

typedef struct _EEmoticonChooserMenu EEmoticonChooserMenu;
typedef struct _EEmoticonChooserMenuClass EEmoticonChooserMenuClass;
typedef struct _EEmoticonChooserMenuPrivate EEmoticonChooserMenuPrivate;

struct _EEmoticonChooserMenu {
	GtkMenu parent;
};

struct _EEmoticonChooserMenuClass {
	GtkMenuClass parent_class;
};

GType		e_emoticon_chooser_menu_get_type
						(void) G_GNUC_CONST;
GtkWidget *	e_emoticon_chooser_menu_new	(void);

G_END_DECLS

#endif /* E_EMOTICON_CHOOSER_MENU_H */
