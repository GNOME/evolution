/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jon Trowbridge <trow@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "mail-search.h"
#include "e-searching-tokenizer.h"
#include <gtkhtml/gtkhtml-search.h>
#include <gtkhtml/htmlengine.h>
#include <libgnomeui/gnome-window-icon.h>


static ESearchingTokenizer *mail_search_tokenizer (MailSearch *ms);
static void mail_search_redisplay_message (MailSearch *ms);


static GtkObjectClass *parent_class = NULL;


static void
mail_search_finalise (GObject *obj)
{
	MailSearch *ms = MAIL_SEARCH (obj);

	g_free (ms->last_search);
	g_object_unref (ms->mail);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
mail_search_destroy (GtkObject *obj)
{
	MailSearch *ms = (MailSearch *) obj;
	ESearchingTokenizer *st = mail_search_tokenizer (ms);

	if (ms->begin_handler) {
		g_signal_handler_disconnect(ms->mail->html->engine->ht, ms->match_handler);
		g_signal_handler_disconnect(ms->mail->html->engine->ht, ms->begin_handler);
		ms->begin_handler = 0;
		e_searching_tokenizer_set_primary_search_string (st, NULL);
		mail_search_redisplay_message (ms);
	}
	
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
mail_search_class_init (MailSearchClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GtkObjectClass *gtk_object_class = (GtkObjectClass *) klass;
	
	parent_class = (GtkObjectClass *) g_type_class_ref (GTK_TYPE_DIALOG);
	
	object_class->finalize = mail_search_finalise;
	
	gtk_object_class->destroy = mail_search_destroy;
}

static void
mail_search_init (MailSearch *ms)
{
	
}

GtkType
mail_search_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (MailSearchClass),
			NULL, NULL,
			(GClassInitFunc) mail_search_class_init,
			NULL, NULL,
			sizeof (MailSearch),
			0,
			(GInstanceInitFunc) mail_search_init,
		};
		
		type = g_type_register_static (GTK_TYPE_DIALOG, "MailSearch", &info, 0);
	}
	
	return type;
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

static void
mail_search_set_subject (MailSearch *ms, const char *subject)
{
	char *utf8_subject = NULL;
	
	if (subject && *subject) {
		utf8_subject = g_strdup (subject);
		
		if (g_utf8_validate (utf8_subject, -1, NULL)) {
#define ARBITRARY_CUTOFF 40
			if (g_utf8_strlen (utf8_subject, -1) > ARBITRARY_CUTOFF + 3) {
				char *p = g_utf8_offset_to_pointer (utf8_subject, ARBITRARY_CUTOFF);
				
				strcpy (p, "...");
			}
		} else {
			/* If the subject contains bad utf8, don't show anything in the frame label. */
			g_free (utf8_subject);
			utf8_subject = NULL;
		}
	} else {
		utf8_subject = g_strdup (_("(Untitled Message)"));
	}
	
	gtk_frame_set_label (GTK_FRAME (ms->msg_frame), utf8_subject);
	
	g_free (utf8_subject);
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

#if 0
static void
toggled_fwd_cb (GtkToggleButton *b, MailSearch *ms)
{
	ms->search_forward = gtk_toggle_button_get_active (b);
	gtk_html_engine_search_set_forward (ms->mail->html, ms->search_forward);
}
#endif

static void
dialog_response_cb (GtkWidget *widget, int button, MailSearch *ms)
{
	ESearchingTokenizer *st = mail_search_tokenizer (ms);
	
	if (button == GTK_RESPONSE_ACCEPT) {
		char *search_text;
		
		search_text = gtk_editable_get_chars (GTK_EDITABLE (ms->entry), 0, -1);
		g_strstrip (search_text);
		
		if (search_text && *search_text) {
			if (ms->last_search && !strcmp (ms->last_search, search_text)) {
				
				if (!gtk_html_engine_search_next (ms->mail->html)) {
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
	} else if (button == GTK_RESPONSE_CLOSE) {
		gtk_widget_destroy (widget);
	}
}

static void
begin_cb (ESearchingTokenizer *st, char *foo, MailSearch *ms)
{
	const char *subject;
	
	if (ms && ms->mail && ms->mail->current_message) {
		subject = ms->mail->current_message->subject;
		
		if (subject == NULL)
			subject = _("Untitled Message");
	} else {
		subject = _("Empty Message");
	}
	
	gtk_label_set_text (GTK_LABEL (ms->count_label), "0");
	mail_search_set_subject (ms, subject);
}

static void
match_cb (ESearchingTokenizer *st, MailSearch *ms)
{
	char buf[16];
	
	g_snprintf (buf, 16, "%d", e_searching_tokenizer_match_count (st));
	gtk_label_set_text (GTK_LABEL (ms->count_label), buf);
}

static void
entry_run_search (GtkWidget *w, MailSearch *ms)
{
	/* run search when enter pressed on widget */
	gtk_dialog_response ((GtkDialog *) ms, GTK_RESPONSE_ACCEPT);
}

void
mail_search_construct (MailSearch *ms, MailDisplay *mail)
{
	GtkWidget *find_hbox;
	GtkWidget *matches_hbox;
	GtkWidget *toggles_hbox;
	GtkWidget *frame_vbox;
	GtkWidget *entry;
	GtkWidget *count_label;
	GtkWidget *case_check;
#if 0
	GtkWidget *fwd_check;
#endif
	GtkWidget *button;
	GtkWidget *msg_hbox;
	GtkWidget *msg_frame;
	
	g_return_if_fail (ms != NULL && IS_MAIL_SEARCH (ms));
	g_return_if_fail (mail != NULL && IS_MAIL_DISPLAY (mail));
	
	/* Basic set-up */
	
	ms->mail = mail;
	g_object_ref (mail);
	
	gtk_window_set_title ((GtkWindow *) ms, _("Find in Message"));
	
	button = gtk_button_new_from_stock (GTK_STOCK_FIND);
	gtk_button_set_label ((GtkButton *) button, _("Search"));
	gtk_dialog_add_action_widget ((GtkDialog*) ms, button, GTK_RESPONSE_ACCEPT);
	gtk_dialog_add_button ((GtkDialog *) ms, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_set_default_response ((GtkDialog *) ms, GTK_RESPONSE_ACCEPT);
	
	ms->search_forward = TRUE;
	ms->case_sensitive = FALSE;
	
	ms->begin_handler = g_signal_connect (ms->mail->html->engine->ht, "begin",
					      G_CALLBACK (begin_cb), ms);
	ms->match_handler = g_signal_connect (ms->mail->html->engine->ht, "match",
					      G_CALLBACK (match_cb), ms);
	
	/* Construct the dialog contents. */
	
	msg_hbox     = gtk_hbox_new (FALSE, 3);
	find_hbox    = gtk_hbox_new (FALSE, 3);
	matches_hbox = gtk_hbox_new (FALSE, 3);
	toggles_hbox = gtk_hbox_new (FALSE, 3);
	frame_vbox   = gtk_vbox_new (FALSE, 3);
	gtk_container_set_border_width ((GtkContainer *) frame_vbox, 3);
	
	entry       = gtk_entry_new ();
	count_label = gtk_label_new ("0");
	
	msg_frame   = gtk_frame_new (NULL);
	gtk_container_set_border_width ((GtkContainer *) msg_frame, 6);
	
	case_check  = gtk_check_button_new_with_label (_("Case Sensitive"));
#if 0
	fwd_check   = gtk_check_button_new_with_label (_("Search Forward"));
#endif
	
	ms->entry       = entry;
	ms->count_label = count_label;
	
	ms->msg_frame   = msg_frame;
	
	if (mail->current_message->subject && *mail->current_message->subject)
		mail_search_set_subject (ms, mail->current_message->subject);
	else
		mail_search_set_subject (ms, NULL);
	
#if 0
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fwd_check),  ms->search_forward);
#endif
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (case_check), ms->case_sensitive);
	
  	gtk_box_pack_start (GTK_BOX (msg_hbox), GTK_WIDGET (msg_frame), TRUE, TRUE, 0);
	
	gtk_box_pack_start (GTK_BOX (find_hbox), gtk_label_new (_("Find:")), FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (find_hbox), entry, TRUE, TRUE, 3);
	
	gtk_box_pack_start (GTK_BOX (matches_hbox), gtk_hbox_new (FALSE, 0), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (matches_hbox), gtk_label_new (_("Matches:")), FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (matches_hbox), count_label, FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (matches_hbox), gtk_hbox_new (FALSE, 0), TRUE, TRUE, 0);
	
	gtk_box_pack_start (GTK_BOX (toggles_hbox), case_check, FALSE, FALSE, 3);
	
	/*
	 * Disabling the forward/backward search button because there are problems with it
	 * (related to how gtkhtml handles searches), the GUI freeze is upon us, and I
	 * don't know if they'll get resolved for 1.0.  Hopefully getting this fixed can
	 * be a 1.1 item.
	 */
	
#if 0
	gtk_box_pack_start (GTK_BOX (toggles_hbox), fwd_check,  FALSE, FALSE, 3);
#endif
	
	gtk_box_pack_start (GTK_BOX (frame_vbox), find_hbox, FALSE, FALSE, 3);
  	gtk_box_pack_start (GTK_BOX (frame_vbox), matches_hbox, FALSE, FALSE, 3); 
  	gtk_box_pack_start (GTK_BOX (frame_vbox), toggles_hbox, FALSE, FALSE, 3);
	
	gtk_container_add (GTK_CONTAINER (msg_frame), GTK_WIDGET (frame_vbox));
	
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (ms)->vbox), msg_hbox, TRUE, TRUE, 0); 	
	
	gtk_widget_grab_focus (entry); /* Give focus to entry by default */ 
	g_signal_connect (entry, "activate", G_CALLBACK (entry_run_search), ms);
	gnome_window_icon_set_from_file (GTK_WINDOW (ms), EVOLUTION_ICONSDIR "/find-message.xpm");
	
	gtk_widget_show_all (msg_hbox);
	gtk_widget_show_all (find_hbox);
	gtk_widget_show_all (matches_hbox);
	gtk_widget_show_all (toggles_hbox);
	
	/* Hook up signals */
	
	g_signal_connect (case_check, "toggled", G_CALLBACK (toggled_case_cb), ms);
#if 0
	g_signal_connect (fwd_check, "toggled", G_CALLBACK (toggled_fwd_cb), ms);
#endif
	g_signal_connect (ms, "response", G_CALLBACK (dialog_response_cb), ms);
	
	g_object_weak_ref ((GObject *) ms->mail, (GWeakNotify) gtk_widget_destroy, ms);
}

GtkWidget *
mail_search_new (MailDisplay *mail)
{
	GtkWidget *widget;
	
	g_return_val_if_fail (IS_MAIL_DISPLAY (mail), NULL);
	
	widget = g_object_new (mail_search_get_type (), NULL);
	mail_search_construct (MAIL_SEARCH (widget), mail);
	
	return widget;
}

