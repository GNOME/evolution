/*
 * e-mail-label-action.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is a toggle action whose menu item shows a checkbox, icon and
 * label.  Use of this thing for anything besides the label submenu in
 * the message list popup menu is discouraged, which is why this class
 * was not given a more generic name. */

#ifndef E_MAIL_LABEL_ACTION_H
#define E_MAIL_LABEL_ACTION_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_LABEL_ACTION \
	(e_mail_label_action_get_type ())
#define E_MAIL_LABEL_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_LABEL_ACTION, EMailLabelAction))
#define E_MAIL_LABEL_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_LABEL_ACTION, EMailLabelActionClass))
#define E_IS_MAIL_LABEL_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_LABEL_ACTION))
#define E_IS_MAIL_LABEL_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_LABEL_ACTION))
#define E_MAIL_LABEL_ACTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_LABEL_ACTION, EMailLabelActionClass))

G_BEGIN_DECLS

typedef struct _EMailLabelAction EMailLabelAction;
typedef struct _EMailLabelActionClass EMailLabelActionClass;
typedef struct _EMailLabelActionPrivate EMailLabelActionPrivate;

struct _EMailLabelAction {
	GtkToggleAction parent;
	EMailLabelActionPrivate *priv;
};

struct _EMailLabelActionClass {
	GtkToggleActionClass parent_class;
};

GType		e_mail_label_action_get_type	(void);
EMailLabelAction *
		e_mail_label_action_new		(const gchar *name,
						 const gchar *label,
						 const gchar *tooltip,
						 const gchar *stock_id);

#endif /* E_MAIL_LABEL_ACTION_H */
