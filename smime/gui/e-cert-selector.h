/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cert-selector.h
 *
 * Copyright (C) 2003 Novell Inc.
 *
 * Authors: Michael Zucchi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef E_CERT_SELECTOR_H
#define E_CERT_SELECTOR_H

#include <gtk/gtkdialog.h>

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define E_TYPE_CERT_SELECTOR			(e_cert_selector_get_type ())
#define E_CERT_SELECTOR(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_CERT_SELECTOR, ECertSelector))
#define E_CERT_SELECTOR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CERT_SELECTOR, ECertSelectorClass))
#define E_IS_CERT_SELECTOR(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_CERT_SELECTOR))
#define E_IS_CERT_SELECTOR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_CERT_SELECTOR))

typedef struct _ECertSelector        ECertSelector;
typedef struct _ECertSelectorClass   ECertSelectorClass;

struct _ECertSelector {
	GtkDialog parent;

	struct _ECertSelectorPrivate *priv;
};

struct _ECertSelectorClass {
	GtkDialogClass parent_class;

	void (*selected)(ECertSelector *, const char *certid);
};

enum _e_cert_selector_type {
	E_CERT_SELECTOR_SIGNER,
	E_CERT_SELECTOR_RECIPIENT,
};

GtkType    e_cert_selector_get_type (void);
GtkWidget *e_cert_selector_new      (int type, const char *currentid);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* E_CERT_SELECTOR_H */
