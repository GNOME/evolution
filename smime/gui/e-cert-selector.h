/*
 *
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
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CERT_SELECTOR_H
#define E_CERT_SELECTOR_H

#include <gtk/gtk.h>

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define E_TYPE_CERT_SELECTOR			(e_cert_selector_get_type ())
#define E_CERT_SELECTOR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CERT_SELECTOR, ECertSelector))
#define E_CERT_SELECTOR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CERT_SELECTOR, ECertSelectorClass))
#define E_IS_CERT_SELECTOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CERT_SELECTOR))
#define E_IS_CERT_SELECTOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CERT_SELECTOR))

typedef struct _ECertSelector        ECertSelector;
typedef struct _ECertSelectorClass   ECertSelectorClass;

struct _ECertSelector {
	GtkDialog parent;

	struct _ECertSelectorPrivate *priv;
};

struct _ECertSelectorClass {
	GtkDialogClass parent_class;

	void (*selected)(ECertSelector *, const gchar *certid);
};

enum _e_cert_selector_type {
	E_CERT_SELECTOR_SIGNER,
	E_CERT_SELECTOR_RECIPIENT
};

GType      e_cert_selector_get_type (void);
GtkWidget *e_cert_selector_new      (gint type, const gchar *currentid);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* E_CERT_SELECTOR_H */
