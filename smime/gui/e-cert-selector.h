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
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CERT_SELECTOR_H
#define E_CERT_SELECTOR_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CERT_SELECTOR \
	(e_cert_selector_get_type ())
#define E_CERT_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CERT_SELECTOR, ECertSelector))
#define E_CERT_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CERT_SELECTOR, ECertSelectorClass))
#define E_IS_CERT_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CERT_SELECTOR))
#define E_IS_CERT_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CERT_SELECTOR))
#define E_CERT_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CERT_SELECTOR, ECertSelectorClass))

G_BEGIN_DECLS

typedef struct _ECertSelector ECertSelector;
typedef struct _ECertSelectorClass ECertSelectorClass;
typedef struct _ECertSelectorPrivate ECertSelectorPrivate;

struct _ECertSelector {
	GtkDialog parent;
	ECertSelectorPrivate *priv;
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

G_END_DECLS

#endif /* E_CERT_SELECTOR_H */

