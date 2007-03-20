/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: R.Raghavendran <raghavguru7@gmail.com>
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Bangalore, MA 02111-1307, India.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtk.h>
#include <libgnome/libgnome.h>
#include <glib/gi18n.h>
#include <glade/glade.h>

#include "e-util/e-util-private.h"

#include "exchange-send-options.h"

struct _ExchangeSendOptionsDialogPrivate {
	/* Glade XML data */
	GladeXML *xml;

	/*Widgets*/

	GtkWidget *main;
	
	/*Importance*/
	GtkWidget *importance;

	/*Sensitivity*/
	GtkWidget *sensitivity;

	/*Read Receipt*/
	GtkWidget *read_receipt;

	/*Delivery Receipt*/
	GtkWidget *delivery_receipt;
	
	/*Label Widgets*/
	GtkWidget *importance_label;
	GtkWidget *sensitivity_label;
	char *help_section;
};

static void exchange_sendoptions_dialog_class_init (GObjectClass *object_class);
static void exchange_sendoptions_dialog_finalize (GObject *object);
static void exchange_sendoptions_dialog_init (GObject *object);
static void exchange_sendoptions_dialog_dispose (GObject *object);

static GObjectClass *parent_class = NULL;
enum {
	SOD_RESPONSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};


static void
exchange_send_options_get_widgets_data (ExchangeSendOptionsDialog *sod)
{
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptions *options;
	
	priv = sod->priv;
	options = sod->options;
	
	options->importance = gtk_combo_box_get_active ((GtkComboBox *)priv->importance);
	options->sensitivity = gtk_combo_box_get_active ((GtkComboBox *)priv->sensitivity);

	options->delivery_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->delivery_receipt));
	options->read_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->read_receipt));

}

static gboolean
get_widgets (ExchangeSendOptionsDialog *sod) 
{
	ExchangeSendOptionsDialogPrivate *priv;

	priv = sod->priv;

#define EXCHANGE(name) glade_xml_get_widget (priv->xml, name)

	priv->main = EXCHANGE ("send_options");
	if (!priv->main)
		return FALSE;
	
	priv->importance = EXCHANGE ("imp_combo_box");
	priv->sensitivity = EXCHANGE ("sensitivity_combo_box");
	priv->read_receipt = EXCHANGE ("read_check_button");
	priv->delivery_receipt = EXCHANGE ("delivery_check_button");
	priv->importance_label = EXCHANGE ("Importance_label");
	priv->sensitivity_label = EXCHANGE ("Sensitivity_label");

#undef EXCHANGE

	return (priv->importance
		&&priv->sensitivity
		&&priv->read_receipt
		&&priv->delivery_receipt
		&&priv->importance_label
		&&priv->sensitivity_label);
}
	
static void
exchange_send_options_fill_widgets_with_data (ExchangeSendOptionsDialog *sod)
{
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptions *options;
	priv = sod->priv;
	options = sod->options;

	priv->help_section = g_strdup ("usage-mail");

	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->importance_label), priv->importance);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->sensitivity_label), priv->sensitivity);

	gtk_combo_box_set_active ((GtkComboBox *) priv->importance, options->importance);
	gtk_combo_box_set_active ((GtkComboBox *) priv->sensitivity, options->sensitivity);
	
	if (options->read_enabled) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->read_receipt), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->read_receipt), FALSE);
	if (options->delivery_enabled)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delivery_receipt), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->delivery_receipt), FALSE);
}

ExchangeSendOptionsDialog *
exchange_sendoptions_dialog_new (void) {
	ExchangeSendOptionsDialog *sod;

	sod = g_object_new (EXCHANGE_TYPE_SENDOPTIONS_DIALOG, NULL);
	
	return sod;
}

GType exchange_sendoptions_dialog_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (ExchangeSendOptionsDialogClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc) exchange_sendoptions_dialog_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (ExchangeSendOptionsDialog),
     0,      /* n_preallocs */
     (GInstanceInitFunc) exchange_sendoptions_dialog_init,
 	NULL    /* instance_init */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "ExchangeSendOptionsDialogType",
                                   &info, 0);
  }
  return type;
}

static void exchange_send_options_cb (GtkDialog *dialog, gint state, gpointer func_data)
{
   	ExchangeSendOptionsDialogPrivate *priv;
   	ExchangeSendOptionsDialog *sod;
        GError *error = NULL;     
		
	sod = func_data;
        priv = sod->priv;
	
	switch (state) {
		case GTK_RESPONSE_OK:
		     exchange_send_options_get_widgets_data (sod);	
		case GTK_RESPONSE_CANCEL:
		     gtk_widget_hide (priv->main);
	    	     gtk_widget_destroy (priv->main);
	             g_object_unref (priv->xml);
	             break;	     
		case GTK_RESPONSE_HELP:
		     gnome_help_display_desktop (NULL,
				    "evolution-" BASE_VERSION,
				    "evolution-" BASE_VERSION ".xml",
				    priv->help_section,
				    &error); 	
		     if (error != NULL)
			g_warning ("%s", error->message);
	    	     break;
	}
	g_signal_emit (G_OBJECT (func_data), signals[SOD_RESPONSE], 0, state);

}
	
gboolean 
exchange_sendoptions_dialog_run (ExchangeSendOptionsDialog *sod, GtkWidget *parent)
{
	ExchangeSendOptionsDialogPrivate *priv;
	GtkWidget *toplevel;
	gchar *filename;

	g_return_val_if_fail (sod != NULL || EXCHANGE_IS_SENDOPTIONS_DIALOG (sod), FALSE);
	
	priv = sod->priv;

	filename = g_build_filename (EVOLUTION_GLADEDIR,
				     "exchange-send-options.glade",
				     NULL);
	priv->xml = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	if (!priv->xml) {
		g_message ( G_STRLOC ": Could not load the Glade XML file ");
		return FALSE;
	}

	if (!get_widgets(sod)) {
		g_object_unref (priv->xml);
		g_message (G_STRLOC ": Could not get the Widgets \n");
		return FALSE;
	}

	toplevel =  gtk_widget_get_toplevel (priv->main);
	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (toplevel),
				      GTK_WINDOW (parent));

	exchange_send_options_fill_widgets_with_data (sod);
	
	g_signal_connect (GTK_DIALOG (priv->main), "response", G_CALLBACK(exchange_send_options_cb), sod);
	
	gtk_window_set_modal ((GtkWindow *)priv->main, TRUE);
	gtk_widget_show (priv->main);
       
	return TRUE;
}

static void
exchange_sendoptions_dialog_class_init (GObjectClass *object)
{
	ExchangeSendOptionsDialogClass *klass;
	GObjectClass *object_class;

	klass = EXCHANGE_SENDOPTIONS_DIALOG_CLASS (object);
	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = exchange_sendoptions_dialog_finalize;
	object_class->dispose = exchange_sendoptions_dialog_dispose;
	signals[SOD_RESPONSE] = g_signal_new ("sod_response",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (ExchangeSendOptionsDialogClass, esod_response),
			NULL, NULL,
			g_cclosure_marshal_VOID__INT,
			G_TYPE_NONE, 1,
			G_TYPE_INT);

}

static void
exchange_sendoptions_dialog_init (GObject *object)
{
	
	ExchangeSendOptionsDialog *sod;
	ExchangeSendOptionsDialogPrivate *priv;
	ExchangeSendOptions *new;

	sod = EXCHANGE_SENDOPTIONS_DIALOG (object);
	new = g_new0 (ExchangeSendOptions, 1);
	
	priv = g_new0 (ExchangeSendOptionsDialogPrivate, 1);
	
	sod->priv = priv;
	sod->options = new;
	sod->options->delivery_enabled = FALSE;
	sod->options->read_enabled = FALSE;
	sod->options->importance = E_IMP_NORMAL;
	sod->options->sensitivity = E_SENSITIVITY_NORMAL;

	priv->xml = NULL;
	priv->main = NULL;
	priv->importance = NULL;
	priv->sensitivity = NULL;
	priv->sensitivity_label = NULL;
	priv->importance_label = NULL;
	priv->read_receipt = NULL;
	priv->delivery_receipt = NULL;

}

static void
exchange_sendoptions_dialog_finalize (GObject *object)
{
	ExchangeSendOptionsDialog *sod = (ExchangeSendOptionsDialog *)object;
	ExchangeSendOptionsDialogPrivate *priv;
	
	g_return_if_fail (EXCHANGE_IS_SENDOPTIONS_DIALOG (sod));
	priv = sod->priv;
	
	g_free (priv->help_section);

	if (sod->options) {
		g_free (sod->options);
		sod->options = NULL;
	}

	if (sod->priv) {
		g_free (sod->priv);
		sod->priv = NULL;
	}

	if (parent_class->finalize) 
		(* parent_class->finalize) (object);
	
}

static void
exchange_sendoptions_dialog_dispose (GObject *object)
{
	ExchangeSendOptionsDialog *sod = (ExchangeSendOptionsDialog *) object;

	g_return_if_fail (EXCHANGE_IS_SENDOPTIONS_DIALOG (sod));

	if (parent_class->dispose)
		(* parent_class->dispose) (object);

}	

