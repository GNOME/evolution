/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * mail-search.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mail-search.h"

#include <gal/widgets/e-unicode.h>
#include <gtkhtml/gtkhtml-search.h>

static GtkObjectClass *parent_class;

static void
mail_search_destroy (GtkObject *obj)
{
	MailSearch *ms = MAIL_SEARCH (obj);

	g_free (ms->last_search);
	gtk_object_unref (GTK_OBJECT (ms->mail));
}

static void
mail_search_class_init (MailSearchClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	parent_class = GTK_OBJECT_CLASS (gtk_type_class (gnome_dialog_get_type ()));
	
	object_class->destroy = mail_search_destroy;
}

static void
mail_search_init (MailSearch *ms)
{

}

GtkType
mail_search_get_type (void)
{
	static GtkType mail_search_type = 0;

	if (! mail_search_type) {
		GtkTypeInfo mail_search_info = {
			"MailSearch",
			sizeof (MailSearch),
			sizeof (MailSearchClass),
			(GtkClassInitFunc) mail_search_class_init,
			(GtkObjectInitFunc) mail_search_init,
			NULL, NULL, /* mysteriously reserved */
			(GtkClassInitFunc) NULL
		};

		mail_search_type = gtk_type_unique (gnome_dialog_get_type (), &mail_search_info);
	}

	return mail_search_type;
}

/*
 *  Construct Objects
 */

static void
toggled_case_cb (GtkToggleButton *b, MailSearch *ms)
{
	ms->case_sensitive = gtk_toggle_button_get_active (b);
}

static void
toggled_fwd_cb (GtkToggleButton *b, MailSearch *ms)
{
	ms->search_forward = gtk_toggle_button_get_active (b);
}

static void
dialog_clicked_cb (GtkWidget *w, gint button_number, MailSearch *ms)
{
	if (button_number == 0) {        /* "Search" */

		char *search_text, *tmp;
		
		tmp = gtk_editable_get_chars (GTK_EDITABLE (ms->entry), 0, -1);
		g_strstrip (tmp);
		search_text = e_utf8_from_gtk_string ((GtkWidget *) ms->entry, tmp);
		g_free (tmp);

		if (search_text && *search_text) {
		
			if (ms->last_search && !strcmp (ms->last_search, search_text)) {
				
				if (! gtk_html_engine_search_next (ms->mail->html)) {
					g_free (ms->last_search);
					ms->last_search = NULL;
				}

			} else {
				
				g_free (ms->last_search);
				ms->last_search = NULL;
				
				if (gtk_html_engine_search (ms->mail->html, search_text,
							    ms->case_sensitive, ms->search_forward,
							    FALSE)) {
					ms->last_search = g_strdup (search_text);
				}
			}
		}

		g_free (search_text);

	} else if (button_number == 1) { /* "Close"  */

		gtk_widget_destroy (w);

	}
}


void
mail_search_construct (MailSearch *ms, MailDisplay *mail)
{
	const gchar *buttons[] = { N_("Search"),
				   GNOME_STOCK_BUTTON_CLOSE,
				   NULL };
	gchar *title = NULL;
	GtkWidget *top_hbox;
	GtkWidget *bot_hbox;
	GtkWidget *entry;
	GtkWidget *case_check;
	GtkWidget *fwd_check;

	g_return_if_fail (ms != NULL && IS_MAIL_SEARCH (ms));
	g_return_if_fail (mail != NULL && IS_MAIL_DISPLAY (mail));

	/* Basic set-up */

	ms->mail = mail;
	gtk_object_ref (GTK_OBJECT (mail));

	if (mail->current_message->subject && *mail->current_message->subject)
		title = g_strdup_printf (_("Search \"%s\""), mail->current_message->subject);
	else
		title = g_strdup (_("Search Untitled Message"));

	gnome_dialog_constructv (GNOME_DIALOG (ms), title, buttons);
	g_free (title);

	ms->search_forward = TRUE;
	ms->case_sensitive = FALSE;


	/* Construct the dialog contents. */
	
	top_hbox = gtk_hbox_new (FALSE, 0);
	bot_hbox = gtk_hbox_new (FALSE, 0);

	entry      = gtk_entry_new ();
	case_check = gtk_check_button_new_with_label (_("Case Sensitive"));
	fwd_check  = gtk_check_button_new_with_label (_("Search Forward"));

	ms->entry = entry;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fwd_check),  ms->search_forward);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (case_check), ms->case_sensitive);

	gtk_box_pack_start (GTK_BOX (top_hbox), gtk_label_new (_("Find:")), FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (top_hbox), entry, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (bot_hbox), case_check, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (bot_hbox), fwd_check,  FALSE, FALSE, 4);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ms)->vbox), top_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ms)->vbox), bot_hbox, TRUE, TRUE, 0);

	gtk_widget_show_all (top_hbox);
	gtk_widget_show_all (bot_hbox);


	/* Hook up signals */

	gtk_signal_connect (GTK_OBJECT (case_check),
			    "toggled",
			    GTK_SIGNAL_FUNC (toggled_case_cb),
			    ms);
	gtk_signal_connect (GTK_OBJECT (fwd_check),
			    "toggled",
			    GTK_SIGNAL_FUNC (toggled_fwd_cb),
			    ms);

	gtk_signal_connect (GTK_OBJECT (ms),
			    "clicked",
			    GTK_SIGNAL_FUNC (dialog_clicked_cb),
			    ms);


}

GtkWidget *
mail_search_new (MailDisplay *mail)
{
	gpointer ptr;

	g_return_val_if_fail (mail && IS_MAIL_DISPLAY (mail), NULL);

	ptr = gtk_type_new (mail_search_get_type ());
	mail_search_construct (MAIL_SEARCH (ptr), mail);

	return GTK_WIDGET (ptr);
}

