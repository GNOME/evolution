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
#  include <config.h>
#endif
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gconf/gconf-client.h>
#include "mail-capplet-shell.h"	
#include "mail-view.h"
#include <gdk/gdkkeysyms.h>

#include <e-util/e-util.h>

#include "mail-decoration.h"
#include <mail/em-utils.h>
#include <mail/em-composer-utils.h>
enum {
	CTRL_W_PRESSED,
	CTRL_Q_PRESSED,
	LAST_SIGNAL
};

/* Re usable colors */

GdkColor *pcolor_sel;
char *scolor_sel;
GdkColor *pcolor_fg_sel;
char *scolor_fg_sel;
GdkColor *pcolor_bg_norm;
char *scolor_bg_norm;
GdkColor *pcolor_norm;
char *scolor_norm;
GdkColor *pcolor_fg_norm;
char *scolor_fg_norm;

static guint mail_capplet_shell_signals[LAST_SIGNAL];

extern gboolean windowed;

struct  _MailCappletShellPrivate {

	GtkWidget *box;
	
	GtkWidget * top_bar;
	GtkWidget *message_pane;
	GtkWidget *bottom_bar;

	/* Top Bar */
	GtkWidget *action_bar;
	GtkWidget *quit;	
	
	MailViewChild *settings_view;
};

static void mail_capplet_shell_quit (MailCappletShell *shell);

G_DEFINE_TYPE (MailCappletShell, mail_capplet_shell, GTK_TYPE_WINDOW)

static void setup_abooks (void);

static void
mail_capplet_shell_init (MailCappletShell  *shell)
{
	shell->priv = g_new0(MailCappletShellPrivate, 1);
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
	MailCappletShellPrivate *priv = shell->priv;

	mail_view_close_view ((MailView *)shell->view);
}

static void
ms_ctrl_q_pressed (MailCappletShell *shell)
{
	mail_capplet_shell_quit (shell);
}	

static void
mail_capplet_shell_class_init (MailCappletShellClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);
	GtkBindingSet *binding_set;

	mail_capplet_shell_parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = mail_capplet_shell_finalize;
	klass->ctrl_w_pressed = ms_ctrl_w_pressed;
	klass->ctrl_q_pressed = ms_ctrl_q_pressed;

	mail_capplet_shell_signals [CTRL_W_PRESSED] =
		g_signal_new ("ctrl_w_pressed",
				G_TYPE_FROM_CLASS (object_class),
					  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				G_STRUCT_OFFSET (MailCappletShellClass, ctrl_w_pressed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
	
	mail_capplet_shell_signals [CTRL_Q_PRESSED] =
		g_signal_new ("ctrl_q_pressed",
				G_TYPE_FROM_CLASS (object_class),
					  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				G_STRUCT_OFFSET (MailCappletShellClass, ctrl_q_pressed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_W, GDK_CONTROL_MASK, "ctrl_w_pressed", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_w, GDK_CONTROL_MASK, "ctrl_w_pressed", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_Q, GDK_CONTROL_MASK, "ctrl_q_pressed", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_Q, GDK_CONTROL_MASK, "ctrl_q_pressed", 0);

};

static int
color_expose (GtkWidget *w,
	      GdkEventExpose *event G_GNUC_UNUSED,
	      gpointer data)
{
	GtkWindow *win = (GtkWindow *)data;
	cairo_t *cr = gdk_cairo_create (w->window);
	int wid = w->allocation.width;
	int heig = w->allocation.height;
	int wwid, wheig;
	GdkColor paint;

	gtk_window_get_size (win, &wwid, &wheig);
	gdk_color_parse ("#000000", &paint);
	gdk_cairo_set_source_color (cr,  &(paint));
	cairo_rectangle (cr, 0, 0, wwid, wheig);
	cairo_stroke (cr);

	gdk_color_parse ("#000000", &paint);
	gdk_cairo_set_source_color (cr,  &(paint));
	cairo_rectangle (cr, 1, 1, wid, heig);
	cairo_fill (cr);

	cairo_destroy (cr);

	return FALSE;
}



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
	gtk_main_quit();
}

static void
mail_capplet_shell_quit_cb (GtkWidget *w G_GNUC_UNUSED,
		    MailCappletShell *shell)
{
	mail_capplet_shell_quit (shell);
}

static void
ms_delete_event (MailCappletShell *shell,
		 GdkEvent *event G_GNUC_UNUSED,
		 gpointer data G_GNUC_UNUSED)
{
	mail_capplet_shell_quit (shell);
}

static gboolean 
ms_check_new ()
{
	GConfClient *client;
	GSList *accounts;

	client = gconf_client_get_default ();
	accounts = gconf_client_get_list (client, "/apps/evolution/mail/accounts", GCONF_VALUE_STRING, NULL);
	g_object_unref (client);

	if (accounts != NULL) {
		g_slist_foreach (accounts, (GFunc) g_free, NULL);
		g_slist_free (accounts);

		return FALSE;
	}
	
	return TRUE;
}


static void
ms_show_post_druid (MailViewChild *mfv G_GNUC_UNUSED,
		    MailCappletShell *shell)
{
	if (shell->priv->settings_view)
		mail_view_switch_to_settings ((MailView *)shell->view, (MailViewChild *)shell->priv->settings_view);
	else {
		shell->priv->settings_view = mail_view_add_page ((MailView *)shell->view, MAIL_VIEW_SETTINGS, NULL);
	}

}

#define PACK_IN_TOOL(wid,icon)	{ GtkWidget *tbox; tbox = gtk_hbox_new (FALSE, 0); gtk_box_pack_start ((GtkBox *)tbox, gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0); wid = (GtkWidget *)gtk_tool_button_new (tbox, NULL); }

#if 0
static void
handle_cmdline (MailView *mv, MailCappletShell *shell)
{
	g_signal_handlers_block_by_func (mv, handle_cmdline, shell);
	mail_capplet_shell_handle_cmdline (shell);
}
#endif

void
mail_capplet_shell_construct (MailCappletShell *shell, int socket_id)
{
	MailCappletShellPrivate *priv = shell->priv;
	GtkWidget *tmp, *img, *box, *ar1, *ar2, *lbl;
	GtkStyle *style = gtk_widget_get_default_style ();
	int window_width = 1024;
	char *custom_dir;

	gtk_window_set_icon_name ((GtkWindow *)shell, "evolution");
	ms_init_style (style);
	g_signal_connect ((GObject *)shell, "delete-event", G_CALLBACK (ms_delete_event), NULL);
	gtk_window_set_type_hint ((GtkWindow *)shell, GDK_WINDOW_TYPE_HINT_NORMAL);
	if (g_getenv("ANJAL_NO_MAX") == NULL && !windowed) {
		 GdkScreen *scr = gtk_widget_get_screen ((GtkWidget *)shell);
		 window_width = gdk_screen_get_width(scr);
		 gtk_window_set_default_size ((GtkWindow *)shell, gdk_screen_get_width(scr), gdk_screen_get_height (scr));
		 gtk_window_set_decorated ((GtkWindow *)shell, FALSE);
	} else  {
		mail_decoration_new ((GtkWindow *)shell);
	}


	priv->box = (GtkWidget *) gtk_vbox_new (FALSE, 0);
	gtk_widget_show ((GtkWidget *)priv->box);

	if (!socket_id) {
		/* Toolbar */
		priv->top_bar = gtk_toolbar_new ();
		gtk_box_pack_start ((GtkBox *)priv->box, priv->top_bar, FALSE, FALSE, 0);
		gtk_widget_show (priv->top_bar);
		if (g_getenv("ANJAL_NO_MAX") || windowed) {
			gtk_container_set_border_width (GTK_CONTAINER (shell), 1);
			g_signal_connect (priv->top_bar, "expose-event",
							  G_CALLBACK (color_expose),
							  shell);
			/* Leave it to the theme to decide the height */
			/* gtk_widget_set_size_request (priv->top_bar, -1, 42);	*/
		}
	
		/* Label */
		tmp = (GtkWidget *)gtk_tool_item_new ();
		gtk_tool_item_set_expand((GtkToolItem *)tmp, FALSE);
		lbl = gtk_label_new (_("Email Settings"));
		gtk_container_add ((GtkContainer *)tmp, lbl);
		gtk_toolbar_insert ((GtkToolbar *)priv->top_bar, (GtkToolItem *)tmp, 0);
		gtk_widget_show_all (tmp);
	
		tmp = (GtkWidget *)gtk_tool_item_new ();
		gtk_tool_item_set_expand((GtkToolItem *)tmp, TRUE);
		lbl = gtk_label_new (NULL);
		gtk_container_add ((GtkContainer *)tmp, lbl);
		gtk_toolbar_insert ((GtkToolbar *)priv->top_bar, (GtkToolItem *)tmp, 1);
		gtk_widget_show_all (tmp);	
	
		/* Close button */
		PACK_IN_TOOL(priv->quit, "gtk-close");
		gtk_widget_set_tooltip_text(priv->quit, _("Quit"));
		gtk_tool_item_set_expand ((GtkToolItem *)priv->quit, FALSE);
		gtk_toolbar_insert ((GtkToolbar *)priv->top_bar, (GtkToolItem *)priv->quit, -1);
		gtk_widget_show_all (priv->quit);
		g_signal_connect (priv->quit, "clicked", G_CALLBACK(mail_capplet_shell_quit_cb), shell);

		gtk_container_add ((GtkContainer *)shell, priv->box);
	} else {
		GtkWidget *plug = gtk_plug_new (socket_id);
		
		gtk_container_add ((GtkContainer *)plug, priv->box);
		g_signal_connect (plug, "destroy", G_CALLBACK (gtk_main_quit), NULL);
		gtk_widget_show (plug);
		gtk_widget_hide ((GtkWidget *)shell);

	}

	shell->view = mail_view_new ();
	gtk_widget_show ((GtkWidget *)shell->view);
	tmp = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_end ((GtkBox *)priv->box, (GtkWidget *)shell->view, TRUE, TRUE, 2);

	/* This also initializes Camel, so it needs to happen early. */
	mail_session_init ();
	mail_config_init ();
	mail_msg_init ();
	custom_dir = g_build_filename (e_get_user_data_dir (), "mail", NULL);
	e_mail_store_init (custom_dir);
	g_free (custom_dir);
	
	if (ms_check_new()) {
		MailViewChild *mc;
		char *pdir = g_build_filename (g_get_home_dir(), ".gnome2_private", NULL);

		mc = mail_view_add_page ((MailView *)shell->view, MAIL_VIEW_ACCOUNT, NULL);
		g_signal_connect (mc, "view-close", G_CALLBACK(ms_show_post_druid), shell);
		setup_abooks ();
		if (!g_file_test(pdir, G_FILE_TEST_EXISTS)) {
			g_mkdir (pdir, 0700);
		}
		g_free (pdir);
	} else 
		shell->priv->settings_view = mail_view_add_page ((MailView *)shell->view, MAIL_VIEW_SETTINGS, NULL);


}

int
mail_capplet_shell_toolbar_height (MailCappletShell *shell)
{
	return shell->priv->top_bar->allocation.height;
}

MailCappletShell *
mail_capplet_shell_new (int socket_id)
{
	MailCappletShell *shell = g_object_new (MAIL_CAPPLET_SHELL_TYPE, NULL);
	mail_capplet_shell_construct (shell, socket_id);

	return shell;
}

#define PERSONAL_RELATIVE_URI "system"

static void
setup_abooks()
{
	char *base_dir, *uri;
	GSList *groups;
	ESourceGroup *group;
	ESourceList *list = NULL;
	ESourceGroup *on_this_computer = NULL;
	ESource *personal_source = NULL;

	base_dir = g_build_filename (e_get_user_data_dir (), "addressbook", "local", NULL);
	uri = g_filename_to_uri (base_dir, NULL, NULL);
	
	if (!e_book_get_addressbooks(&list, NULL)) {
		g_warning ("Unable to get books\n");
		return;
	}

	groups = e_source_list_peek_groups (list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			if (!on_this_computer && !strcmp (uri, e_source_group_peek_base_uri (group))) {
				on_this_computer = g_object_ref (group);
				break;
			}
		}
	}
	
	if (on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;
			if (!strcmp (PERSONAL_RELATIVE_URI, relative_uri)) {
				personal_source = g_object_ref (source);
				break;
			}
		}
	}
	else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), uri);
		e_source_list_add_group (list, group, -1);

		on_this_computer = group;
	}

	if (!personal_source) {
		/* Create the default Person addressbook */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);

		e_source_set_property (source, "completion", "true");

		personal_source = source;
	}

	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (personal_source)
		g_object_unref (personal_source);
	
	e_source_list_sync (list, NULL);
	g_object_unref (list);
	g_free (uri);
	g_free (base_dir);
}

