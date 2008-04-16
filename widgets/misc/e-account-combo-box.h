/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_ACCOUNT_COMBO_BOX_H
#define E_ACCOUNT_COMBO_BOX_H

#include <gtk/gtk.h>
#include <camel/camel-session.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

/* Standard GObject macros */
#define E_TYPE_ACCOUNT_COMBO_BOX \
	(e_account_combo_box_get_type ())
#define E_ACCOUNT_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACCOUNT_COMBO_BOX, EAccountComboBox))
#define E_ACCOUNT_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACCOUNT_COMBO_BOX, EAccountComboBoxClass))
#define E_IS_ACCOUNT_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACCOUNT_COMBO_BOX))
#define E_IS_ACCOUNT_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACCOUNT_COMBO_BOX))
#define E_ACCOUNT_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACCOUNT_COMBO_BOX, EAccountComboBoxClass))

G_BEGIN_DECLS

typedef struct _EAccountComboBox EAccountComboBox;
typedef struct _EAccountComboBoxClass EAccountComboBoxClass;
typedef struct _EAccountComboBoxPrivate EAccountComboBoxPrivate;

struct _EAccountComboBox {
	GtkComboBox parent;
	EAccountComboBoxPrivate *priv;
};

struct _EAccountComboBoxClass {
	GtkComboBoxClass parent_class;
};

GType		e_account_combo_box_get_type	(void);
GtkWidget *	e_account_combo_box_new		(void);
void		e_account_combo_box_set_session	(CamelSession *session);
EAccountList *	e_account_combo_box_get_account_list
						(EAccountComboBox *combo_box);
void		e_account_combo_box_set_account_list
						(EAccountComboBox *combo_box,
						 EAccountList *account_list);
EAccount *	e_account_combo_box_get_active	(EAccountComboBox *combo_box);
gboolean	e_account_combo_box_set_active	(EAccountComboBox *combo_box,
						 EAccount *account);
const gchar *	e_account_combo_box_get_active_name
						(EAccountComboBox *combo_box);
gboolean	e_account_combo_box_set_active_name
						(EAccountComboBox *combo_box,
						 const gchar *account_name);

G_END_DECLS

#endif /* E_ACCOUNT_COMBO_BOX_H */
