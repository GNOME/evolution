/*
 * mail-display.c: Mail display widget
 *
 * Author:
 *   Miguel de Icaza
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "e-util/e-util.h"
#include "mail-display.h"
#include "html-stream.h"

#define PARENT_TYPE (gtk_table_get_type ())

static GtkObjectClass *mail_display_parent_class;

static void
mail_display_init (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	mail_display->html =  (GtkHTML *) gtk_html_new ();
	gtk_widget_show (GTK_WIDGET (mail_display->html));
}

static void
mail_display_destroy (GtkObject *object)
{
	MailDisplay *mail_display = MAIL_DISPLAY (object);

	mail_display_parent_class->destroy (object);
}

static void
mail_display_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = mail_display_destroy;
	mail_display_parent_class = gtk_type_class (PARENT_TYPE);
}

GtkWidget *
mail_display_new (void)
{
	MailDisplay *mail_display = gtk_type_new (mail_display_get_type ());
	GtkTable *table = GTK_TABLE (mail_display);
	
	table->homogeneous = FALSE;
	gtk_table_resize (table, 1, 2);

	gtk_table_attach (table, GTK_WIDGET (mail_display->html),
			  0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	return GTK_WIDGET (mail_display);
}

CamelStream *
mail_display_get_stream (MailDisplay *mail_display)
{
	g_return_val_if_fail (mail_display != NULL, NULL);
	g_return_val_if_fail (IS_MAIL_DISPLAY (mail_display), NULL);

	return html_stream_new (mail_display->html);
}

E_MAKE_TYPE (mail_display, "MailDisplay", MailDisplay, mail_display_class_init, mail_display_init, PARENT_TYPE);


