/*
 *
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
 *		Shreyas Srinivasan <sshreyas@novell.com>
 *		Sankar P <psankar@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <libedataserver/e-account.h>
#include <gtk/gtk.h>

#define TYPE_PROXY_DIALOG       (proxy_dialog_get_type ())
#define PROXY_DIALOG(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PROXY_DIALOG, proxyDialog))
#define PROXY_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PROXY_DIALOG, proxyDialogClass))
#define IS_PROXY_DIALOG(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PROXY_DIALOG))
#define IS_PROXY_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_PROXY_DIALOG))

typedef struct _proxyDialog		proxyDialog;
typedef struct _proxyDialogClass        proxyDialogClass;
typedef struct _proxyDialogPrivate	proxyDialogPrivate;

struct _proxyDialog{
    GObject object;

    /*Connection */
    EGwConnection *cnc;

    /* Private Dialog Information*/
    proxyDialogPrivate *priv;
};

struct _proxyDialogClass {
	GObjectClass parent_class;
};

GType proxy_dialog_get_type (void);
proxyDialog *proxy_dialog_new (void);
GtkWidget * org_gnome_proxy (EPlugin *epl, EConfigHookItemFactoryData *data);
static void proxy_add_account (GtkWidget *button, EAccount *account);
static void proxy_remove_account (GtkWidget *button, EAccount *account);
static void proxy_update_tree_view (EAccount *account);
static void proxy_cancel(GtkWidget *button, EAccount *account);
static void proxy_edit_account (GtkWidget *button, EAccount *account);
void proxy_abort (GtkWidget *button, EConfigHookItemFactoryData *data);
void proxy_commit (GtkWidget *button, EConfigHookItemFactoryData *data);
static void proxy_setup_meta_tree_view (EAccount *account);
static proxyHandler *proxy_get_item_from_list (EAccount *account, gchar *account_name);
static void proxy_load_edit_dialog (EAccount *account, proxyHandler *edited);
void free_proxy_list (GList *proxy_list);
