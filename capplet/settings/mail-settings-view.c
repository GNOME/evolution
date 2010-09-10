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
#include "mail-settings-view.h"
#include <libedataserver/e-account-list.h>
#include "mail-view.h"
#include "mail/mail-config.h"
#include <e-util/e-account-utils.h>

struct _MailSettingsViewPrivate {
	GtkWidget *tab_str;

	GtkWidget *scroll;
	GtkWidget *box;

	EAccountList *accounts;
};

G_DEFINE_TYPE (MailSettingsView, mail_settings_view, GTK_TYPE_VBOX)

enum {
	VIEW_CLOSE,
	SHOW_ACCOUNT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void msv_regen_view (MailSettingsView *acview);

static void
mail_settings_view_init (MailSettingsView  *shell)
{
	shell->priv = g_new0(MailSettingsViewPrivate, 1);

}

static void
mail_settings_view_finalize (GObject *object)
{
	/* MailSettingsView *shell = (MailSettingsView *)object; */

	G_OBJECT_CLASS (mail_settings_view_parent_class)->finalize (object);
}

static void
mail_settings_view_class_init (MailSettingsViewClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);

	mail_settings_view_parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = mail_settings_view_finalize;

	signals[VIEW_CLOSE] =
		g_signal_new ("view-close",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (MailSettingsViewClass , view_close),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[SHOW_ACCOUNT] =
		g_signal_new ("show-account",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (MailSettingsViewClass , show_account),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

}

static void
msv_edit_account(GtkButton *button, MailSettingsView *msv)
{
	EAccount *acc = g_object_get_data((GObject *)button, "account");

	g_signal_emit (msv, signals[SHOW_ACCOUNT], 0, acc);
}

static void
msv_delete_account(GtkButton *button, MailSettingsView *msv)
{
	EAccount *account = g_object_get_data((GObject *)button, "account");
	EAccountList *account_list = e_get_account_list ();

	e_account_list_remove (account_list, account);

	e_account_list_save (account_list);

	msv_regen_view (msv);

}

static void
msv_account_added (EAccountList *al, EAccount *account, MailSettingsView *msv)
{
	msv_regen_view (msv);
}

#define PACK_BOX(w,s) box = gtk_hbox_new(FALSE, 0); gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, s); gtk_widget_show(box); gtk_widget_show(w); gtk_box_pack_start((GtkBox *)acview->priv->box, box, FALSE, FALSE, 3);

static void
build_account_button (MailSettingsView *acview, EAccount *account)
{
	GtkWidget *box, *box1, *label, *tbox, *tlabel;
	gchar *tmp;

	box1 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (box1);

	label = gtk_button_new ();
	tbox = gtk_hbox_new (FALSE, 0);

	if (FALSE) {
		tlabel = (GtkWidget *)gtk_image_new_from_stock (account ? "gtk-edit" : "gtk-new", GTK_ICON_SIZE_BUTTON);
		gtk_widget_show(tlabel);
		gtk_box_pack_start((GtkBox *)tbox, tlabel, FALSE, FALSE, 6);
	}

	if (account)
		tmp = g_strdup_printf (_("Modify %s..."), e_account_get_string(account, E_ACCOUNT_ID_ADDRESS));
	else
		tmp = _("Add a new account");
	tlabel = gtk_label_new(tmp);
	if (account)
		g_free(tmp);
	gtk_widget_show(tlabel);
	gtk_box_pack_start((GtkBox *)tbox, tlabel, FALSE, FALSE, 0);
	gtk_widget_show(tbox);
	gtk_container_add((GtkContainer *)label, tbox);
	g_object_set_data ((GObject *)label, "account", account);
	g_signal_connect(label, "clicked", G_CALLBACK(msv_edit_account), acview);
	gtk_box_pack_start ((GtkBox *)box1, label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	if (account) {
		tlabel = gtk_button_new_from_stock ("gtk-delete");
		gtk_box_pack_start((GtkBox *)box1, tlabel, FALSE, FALSE, 12);
		gtk_widget_show(tlabel);
		g_object_set_data ((GObject *)tlabel, "account", account);
		g_signal_connect (tlabel, "clicked", G_CALLBACK(msv_delete_account), acview);
	}

	PACK_BOX(box1,24);
}

static void
msv_regen_view (MailSettingsView *acview)
{
	struct _EAccount *account;
	EAccountList *accounts = acview->priv->accounts;
	EIterator *node;
	GtkWidget *box, *label;
	gchar *buff;

	gtk_container_foreach((GtkContainer *)acview->priv->box, (GtkCallback)gtk_widget_destroy, NULL);

	label = gtk_label_new (NULL);
	buff = g_markup_printf_escaped ("<span size=\"large\" weight=\"bold\">%s</span>", _("Account management"));
	gtk_label_set_markup ((GtkLabel *)label, buff);
	g_free (buff);
	PACK_BOX(label,12);

	node = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (node)) {
		account = (EAccount *) e_iterator_get (node);
		build_account_button (acview, account);
		e_iterator_next (node);
	}
	g_object_unref (node);
	build_account_button (acview, NULL);
}

static void
mail_settings_view_construct (MailSettingsView *acview)
{
	acview->priv->scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (acview->priv->scroll);
	gtk_container_add ((GtkContainer *)acview, acview->priv->scroll);
	gtk_scrolled_window_set_policy ((GtkScrolledWindow *)acview->priv->scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	acview->priv->box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (acview->priv->box);
	gtk_scrolled_window_add_with_viewport ((GtkScrolledWindow *)acview->priv->scroll, acview->priv->box);

	acview->priv->accounts = e_get_account_list ();
	g_signal_connect (acview->priv->accounts, "account-added", G_CALLBACK(msv_account_added), acview);
	msv_regen_view (acview);
	gtk_widget_show((GtkWidget *)acview);

}

MailSettingsView *
mail_settings_view_new ()
{
	MailSettingsView *view = g_object_new (MAIL_SETTINGS_VIEW_TYPE, NULL);
	view->type = MAIL_VIEW_SETTINGS;
	view->uri = "settings://";

	mail_settings_view_construct (view);

	return view;
}

static gboolean
msv_btn_expose (GtkWidget *w, GdkEventExpose *event, MailSettingsView *mfv)
{
	GdkPixbuf *img = g_object_get_data ((GObject *)w, "pbuf");
	cairo_t *cr;

	cr = gdk_cairo_create (gtk_widget_get_window (w));
	cairo_save (cr);
	gdk_cairo_set_source_pixbuf (cr, img, event->area.x-5, event->area.y-4);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_destroy (cr);

	return TRUE;
}

static void
msv_close (GtkButton *w, MailSettingsView *mfv)
{
	g_signal_emit (mfv, signals[VIEW_CLOSE], 0);
}

GtkWidget *
mail_settings_view_get_tab_widget(MailSettingsView *mcv)
{
	GdkPixbuf *pbuf = gtk_widget_render_icon ((GtkWidget *)mcv, "gtk-close", GTK_ICON_SIZE_MENU, NULL);

	GtkWidget *tool, *box, *img;
	gint w=-1, h=-1;
	GtkWidget *tab_label;

	img = gtk_image_new_from_pixbuf (pbuf);
	g_object_set_data ((GObject *)img, "pbuf", pbuf);
	g_signal_connect (img, "expose-event", G_CALLBACK(msv_btn_expose), mcv);

	tool = gtk_button_new ();
	gtk_button_set_relief((GtkButton *)tool, GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click ((GtkButton *)tool, FALSE);
	gtk_widget_set_tooltip_text (tool, _("Close Tab"));
	g_signal_connect (tool, "clicked", G_CALLBACK(msv_close), mcv);

	box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)box, img, FALSE, FALSE, 0);
	gtk_container_add ((GtkContainer *)tool, box);
	gtk_widget_show_all (tool);
	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings(tool) , GTK_ICON_SIZE_MENU, &w, &h);
	gtk_widget_set_size_request (tool, w+2, h+2);

	box = gtk_label_new (_("Settings"));
	tab_label = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)tab_label, box, FALSE, FALSE, 0);
#ifndef ANJAL_SETTINGS
	gtk_box_pack_start ((GtkBox *)tab_label, tool, FALSE, FALSE, 0);
#endif
	gtk_widget_show_all (tab_label);

	return tab_label;

}

void
mail_settings_view_activate (MailSettingsView *mcv, GtkWidget *tree, GtkWidget *folder_tree, GtkWidget *check_mail, GtkWidget *sort_by, GtkWidget *slider, gboolean act)
{
	 if (!check_mail || !sort_by)
		  return;
	 //if (!GTK_WIDGET_VISIBLE(folder_tree))
	 //	 gtk_widget_show (slider);
	 gtk_widget_set_sensitive (check_mail, TRUE);
	 gtk_widget_set_sensitive (sort_by, FALSE);
}
