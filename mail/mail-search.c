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
#include "e-searching-tokenizer.h"
#include <gal/widgets/e-unicode.h>
#include <gtkhtml/gtkhtml-search.h>
#include <gtkhtml/htmlengine.h>

static GtkObjectClass *parent_class;

static void
mail_search_destroy (GtkObject *obj)
{
	MailSearch *ms = MAIL_SEARCH (obj);

	gtk_signal_disconnect (GTK_OBJECT (ms->mail->html->engine->ht),
			       ms->match_handler);
	gtk_signal_disconnect (GTK_OBJECT (ms->mail->html->engine->ht),
			       ms->begin_handler);

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
 *  Convenience
 */

static ESearchingTokenizer *
mail_search_tokenizer (MailSearch *ms)
{
	return E_SEARCHING_TOKENIZER (ms->mail->html->engine->ht);
}

static void
mail_search_redisplay_message (MailSearch *ms)
{
	mail_display_redisplay (ms->mail, FALSE);
}

/*
 *  Construct Objects
 */

static void
toggled_case_cb (GtkToggleButton *b, MailSearch *ms)
{
	ms->case_sensitive = gtk_toggle_button_get_active (b);

	e_searching_tokenizer_set_primary_case_sensitivity (mail_search_tokenizer (ms),
							    ms->case_sensitive);
	mail_search_redisplay_message (ms);
	
}

static void
toggled_fwd_cb (GtkToggleButton *b, MailSearch *ms)
{
	ms->search_forward = gtk_toggle_button_get_active (b);
}

static void
dialog_clicked_cb (GtkWidget *w, gint button_number, MailSearch *ms)
{
	ESearchingTokenizer *st = mail_search_tokenizer (ms);
	
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

				e_searching_tokenizer_set_primary_search_string (st, search_text);
				e_searching_tokenizer_set_primary_case_sensitivity (st, ms->case_sensitive);

				mail_search_redisplay_message (ms);

				if (gtk_html_engine_search (ms->mail->html, search_text,
							    ms->case_sensitive, ms->search_forward,
							    FALSE)) {
					ms->last_search = g_strdup (search_text);
				}
			}
		}

	
		g_free (search_text);

	} else if (button_number == 1) { /* "Close"  */

		e_searching_tokenizer_set_primary_search_string (st, NULL);
		mail_search_redisplay_message (ms);

		gtk_widget_destroy (w);

	}
}

static void
begin_cb (ESearchingTokenizer *st, gchar *foo, MailSearch *ms)
{
	gtk_label_set_text (GTK_LABEL (ms->count_label), "0");
	
#ifdef SUBJECT_IN_DIALOG
	if (ms->mail->current_message->subject && *ms->mail->current_message->subject) {
		gchar *msg_subject = e_utf8_to_gtk_string (GTK_WIDGET (ms->msg_label), ms->mail->current_message->subject);
		gtk_label_set_text (GTK_LABEL (ms->msg_label), msg_subject); /* Use the converted string */
		g_free (msg_subject);
	} else {
		gtk_label_set_text (GTK_LABEL (ms->msg_label), _("(Untitled Message)"));
	}
#endif

}

static void
match_cb (ESearchingTokenizer *st, MailSearch *ms)
{
	gchar buf[16];
	g_snprintf (buf, 16, "%d", e_searching_tokenizer_match_count (st));
	gtk_label_set_text (GTK_LABEL (ms->count_label), buf);
}

void
mail_search_construct (MailSearch *ms, MailDisplay *mail)
{
	const gchar *buttons[] = { N_("Search"),
				   GNOME_STOCK_BUTTON_CLOSE,
				   NULL };
	gchar *title = NULL;

	GtkWidget *find_hbox;
	GtkWidget *matches_hbox;
	GtkWidget *toggles_hbox;

	GtkWidget *entry;
	GtkWidget *count_label;
	GtkWidget *case_check;
	GtkWidget *fwd_check;

#ifdef SUBJECT_IN_DIALOG
	gchar *utf8_subject;
	gchar *msg_subject;
	GtkWidget *msg_hbox;
	GtkWidget *msg_label;
#endif

	g_return_if_fail (ms != NULL && IS_MAIL_SEARCH (ms));
	g_return_if_fail (mail != NULL && IS_MAIL_DISPLAY (mail));

	/* Basic set-up */

	ms->mail = mail;
	gtk_object_ref (GTK_OBJECT (mail));

	title = g_strdup (_("Find in Message")); 
	
#ifdef SUBJECT_IN_DIALOG
	if (mail->current_message->subject && *mail->current_message->subject)
		utf8_subject = g_strdup (mail->current_message->subject);
	else
		utf8_subject = g_strdup (_("(Untitled Message)"));
#endif
	
	gnome_dialog_constructv (GNOME_DIALOG (ms), title, buttons);
	g_free (title);

	ms->search_forward = TRUE;
	ms->case_sensitive = FALSE;

	ms->begin_handler = gtk_signal_connect (GTK_OBJECT (ms->mail->html->engine->ht),
						"begin",
						GTK_SIGNAL_FUNC (begin_cb),
						ms);
	ms->match_handler = gtk_signal_connect (GTK_OBJECT (ms->mail->html->engine->ht),
						"match",
						GTK_SIGNAL_FUNC (match_cb),
						ms);

	/* Construct the dialog contents. */
	
	/* msg_hbox     = gtk_hbox_new (FALSE, 0); */
	find_hbox    = gtk_hbox_new (FALSE, 0);
	matches_hbox = gtk_hbox_new (FALSE, 0);
	toggles_hbox = gtk_hbox_new (FALSE, 0);

	entry       = gtk_entry_new ();
	count_label = gtk_label_new ("0");

#ifdef SUBJECT_IN_DIALOG
	msg_label   = gtk_label_new ("");	
	msg_subject = e_utf8_to_gtk_string (GTK_WIDGET (msg_label), utf8_subject); 
	gtk_label_set_text (GTK_LABEL (msg_label), msg_subject); /* Use converted string instead */
	g_free (utf8_subject);
	g_free (msg_subject);
#endif

	case_check  = gtk_check_button_new_with_label (_("Case Sensitive"));
	fwd_check   = gtk_check_button_new_with_label (_("Search Forward"));

	ms->entry       = entry;
	ms->count_label = count_label;

#ifdef SUBJECT_IN_DIALOG
	ms->msg_label   = msg_label;
#endif

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fwd_check),  ms->search_forward);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (case_check), ms->case_sensitive);
#ifdef SUBJECT_IN_DIALOG
	/* gtk_box_pack_start (GTK_BOX (msg_hbox), gtk_label_new (_("Message subject:")), FALSE, FALSE, 3); */
	gtk_box_pack_start (GTK_BOX (msg_hbox), msg_label, TRUE, TRUE, 0);
#endif
	gtk_box_pack_start (GTK_BOX (find_hbox), gtk_label_new (_("Find:")), FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (find_hbox), entry, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (matches_hbox), gtk_hbox_new (FALSE, 0), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (matches_hbox), gtk_label_new (_("Matches:")), FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (matches_hbox), count_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (matches_hbox), gtk_hbox_new (FALSE, 0), TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (toggles_hbox), case_check, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (toggles_hbox), fwd_check,  FALSE, FALSE, 4);

#ifdef SUBJECT_IN_DIALOG
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ms)->vbox), msg_hbox, TRUE, TRUE, 0);
#endif
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ms)->vbox), find_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ms)->vbox), matches_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ms)->vbox), toggles_hbox, TRUE, TRUE, 0);

	gtk_widget_grab_focus (entry); /* Give focus to entry by default */ 
	gnome_dialog_set_default (GNOME_DIALOG (ms), 0); 
	gnome_dialog_editable_enters (GNOME_DIALOG (ms), GTK_EDITABLE(entry)); /* Make <enter> run the search */

#ifdef SUBJECT_IN_DIALOG
	gtk_widget_show_all (msg_hbox);
#endif
	gtk_widget_show_all (find_hbox);
	gtk_widget_show_all (matches_hbox);
	gtk_widget_show_all (toggles_hbox);

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
	gtk_signal_connect_object (GTK_OBJECT (ms->mail),
				   "destroy",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (ms));
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

