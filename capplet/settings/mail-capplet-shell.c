/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *		Srinivasa Ragavan <srini@linux.intel.com>
 *
 * Copyright (C) 2010 Intel Corporation. (www.intel.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkx.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include "mail-capplet-shell.h"
#include "mail-view.h"
#include <gdk/gdkkeysyms.h>

#include <e-util/e-util.h>

#include <mail/em-utils.h>
#include <mail/em-composer-utils.h>
#include <libemail-engine/mail-config.h>
#include <libemail-utils/mail-mt.h>

#include <shell/e-shell.h>

enum {
	CTRL_W_PRESSED,
	CTRL_Q_PRESSED,
	LAST_SIGNAL
};

/* Re usable colors */

GdkColor *pcolor_sel;
gchar *scolor_sel;
GdkColor *pcolor_fg_sel;
gchar *scolor_fg_sel;
GdkColor *pcolor_bg_norm;
gchar *scolor_bg_norm;
GdkColor *pcolor_norm;
gchar *scolor_norm;
GdkColor *pcolor_fg_norm;
gchar *scolor_fg_norm;

static guint mail_capplet_shell_signals[LAST_SIGNAL];

struct  _MailCappletShellPrivate {

	EMailBackend *backend;
	GtkWidget *box;

	GtkWidget * top_bar;
	GtkWidget *message_pane;
	GtkWidget *bottom_bar;

	/* Top Bar */
	GtkWidget *action_bar;
	GtkWidget *quit;

	gboolean main_loop;

	MailViewChild *settings_view;
};

static void mail_capplet_shell_quit (MailCappletShell *shell);

G_DEFINE_TYPE (MailCappletShell, mail_capplet_shell, GTK_TYPE_WINDOW)

static void
mail_capplet_shell_init (MailCappletShell *shell)
{
	shell->priv = g_new0 (MailCappletShellPrivate, 1);
	shell->priv->settings_view = NULL;
}

static void
mail_capplet_shell_finalize (GObject *object)
{
	G_OBJECT_CLASS (mail_capplet_shell_parent_class)->finalize (object);
}

static void
ms_ctrl_w_pressed (MailCappletShell *shell)
{
	mail_view_close_view ((MailView *) shell->view);
}

static void
ms_ctrl_q_pressed (MailCappletShell *shell)
{
	mail_capplet_shell_quit (shell);
}

static void
mail_capplet_shell_class_init (MailCappletShellClass *class)
{
	GObjectClass * object_class = G_OBJECT_CLASS (class);
	GtkBindingSet *binding_set;

	object_class->finalize = mail_capplet_shell_finalize;
	class->ctrl_w_pressed = ms_ctrl_w_pressed;
	class->ctrl_q_pressed = ms_ctrl_q_pressed;

	mail_capplet_shell_signals[CTRL_W_PRESSED] =
		g_signal_new ("ctrl_w_pressed",
				G_TYPE_FROM_CLASS (object_class),
					  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				G_STRUCT_OFFSET (MailCappletShellClass, ctrl_w_pressed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);

	mail_capplet_shell_signals[CTRL_Q_PRESSED] =
		g_signal_new ("ctrl_q_pressed",
				G_TYPE_FROM_CLASS (object_class),
					  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				G_STRUCT_OFFSET (MailCappletShellClass, ctrl_q_pressed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_W, GDK_CONTROL_MASK, "ctrl_w_pressed", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_w, GDK_CONTROL_MASK, "ctrl_w_pressed", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Q, GDK_CONTROL_MASK, "ctrl_q_pressed", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Q, GDK_CONTROL_MASK, "ctrl_q_pressed", 0);

};

static void
ms_init_style (GtkStyle *style)
{
	 pcolor_sel = &style->base[GTK_STATE_SELECTED];
	 scolor_sel =  gdk_color_to_string (pcolor_sel);

	 pcolor_norm = &style->bg[GTK_STATE_NORMAL];
	 scolor_norm =  gdk_color_to_string (pcolor_norm);

	 pcolor_bg_norm = &style->base[GTK_STATE_NORMAL];
	 scolor_bg_norm = gdk_color_to_string (pcolor_bg_norm);

	 pcolor_fg_sel =&style->fg[GTK_STATE_SELECTED];
	 scolor_fg_sel = gdk_color_to_string (pcolor_fg_sel);

	 pcolor_fg_norm =&style->fg[GTK_STATE_NORMAL];
	 scolor_fg_norm = gdk_color_to_string (pcolor_fg_norm);
}

static void
mail_capplet_shell_quit (MailCappletShell *shell)
{
	MailCappletShellPrivate *priv = shell->priv;

	if (!priv->main_loop)
		gtk_widget_hide ((GtkWidget *) shell);
}

static void
ms_delete_event (MailCappletShell *shell,
                 GdkEvent *event G_GNUC_UNUSED,
                 gpointer data G_GNUC_UNUSED)
{
	mail_capplet_shell_quit (shell);
	gtk_main_quit ();
}

static void
ms_show_post_druid (MailViewChild *mfv G_GNUC_UNUSED,
                    MailCappletShell *shell)
{
	gtk_main_quit ();
	g_timeout_add_seconds (5, (GSourceFunc) gtk_widget_destroy, shell);
}

#define PACK_IN_TOOL(wid,icon)	{ GtkWidget *tbox; tbox = gtk_hbox_new (FALSE, 0); gtk_box_pack_start ((GtkBox *)tbox, gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0); wid = (GtkWidget *)gtk_tool_button_new (tbox, NULL); }

static void
mail_capplet_shell_construct (MailCappletShell *shell,
                              gint socket_id,
                              gboolean just_druid,
                              gboolean main_loop)
{
	MailCappletShellPrivate *priv = shell->priv;
	GtkStyle *style = gtk_widget_get_default_style ();
	EShell *eshell;
	EMailSession *session;

	gtk_window_set_icon_name ((GtkWindow *)shell, "evolution");
	gtk_window_set_title ((GtkWindow *)shell, _("Evolution account assistant"));
	ms_init_style (style);
	g_signal_connect (
		shell, "delete-event",
		G_CALLBACK (ms_delete_event), NULL);
	gtk_window_set_type_hint ((GtkWindow *) shell, GDK_WINDOW_TYPE_HINT_NORMAL);
	if (g_getenv("ANJAL_NO_MAX") == NULL && FALSE) {
		 GdkScreen *scr = gtk_widget_get_screen ((GtkWidget *) shell);
		 gtk_window_set_default_size ((GtkWindow *) shell, gdk_screen_get_width (scr), gdk_screen_get_height (scr));
		 gtk_window_set_decorated ((GtkWindow *) shell, FALSE);
	} else  {
		gtk_window_set_default_size ((GtkWindow *) shell, 1024, 500);
	}

	priv->main_loop = main_loop;
	priv->box = (GtkWidget *) gtk_vbox_new (FALSE, 0);
	gtk_widget_show ((GtkWidget *) priv->box);

	if (!socket_id) {
		gtk_container_add ((GtkContainer *) shell, priv->box);
	} else {
		GtkWidget *plug = gtk_plug_new (socket_id);

		gtk_container_add ((GtkContainer *) plug, priv->box);
		g_signal_connect (
			plug, "destroy",
			G_CALLBACK (gtk_main_quit), NULL);
		gtk_widget_show (plug);
		gtk_widget_hide ((GtkWidget *) shell);

	}

	if (camel_init (e_get_user_data_dir (), TRUE) != 0)
		exit (0);

	camel_provider_init ();

	eshell = e_shell_get_default ();

	if (eshell == NULL) {
		GError *error = NULL;

		eshell = g_initable_new (
			E_TYPE_SHELL, NULL, &error,
			"application-id", "org.gnome.Evolution",
			"flags", G_APPLICATION_HANDLES_OPEN | G_APPLICATION_HANDLES_COMMAND_LINE,
			"geometry", NULL,
			"module-directory", EVOLUTION_MODULEDIR,
			"meego-mode", TRUE,
			"express-mode", TRUE,
			"small-screen-mode", TRUE,
			"online", TRUE,
			NULL);

		if (error != NULL)
			g_error ("%s", error->message);

		e_shell_load_modules (eshell);
	}

	shell->priv->backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (eshell, "mail"));
	session = e_mail_backend_get_session (shell->priv->backend);

	shell->view = mail_view_new ();
	shell->view->backend = shell->priv->backend;
	gtk_widget_show ((GtkWidget *) shell->view);
	gtk_box_pack_end ((GtkBox *) priv->box, (GtkWidget *) shell->view, TRUE, TRUE, 2);

	mail_config_init (session);
	mail_msg_init ();

	if (just_druid) {
		MailViewChild *mc;

		gtk_notebook_set_show_tabs ((GtkNotebook *) shell->view, FALSE);
		mc = mail_view_add_page ((MailView *) shell->view, MAIL_VIEW_ACCOUNT, NULL);
		g_signal_connect (
			mc, "view-close",
			G_CALLBACK (ms_show_post_druid), shell);
	} else
		shell->priv->settings_view = mail_view_add_page ((MailView *) shell->view, MAIL_VIEW_SETTINGS, NULL);

}

GtkWidget *
mail_capplet_shell_new (gint socket_id,
                        gboolean just_druid,
                        gboolean main_loop)
{
	MailCappletShell *shell = g_object_new (MAIL_CAPPLET_SHELL_TYPE, NULL);
	mail_capplet_shell_construct (shell, socket_id, just_druid, main_loop);

	return GTK_WIDGET (shell);
}
