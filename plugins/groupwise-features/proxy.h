 /* Evolution calendar - Timezone selector dialog
 *
 * Copyright (C) 2005 Novell, Inc.
 *
 * Authors: 
 * Shreyas Srinivasan <sshreyas@novell.com>
 * Sankar P <psankar@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtkwidget.h>
#include <e-util/e-account.h>
#include <gtk/gtk.h>

#define TYPE_PROXY_DIALOG       (proxy_dialog_get_type ())
#define PROXY_DIALOG(obj)       (GTK_CHECK_CAST ((obj), TYPE_PROXY_DIALOG, proxyDialog))
#define PROXY_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), TYPE_PROXY_DIALOG, proxyDialogClass))
#define IS_PROXY_DIALOG(obj)    (GTK_CHECK_TYPE ((obj), TYPE_PROXY_DIALOG))
#define IS_PROXY_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_PROXY_DIALOG))

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
void proxy_commit(GtkWidget *button, EConfigHookItemFactoryData *data);
GtkWidget * org_gnome_proxy (EPlugin *epl, EConfigHookItemFactoryData *data);
static void proxy_add_account (GtkWidget *button, EAccount *account);
static void proxy_remove_account (GtkWidget *button, EAccount *account);
static void proxy_update_tree_view (EAccount *account);
static void proxy_cancel(GtkWidget *button, EAccount *account);
static void proxy_edit_account (GtkWidget *button, EAccount *account);
void proxy_abort (GtkWidget *button, EConfigHookItemFactoryData *data);
void proxy_commit (GtkWidget *button, EConfigHookItemFactoryData *data);
static void proxy_setup_meta_tree_view (EAccount *account);
static proxyHandler *proxy_get_item_from_list (EAccount *account, char *account_name);
static void proxy_load_edit_dialog (EAccount *account, proxyHandler *edited);
void free_proxy_list (GList *proxy_list);
