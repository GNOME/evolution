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


#include <config.h>
#include <stdarg.h>
#include <string.h> /* for strcmp */

#include "e-messagebox.h"

#define E_MESSAGE_BOX_WIDTH  425
#define E_MESSAGE_BOX_HEIGHT 125

struct _EMessageBoxPrivate {
	char *id;
	GtkWidget *label;
	GtkWidget *checkbox;
};

static void e_message_box_class_init (EMessageBoxClass *klass);
static void e_message_box_init       (EMessageBox *messagebox);
static void e_message_box_destroy    (GtkObject *object);
static void e_message_box_finalize   (GtkObject *object);

static GnomeDialogClass *parent_class;

GtkType
e_message_box_get_type (void)
{
	static GtkType message_box_type = 0;
	
	if (!message_box_type) {
		GtkTypeInfo message_box_info = {
			"EMessageBox",
			sizeof (EMessageBox),
			sizeof (EMessageBoxClass),
			(GtkClassInitFunc) e_message_box_class_init,
			(GtkObjectInitFunc) e_message_box_init,
			NULL,
			NULL
		};
		
		message_box_type = gtk_type_unique (gnome_dialog_get_type (), &message_box_info);
	}
	
	return message_box_type;
}

static void
e_message_box_class_init (EMessageBoxClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)klass;
	parent_class = gtk_type_class (gnome_dialog_get_type ());
	
	object_class->destroy = e_message_box_destroy;
	object_class->finalize = e_message_box_finalize;
}

static void
e_message_box_init (EMessageBox *message_box)
{
	message_box->_priv = g_new0 (EMessageBoxPrivate, 1); 
}

static void
e_message_box_destroy (GtkObject *object)
{
	/* remember, destroy can be run multiple times! */
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
e_message_box_finalize (GtkObject *object)
{
	EMessageBox *mbox = E_MESSAGE_BOX (object);
	
	g_free (mbox->_priv->id);
	
	g_free (mbox->_priv);
	mbox->_priv = NULL;
	
	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/**
 * e_message_box_construct:
 * @messagebox: The message box to construct
 * @message: The message to be displayed.
 * @message_box_type: The type of the message
 * @buttons: a NULL terminated array with the buttons to insert.
 *
 * For language bindings or subclassing, from C use #e_message_box_new or
 * #e_message_box_newv
 *
 * Returns:
 */
void
e_message_box_construct (EMessageBox *messagebox,
			 const gchar *message,
			 const gchar *message_box_type,
			 const gchar **buttons)
{
	GtkWidget *hbox;
	GtkWidget *pixmap = NULL;
	GtkWidget *alignment;
	char *s;
	GtkStyle *style;
        const gchar* title = NULL;
	gint i = 0;
	
	g_return_if_fail (messagebox != NULL);
	g_return_if_fail (E_IS_MESSAGE_BOX (messagebox));
	g_return_if_fail (message != NULL);
	g_return_if_fail (message_box_type != NULL);
	
	style = gtk_widget_get_style (GTK_WIDGET (messagebox));
	
	/* Make noises, basically */
	gnome_triggers_vdo (message, message_box_type, NULL);
	
	if (strcmp (E_MESSAGE_BOX_INFO, message_box_type) == 0) {
                title = _("Information");
		s = gnome_unconditional_pixmap_file("gnome-info.png");
		if (s) {
                        pixmap = gnome_pixmap_new_from_file (s);
                        g_free(s);
                }
	} else if (strcmp (E_MESSAGE_BOX_WARNING, message_box_type) == 0) {
                title = _("Warning");
		s = gnome_unconditional_pixmap_file ("gnome-warning.png");
		if (s) {
                        pixmap = gnome_pixmap_new_from_file (s);
                        g_free (s);
                }
	} else if (strcmp (E_MESSAGE_BOX_ERROR, message_box_type) == 0) {
                title = _("Error");
		s = gnome_unconditional_pixmap_file ("gnome-error");
		if (s) {
                        pixmap = gnome_pixmap_new_from_file (s);
                        g_free(s);
                }
	} else if (strcmp (E_MESSAGE_BOX_QUESTION, message_box_type) == 0) {
                title = _("Question");
		s = gnome_unconditional_pixmap_file ("gnome-question.png");
		if (s) {
                        pixmap = gnome_pixmap_new_from_file (s);
                        g_free (s);
                }
	} else {
                title = _("Message");
	}
	
        g_assert (title != NULL);
	
	gtk_window_set_title (GTK_WINDOW (messagebox), title);
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (messagebox)->vbox),
			    hbox, TRUE, TRUE, 10);
	gtk_widget_show (hbox);
	
	if ((pixmap == NULL) || (GNOME_PIXMAP (pixmap)->pixmap == NULL)) {
        	if (pixmap)
			gtk_widget_destroy (pixmap);
		s = gnome_unconditional_pixmap_file ("gnome-default.png");
         	if (s) {
			pixmap = gnome_pixmap_new_from_file (s);
                        g_free (s);
                } else
			pixmap = NULL;
	}
	if (pixmap) {
		gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, TRUE, 0);
		gtk_widget_show (pixmap);
	}
	
	messagebox->_priv->label = gtk_label_new (message);
	gtk_label_set_justify (GTK_LABEL (messagebox->_priv->label), GTK_JUSTIFY_LEFT);
        gtk_label_set_line_wrap (GTK_LABEL (messagebox->_priv->label), TRUE);
	gtk_misc_set_padding (GTK_MISC (messagebox->_priv->label), GNOME_PAD, 0);
	gtk_box_pack_start (GTK_BOX (hbox), messagebox->_priv->label, TRUE, TRUE, 0);
	gtk_widget_show (messagebox->_priv->label);
	
	/* Add some extra space on the right to balance the pixmap */
	if (pixmap) {
		alignment = gtk_alignment_new (0., 0., 0., 0.);
		gtk_widget_set_usize (alignment, GNOME_PAD, -1);
		gtk_widget_show (alignment);
		
		gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);
	}
	
	/* Add the "Don't show this message again." checkbox */
	messagebox->_priv->checkbox = gtk_check_button_new_with_label (_("Don't show this message again."));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (messagebox)->vbox),
			    messagebox->_priv->checkbox, TRUE, TRUE, 10);
	gtk_widget_show (messagebox->_priv->checkbox);
	
	if (buttons) {
		while (buttons[i]) {
			gnome_dialog_append_button (GNOME_DIALOG (messagebox), 
						    buttons[i]);
			i++;
		};
	}
	
	if (GNOME_DIALOG (messagebox)->buttons)
		gtk_widget_grab_focus (g_list_last (GNOME_DIALOG (messagebox)->buttons)->data);
	
	gnome_dialog_set_close (GNOME_DIALOG (messagebox), TRUE);
}

/**
 * e_message_box_new:
 * @message: The message to be displayed.
 * @message_box_type: The type of the message
 * @...: A NULL terminated list of strings to use in each button.
 *
 * Creates a dialog box of type @message_box_type with @message.  A number
 * of buttons are inserted on it.  You can use the GNOME stock identifiers
 * to create gnome-stock-buttons.
 *
 * Returns a widget that has the dialog box.
 */
GtkWidget*
e_message_box_new (const gchar *message,
		   const gchar *message_box_type, ...)
{
	va_list ap;
	EMessageBox *message_box;
	
	g_return_val_if_fail (message != NULL, NULL);
	g_return_val_if_fail (message_box_type != NULL, NULL);
        
	va_start (ap, message_box_type);
	
	message_box = gtk_type_new (e_message_box_get_type ());
	
	e_message_box_construct (message_box, message, message_box_type, NULL);
	
	/* we need to add buttons by hand here */
	while (TRUE) {
		gchar * button_name;
		
		button_name = va_arg (ap, gchar *);
		
		if (button_name == NULL) {
			break;
		}
		
		gnome_dialog_append_button (GNOME_DIALOG (message_box), button_name);
	}
	
	va_end (ap);
	
	gtk_widget_grab_focus (g_list_last (GNOME_DIALOG (message_box)->buttons)->data);
	
	return GTK_WIDGET (message_box);
}

/**
 * e_message_box_newv:
 * @message: The message to be displayed.
 * @message_box_type: The type of the message
 * @buttons: a NULL terminated array with the buttons to insert.
 *
 * Creates a dialog box of type @message_box_type with @message.  A number
 * of buttons are inserted on it, the messages come from the @buttons array.
 * You can use the GNOME stock identifiers to create gnome-stock-buttons.
 * The buttons array can be NULL if you wish to add buttons yourself later.
 *
 * Returns a widget that has the dialog box.
 */
GtkWidget*
e_message_box_newv (const gchar *message,
		    const gchar *message_box_type,
		    const gchar **buttons)
{
	EMessageBox *message_box;
	
	g_return_val_if_fail (message != NULL, NULL);
	g_return_val_if_fail (message_box_type != NULL, NULL);
	
	message_box = gtk_type_new (e_message_box_get_type ());
	
	e_message_box_construct (message_box, message,
				     message_box_type, buttons);
	
	return GTK_WIDGET (message_box);
}


/**
 * e_message_box_get_label:
 * @messagebox: The message box to work on
 *
 * Gets the label widget of the message box.  You should use this
 * function instead of using the structure directly.
 *
 * Returns: the widget of the label with the message */
GtkWidget *
e_message_box_get_label (EMessageBox *messagebox)
{
	g_return_val_if_fail (messagebox != NULL, NULL);
	g_return_val_if_fail (E_IS_MESSAGE_BOX (messagebox), NULL);
	
	return messagebox->_priv->label;
}


/**
 * e_message_box_get_checkbox:
 * @messagebox: The message box to work on
 *
 * Gets the checkbox widget of the message box.  You should use this
 * function instead of using the structure directly.
 *
 * Returns: the checkbox widget */
GtkWidget *
e_message_box_get_checkbox (EMessageBox *messagebox)
{
	g_return_val_if_fail (messagebox != NULL, NULL);
	g_return_val_if_fail (E_IS_MESSAGE_BOX (messagebox), NULL);
	
	return messagebox->_priv->checkbox;
}
