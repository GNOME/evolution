/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Original Author: Jay Painter
 *  Modified: Jeffrey Stedfast
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


#ifndef __E_MESSAGE_BOX_H__
#define __E_MESSAGE_BOX_H__

#include <gnome.h>

BEGIN_GNOME_DECLS

#define E_TYPE_MESSAGE_BOX            (e_message_box_get_type ())
#define E_MESSAGE_BOX(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_MESSAGE_BOX, EMessageBox))
#define E_MESSAGE_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MESSAGE_BOX, EMessageBoxClass))
#define E_IS_MESSAGE_BOX(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_MESSAGE_BOX))
#define E_IS_MESSAGE_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_MESSAGE_BOX))
#define E_MESSAGE_BOX_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), E_TYPE_MESSAGE_BOX, EMessageBoxClass))


#define E_MESSAGE_BOX_INFO      "info"
#define E_MESSAGE_BOX_WARNING   "warning"
#define E_MESSAGE_BOX_ERROR     "error"
#define E_MESSAGE_BOX_QUESTION  "question"
#define E_MESSAGE_BOX_GENERIC   "generic"


typedef struct _EMessageBox        EMessageBox;
typedef struct _EMessageBoxPrivate EMessageBoxPrivate;
typedef struct _EMessageBoxClass   EMessageBoxClass;
typedef struct _EMessageBoxButton  EMessageBoxButton;

struct _EMessageBox
{
	GnomeDialog dialog;
	/*< private >*/
	EMessageBoxPrivate *_priv;
};

struct _EMessageBoxClass
{
	GnomeDialogClass parent_class;
};


guint      e_message_box_get_type   (void) G_GNUC_CONST;
GtkWidget* e_message_box_new        (const gchar *message,
				     const gchar *messagebox_type,
				     ...);

GtkWidget* e_message_box_newv       (const gchar *message,
				     const gchar *messagebox_type,
				     const gchar **buttons);

void       e_message_box_construct  (EMessageBox *messagebox,
				     const gchar *message,
				     const gchar *messagebox_type,
				     const gchar **buttons);

GtkWidget *e_message_box_get_label  (EMessageBox *messagebox);

GtkWidget *e_message_box_get_checkbox  (EMessageBox *messagebox);

END_GNOME_DECLS

#endif /* __E_MESSAGE_BOX_H__ */
