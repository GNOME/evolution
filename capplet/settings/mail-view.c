/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 2009 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <glib/gi18n.h>
#include "mail/em-utils.h"
#include "mail/mail-send-recv.h"
#include "mail/mail-ops.h"
#include "mail-view.h"
#ifndef ANJAL_SETTINGS
#include "mail-folder-view.h"
#include "mail-composer-view.h"
#include "mail-conv-view.h"
#endif

#include "mail-settings-view.h"

#include "anjal-mail-view.h"
#include "mail-account-view.h"
#include "mail/em-folder-tree.h"
#include <shell/e-shell-searchbar.h>

struct  _MailViewPrivate {

	GtkWidget *box;
	GList *children;
	MailViewChild *current_view;
	GtkWidget *new;
	GtkWidget *search;
	GtkWidget *search_entry;
};

enum {
	VIEW_NEW,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (MailView, mail_view, ANJAL_MAIL_VIEW_TYPE)
#ifndef ANJAL_SETTINGS
static MailConvView * mv_switch_message_view (MailView *mv, const gchar *uri);
#endif

#define REALIGN_NODES(list,pdata)	if (list->data != pdata) { \
						list = g_list_remove (list, pdata);	\
						list = g_list_prepend (list, pdata); \
					}

void anjal_shell_view_restore_state (EShellView *view, const gchar *uri);

static void
mail_view_init (MailView  *shell)
{
	shell->priv = g_new0(MailViewPrivate, 1);
	shell->priv->children = NULL;
	shell->priv->current_view = NULL;
	shell->folder_tree = NULL;
	shell->check_mail = NULL;
	shell->sort_by = NULL;
}

static void
mail_view_finalize (GObject *object)
{
	MailView *shell = (MailView *)object;
	MailViewPrivate *priv = shell->priv;

	g_list_free (priv->children);
	g_free (priv);

	G_OBJECT_CLASS (mail_view_parent_class)->finalize (object);
}

static void
mv_set_folder_uri (AnjalMailView *mv, const gchar *uri)
{
#ifndef ANJAL_SETTINGS
	mail_view_set_folder_uri ((MailView *)mv, uri);
#endif
}

static void set_folder_tree (AnjalMailView *mv, EMFolderTree *tree)
{
	mail_view_set_folder_tree ((MailView *)mv, (GtkWidget *)tree);
}

static void
set_search (AnjalMailView *mv, const gchar *search)
{
#ifndef ANJAL_SETTINGS
	mail_view_set_search ((MailView *)mv, search);
#endif
}

static void
mail_view_class_init (MailViewClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);
	AnjalMailViewClass *pclass;
	signals[VIEW_NEW] =
		g_signal_new ("view-new",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (MailViewClass , view_new),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	pclass = mail_view_parent_class = g_type_class_peek_parent (klass);
	((AnjalMailViewClass *)klass)->set_folder_uri = mv_set_folder_uri;
	((AnjalMailViewClass *)klass)->set_folder_tree = set_folder_tree;
	((AnjalMailViewClass *)klass)->set_search = set_search;

	object_class->finalize = mail_view_finalize;

};

static void
mv_switch (GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data)
{
	MailView *shell = (MailView *)notebook;
	MailViewPrivate *priv = shell->priv;
	MailViewChild *curr = priv->current_view;
	MailViewChild *child;
	gint current_child = gtk_notebook_get_current_page (notebook);

	curr->flags &= ~MAIL_VIEW_HOLD_FOCUS;

	child = (MailViewChild *)gtk_notebook_get_nth_page (notebook, current_child);

	priv->current_view = child;
	REALIGN_NODES(shell->priv->children,child);

#ifndef ANJAL_SETTINGS
	if (child->type == MAIL_VIEW_COMPOSER)
		 mail_composer_view_activate ((MailComposerView *)child, shell->folder_tree, shell->check_mail, shell->sort_by, TRUE);
	else if (child->type == MAIL_VIEW_MESSAGE)
		 mail_conv_view_activate ((MailConvView *)child, shell->tree, shell->folder_tree, shell->check_mail, shell->sort_by, FALSE);
	else if (child->type == MAIL_VIEW_FOLDER)  {
		 mail_folder_view_activate ((MailFolderView *)child, shell->tree, shell->folder_tree, shell->check_mail, shell->sort_by, shell->slider, TRUE);
		 anjal_shell_view_restore_state (shell->shell_view, child->uri);
	} else if (child->type == MAIL_VIEW_ACCOUNT)
		mail_account_view_activate ((MailAccountView *)child, shell->tree, shell->folder_tree, shell->check_mail, shell->sort_by, FALSE);
	else if (child->type == MAIL_VIEW_SETTINGS)
		mail_settings_view_activate ((MailSettingsView *)child, shell->tree, shell->folder_tree, shell->check_mail, shell->sort_by, shell->slider, FALSE);
#else
	if (child->type == MAIL_VIEW_ACCOUNT)
		mail_account_view_activate ((MailAccountView *)child, shell->tree, shell->folder_tree, shell->check_mail, shell->sort_by, FALSE);
	else if (child->type == MAIL_VIEW_SETTINGS)
		mail_settings_view_activate ((MailSettingsView *)child, shell->tree, shell->folder_tree, shell->check_mail, shell->sort_by, shell->slider, FALSE);
#endif
}

static void
mail_view_construct (MailView *shell)
{
	gtk_notebook_set_show_tabs ((GtkNotebook *)shell, TRUE);
	gtk_notebook_set_scrollable ((GtkNotebook *)shell, TRUE);
	gtk_notebook_popup_disable ((GtkNotebook *)shell);
	g_signal_connect_after (shell, "switch-page", G_CALLBACK(mv_switch), shell);
}

MailView *
mail_view_new ()
{
	MailView *shell = g_object_new (MAIL_VIEW_TYPE, NULL);
	mail_view_construct (shell);

	return shell;
}

static gint
mv_get_page_number (GtkNotebook *note, GtkWidget *widget)
{
	gint i, total;

	total = gtk_notebook_get_n_pages (note);
	for (i=0; i<total; i++) {
		if (gtk_notebook_get_nth_page(note, i) == widget)
			return i;
	}

	return total-1;
}

static void
mv_close_mcv (MailViewChild *mfv, MailView *mv)
{
	gint n = mv_get_page_number ((GtkNotebook *)mv, (GtkWidget *)mfv);
	gint pos = gtk_notebook_get_current_page ((GtkNotebook *)mv);
	MailViewChild *child;
	gboolean removing_viewed = FALSE;

	if (g_list_length(mv->priv->children) == 1)
		return;

#ifndef ANJAL_SETTINGS
	/* Make sure atleast one folder view is open. */
	if (mfv->type == MAIL_VIEW_FOLDER) {
		GList *tmp = mv->priv->children;
		gboolean found = FALSE;

		while (!found && tmp) {
			MailViewChild *tchild = (MailViewChild *)tmp->data;

			if (tchild && tchild != mfv && tchild->type == MAIL_VIEW_FOLDER)
				found = true;
			tmp = tmp->next;
		}

		if (!found)
			return;
	}

	if (mfv->type == MAIL_VIEW_COMPOSER) {
		if (!mail_composer_view_can_quit((MailComposerView *)mfv))
			return;
	}
#endif

	g_signal_handlers_block_by_func(mv, mv_switch, mv);
	gtk_notebook_remove_page ((GtkNotebook *)mv, n);
	g_signal_handlers_unblock_by_func(mv, mv_switch, mv);

	if (mfv == mv->priv->children->data)
		removing_viewed = TRUE;
	mv->priv->children  = g_list_remove (mv->priv->children, mfv);

	if (!removing_viewed)
		return;

	child = (MailViewChild *)mv->priv->children->data;
	mv->priv->current_view = child;
	pos = mv_get_page_number ((GtkNotebook *)mv, (GtkWidget *)child);
	gtk_notebook_set_current_page ((GtkNotebook *)mv, pos);

#ifndef ANJAL_SETTINGS
	if (child->type == MAIL_VIEW_COMPOSER)
		 mail_composer_view_activate ((MailComposerView *)child, mv->folder_tree, mv->check_mail, mv->sort_by, TRUE);
	else if (child->type == MAIL_VIEW_MESSAGE)
		 mail_conv_view_activate ((MailConvView *)child, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, FALSE);
	else if (child->type == MAIL_VIEW_FOLDER) {
		 mail_folder_view_activate ((MailFolderView *)child, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, mv->slider, TRUE);
		 anjal_shell_view_restore_state ((EShellView *)mv->shell_view, child->uri);
	} else if (child->type == MAIL_VIEW_ACCOUNT)
		mail_account_view_activate ((MailAccountView *)child, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, FALSE);
	else if (child->type == MAIL_VIEW_SETTINGS)
		mail_settings_view_activate ((MailSettingsView *)child, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, mv->slider, FALSE);
#else
	if (child->type == MAIL_VIEW_ACCOUNT)
		mail_account_view_activate ((MailAccountView *)child, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, FALSE);
	else if (child->type == MAIL_VIEW_SETTINGS)
		mail_settings_view_activate ((MailSettingsView *)child, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, mv->slider, FALSE);

#endif
}

#ifndef ANJAL_SETTINGS
static void
mv_message_new (MailFolderView *mfv, gpointer data, gchar *umid, MailView *mv)
{
	MailConvView *conv = (MailConvView *)mv_switch_message_view(mv, umid);
	*(MailConvView **)data = conv;

	if (conv)
		conv->uri = g_strdup(umid);

	return;
}

static void
mv_search_set (MailFolderView *mfv, MailView *mv)
{
	anjal_shell_view_restore_state (mv->shell_view, ((MailViewChild *)mfv)->uri);
}

static void
mv_folder_loaded (MailFolderView *mfv, MailView *mv)
{
	g_signal_handlers_block_by_func(mfv, mv_folder_loaded, mv);
	g_signal_emit (mv, signals[VIEW_NEW], 0);
}

static MailViewChild *
mail_view_add_folder (MailView *mv, gpointer data, gboolean block)
{
	MailFolderView *mfv = mail_folder_view_new ();
	gint position = 0;
	mail_folder_view_set_folder_pane (mfv, mv->folder_tree);
	if (!block)
		mv->priv->current_view = (MailViewChild *)mfv;
	mv->priv->children = block ? g_list_append(mv->priv->children,  mfv) :  g_list_prepend (mv->priv->children,  mfv);
	position = gtk_notebook_append_page ((GtkNotebook *)mv, (GtkWidget *)mfv, mfv->tab_label);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (mv), (GtkWidget *)mfv, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (mv), (GtkWidget *)mfv, FALSE);

	g_signal_connect (mfv, "view-close", G_CALLBACK(mv_close_mcv), mv);
	if (!block)
		 gtk_notebook_set_current_page ((GtkNotebook *)mv, position);
	g_signal_connect (mfv, "message-shown", G_CALLBACK(mv_message_shown), mv);
	g_signal_connect (mfv, "message-new", G_CALLBACK(mv_message_new), mv);
	g_signal_connect (mfv, "search-set", G_CALLBACK(mv_search_set), mv);
	g_signal_connect (mfv, "view-loaded", G_CALLBACK (mv_folder_loaded), mv);
	if (!block)
		 mail_folder_view_activate (mfv, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, mv->slider, TRUE);

	return (MailViewChild *)mfv;
}

static MailViewChild *
mail_view_add_composer (MailView *mv, gpointer data, gboolean block)
{
	MailComposerView *mcv;
	gint position = 0;
	gboolean special = FALSE;

	if (!data)
		mcv = mail_composer_view_new ();
	else if (data == (gpointer)-1) {
		special = TRUE;
		data = NULL;
	} else
		mcv = mail_composer_view_new_with_composer ((GtkWidget *)data);
	if (!block)
		mv->priv->current_view = (MailViewChild *)mcv;
	mv->priv->children = block ? g_list_append(mv->priv->children,  mcv) :  g_list_prepend (mv->priv->children,  mcv);

	if (!special)
		position = gtk_notebook_append_page ((GtkNotebook *)mv, (GtkWidget *)mcv, mcv->tab_label);
	else {
		gint position = gtk_notebook_get_current_page ((GtkNotebook *)mv);
		gtk_notebook_insert_page ((GtkNotebook *)mv, (GtkWidget *)mcv, mcv->tab_label, position+1);
	}

	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (mv), (GtkWidget *)mcv, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (mv), (GtkWidget *)mcv, FALSE);
	if (!block)
		 gtk_notebook_set_current_page ((GtkNotebook *)mv, position);
	if (!block)
		 mail_composer_view_activate (mcv, mv->folder_tree, mv->check_mail, mv->sort_by, FALSE);

	g_signal_connect (mcv, "view-close", G_CALLBACK(mv_close_mcv), mv);
	g_signal_connect (mcv, "message-shown", G_CALLBACK(mv_message_shown), mv);

	return (MailViewChild *)mcv;
}

static MailViewChild *
mail_view_add_message (MailView *mv, gpointer data, gboolean block)
{
	MailConvView *mcv = mail_conv_view_new ();
	gint position = 0;

	gtk_widget_show ((GtkWidget *)mcv);
	mcv->type = MAIL_VIEW_MESSAGE;
	if (!block)
		mv->priv->current_view = (MailViewChild *)mcv;
	mv->priv->children = block ? g_list_append(mv->priv->children,  mcv) :  g_list_prepend (mv->priv->children,  mcv);

	position = gtk_notebook_get_current_page ((GtkNotebook *)mv);
	gtk_notebook_insert_page ((GtkNotebook *)mv, (GtkWidget *)mcv, mail_conv_view_get_tab_widget(mcv), position+1);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (mv), (GtkWidget *)mcv, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (mv), (GtkWidget *)mcv, FALSE);
	if (!block)
		gtk_notebook_set_current_page ((GtkNotebook *)mv, position+1);
	if (!block)
		 mail_conv_view_activate (mcv, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, FALSE);

	g_signal_connect (mcv, "view-close", G_CALLBACK(mv_close_mcv), mv);
	g_signal_connect (mcv, "message-shown", G_CALLBACK(mv_message_shown), mv);

	return (MailViewChild *)mcv;
}

#endif

static void
mv_show_acc_mcv (MailViewChild *mfv, EAccount *account, MailView *mv)
{
	mail_view_add_page(mv, MAIL_VIEW_ACCOUNT, account);
}

static MailViewChild *
mail_view_add_settings (MailView *mv, gpointer data, gboolean block)
{
	MailSettingsView *msv  = mail_settings_view_new ();
	gint position = 0;

	gtk_widget_show ((GtkWidget *)msv);
	if (!block)
		mv->priv->current_view = (MailViewChild *)msv;
	mv->priv->children = block ? g_list_append(mv->priv->children,  msv) :  g_list_prepend (mv->priv->children,  msv);

	position = gtk_notebook_append_page ((GtkNotebook *)mv, (GtkWidget *)msv, mail_settings_view_get_tab_widget(msv));
	g_signal_connect (msv, "view-close", G_CALLBACK(mv_close_mcv), mv);
	g_signal_connect (msv, "show-account", G_CALLBACK(mv_show_acc_mcv), mv);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (mv), (GtkWidget *)msv, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (mv), (GtkWidget *)msv, FALSE);
	if (!block)
		gtk_notebook_set_current_page ((GtkNotebook *)mv, position);
	if (!block)
		 mail_settings_view_activate (msv, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, mv->slider, FALSE);

	return (MailViewChild *)msv;
}

void
mail_view_switch_to_settings (MailView* mv, MailViewChild *mpv)
{
	gint position = 0;

	position = mv_get_page_number((GtkNotebook *)mv, (GtkWidget *)mpv);
	REALIGN_NODES(mv->priv->children,mpv);
	gtk_notebook_set_current_page ((GtkNotebook *)mv, position);
	mail_settings_view_activate ((MailSettingsView *)mpv, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, mv->slider, FALSE);
}

static MailViewChild *
mail_view_add_account (MailView *mv, gpointer data, gboolean block)
{
	MailAccountView *msv  = mail_account_view_new (data);
	gint position = 0;

	gtk_widget_show ((GtkWidget *)msv);
	if (!block)
		mv->priv->current_view = (MailViewChild *)msv;
	mv->priv->children = block ? g_list_append(mv->priv->children,  msv) :  g_list_prepend (mv->priv->children,  msv);
	position = gtk_notebook_append_page ((GtkNotebook *)mv, (GtkWidget *)msv, mail_account_view_get_tab_widget(msv));
	g_signal_connect_after (msv, "view-close", G_CALLBACK(mv_close_mcv), mv);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (mv), (GtkWidget *)msv, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (mv), (GtkWidget *)msv, FALSE);
	if (!block)
		gtk_notebook_set_current_page ((GtkNotebook *)mv, position);
	if (!block)
		 mail_account_view_activate (msv, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, FALSE);

	return (MailViewChild *)msv;
}

MailViewChild *
mail_view_add_page (MailView *mv, guint16 type, gpointer data)
{
	MailViewChild *child = NULL, *current_child;
	gboolean block = FALSE;

	current_child = mv->priv->current_view;
	if (current_child && (current_child->flags & MAIL_VIEW_HOLD_FOCUS))
		 block = TRUE;

	g_signal_handlers_block_by_func(mv, mv_switch, mv);
	switch (type) {
#ifndef ANJAL_SETTINGS
	case MAIL_VIEW_FOLDER:
		 child = mail_view_add_folder (mv, data, block);
		break;

	case MAIL_VIEW_COMPOSER:
		 child = mail_view_add_composer (mv, data, block);
		break;
	case MAIL_VIEW_MESSAGE:
		 child = mail_view_add_message (mv, data, block);
		break;
#endif
	case MAIL_VIEW_SETTINGS:
		 child = mail_view_add_settings (mv, data, block);
		break;
	case MAIL_VIEW_ACCOUNT:
		 child = mail_view_add_account (mv, data, block);
		break;
	}
	gtk_widget_grab_focus((GtkWidget *)child);
	child->type = type;
	g_signal_handlers_unblock_by_func(mv, mv_switch, mv);

	child->flags = 0;

	return child;
}

#ifndef ANJAL_SETTINGS
static void
mv_switch_folder_view (MailView *mv, const gchar *uri)
{
	 gint i=0, len = g_list_length(mv->priv->children);
	 GList *tmp = mv->priv->children;
	 while (i<len) {
		  MailViewChild *child = (MailViewChild *)gtk_notebook_get_nth_page ((GtkNotebook *)mv, i);

		  if (child->type == MAIL_VIEW_FOLDER && !strcmp (uri, child->uri)) {
			if (child != mv->priv->current_view) {
				gtk_notebook_set_current_page ((GtkNotebook *)mv, i);
			}
			return;
		  }
		  i++;
		  tmp = tmp->next;
	 }

	 mail_view_add_page (mv, MAIL_VIEW_FOLDER, NULL);
	 mail_folder_view_set_folder_uri ((MailFolderView *)mv->priv->current_view, uri);
}

static MailConvView *
mv_switch_message_view (MailView *mv, const gchar *uri)
{
	 gint i=0;
	 GList *tmp = mv->priv->children;
	 while (tmp) {
		  MailViewChild *child = tmp->data;
		  if (child->type == MAIL_VIEW_MESSAGE && !strcmp (uri, child->uri)) {
			   gtk_notebook_set_current_page ((GtkNotebook *)mv, i);
			   mail_conv_view_activate ((MailConvView *)child, mv->tree, mv->folder_tree, mv->check_mail, mv->sort_by, FALSE);
			   REALIGN_NODES(mv->priv->children,child);
			   return NULL;
		  }
		  i++;
		  tmp = tmp->next;
	 }

	 return (MailConvView *)mail_view_add_page (mv, MAIL_VIEW_MESSAGE, NULL);
}

void
mail_view_set_folder_uri (MailView *mv, const gchar *uri)
{

	 mv_switch_folder_view (mv, uri);
}

void
mail_view_show_sort_popup (MailView *mv, GtkWidget *button)
{
	mail_folder_view_show_sort_popup ((MailFolderView *)mv->priv->current_view, button);
}

void
mail_view_show_list (MailView *mv)
{
	 MailViewChild *child = (MailViewChild *)mv->priv->current_view;

	 if (child->type == MAIL_VIEW_MESSAGE)
		  mv_close_mcv (child, mv);
	 else if (child->type == MAIL_VIEW_FOLDER)
		  mail_folder_view_show_list ((MailFolderView *)mv->priv->current_view);

}
#endif
void
mail_view_close_view (MailView *mv)
{
	 MailViewChild *child = (MailViewChild *)mv->priv->current_view;

	 mv_close_mcv (child, mv);
}

#ifndef ANJAL_SETTINGS
static void
mv_slider_clicked (GtkButton *slider, MailView *mv)
{
	gtk_widget_hide (mv->slider);
	gtk_widget_show (mv->folder_tree);
	if (mv->priv->current_view->type == MAIL_VIEW_FOLDER)
		 mail_folder_view_show_list ((MailFolderView *)mv->priv->current_view);
	else {
		 gtk_widget_show (mv->folder_tree);
		 gtk_widget_hide ((GtkWidget *)slider);
	}
}

void
mail_view_set_slider (MailView *mv, GtkWidget *slider)
{
	mv->slider = slider;
	g_signal_connect (slider, "clicked", G_CALLBACK(mv_slider_clicked), mv);
}
#endif

void
mail_view_set_folder_tree_widget (MailView *mv, GtkWidget *tree)
{
	mv->folder_tree = tree;
}

void
mail_view_set_folder_tree (MailView *mv, GtkWidget *tree)
{
	mv->tree = tree;
}

void
mail_view_set_check_email (MailView *mv, GtkWidget *button)
{
	 mv->check_mail = button;;
}

void
mail_view_set_sort_by  (MailView *mv, GtkWidget *button)
{
	 mv->sort_by = button;;
}

#ifndef ANJAL_SETTINGS
static void
mv_spinner_done (CamelFolder *f, gpointer data)
{
	MailView *mv = (MailView *)data;
	 mv_spinner_show (mv, FALSE);
}

void
mail_view_check_mail(MailView *mv, gboolean deep)
{
	 MailViewChild *child = (MailViewChild *)mv->priv->current_view;

	 if (child && child->type == MAIL_VIEW_FOLDER) {
		  mail_folder_view_check_mail ((MailFolderView *)child);
		  CamelFolder *folder;

		  if ((folder = em_folder_tree_get_selected_folder ((EMFolderTree *)mv->tree)) != NULL) {
			   mv_spinner_show (mv, TRUE);
			   mail_refresh_folder(folder, mv_spinner_done, mv);
		  }
	 }

	if (deep) {
		em_utils_clear_get_password_canceled_accounts_flag ();
		mail_send_receive (NULL);
	}
}

void
mail_view_save (MailView *mv)
{
	 GList *child = mv->priv->children;
	 MailViewChild *cview;

	 while (child) {
		  cview = (MailViewChild *)child->data;
		  if (cview->type == MAIL_VIEW_FOLDER) {
			   mail_folder_view_save ((MailFolderView *)cview);
		  }
		  child = child->next;
	 }
}

void
mail_view_set_search (MailView *mv, const gchar *search)
{
	MailViewChild *child = (MailViewChild *)mv->priv->current_view;

	if (child && child->type == MAIL_VIEW_FOLDER) {
		MailFolderView *mfv = (MailFolderView *)child;

		mail_folder_view_set_search (mfv, search, e_shell_searchbar_get_search_text ((EShellSearchbar *)mv->priv->search_entry));
	}
}
#endif

void
mail_view_set_search_entry (MailView *mv, GtkWidget *entry)
{
	mv->priv->search_entry = entry;
}

void
mail_view_init_search (MailView *mv, GtkWidget *search)
{
	mv->priv->search = search;
}

void
mail_view_set_shell_view (MailView *mv, EShellView *shell)
{
	mv->shell_view = shell;
}
