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
 *		Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SELECT_NAMES_EDITABLE_H
#define E_SELECT_NAMES_EDITABLE_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_SELECT_NAMES_EDITABLE \
	(e_select_names_editable_get_type ())
#define E_SELECT_NAMES_EDITABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SELECT_NAMES_EDITABLE, ESelectNamesEditable))
#define E_SELECT_NAMES_EDITABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SELECT_NAMES_EDITABLE, ESelectNamesEditableClass))
#define E_IS_SELECT_NAMES_EDITABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SELECT_NAMES_EDITABLE))
#define E_IS_SELECT_NAMES_EDITABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SELECT_NAMES_EDITABLE))
#define E_SELECT_NAMES_EDITABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SELECT_NAMES_EDITABLE, ESelectNamesEditableClass))

G_BEGIN_DECLS

typedef struct _ESelectNamesEditable ESelectNamesEditable;
typedef struct _ESelectNamesEditableClass ESelectNamesEditableClass;
typedef struct _ESelectNamesEditablePrivate ESelectNamesEditablePrivate;

struct _ESelectNamesEditable {
	ENameSelectorEntry parent;
	ESelectNamesEditablePrivate *priv;
};

struct _ESelectNamesEditableClass {
	ENameSelectorEntryClass parent_class;
};

GType		e_select_names_editable_get_type
						(void) G_GNUC_CONST;
GtkWidget *	e_select_names_editable_new	(EClientCache *client_cache);
EDestination *	e_select_names_editable_get_destination
						(ESelectNamesEditable *esne);
gchar *		e_select_names_editable_get_email
						(ESelectNamesEditable *esne);
GList *		e_select_names_editable_get_emails
						(ESelectNamesEditable *esne);
gchar *		e_select_names_editable_get_name
						(ESelectNamesEditable *esne);
GList *		e_select_names_editable_get_names
						(ESelectNamesEditable *esne);
void		e_select_names_editable_set_address
						(ESelectNamesEditable *esne,
						 const gchar *name,
						 const gchar *email);

G_END_DECLS

#endif /* E_SELECT_NAMES_EDITABLE_H */
