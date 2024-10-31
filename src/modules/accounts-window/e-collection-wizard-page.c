/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "shell/e-shell.h"

#include "e-collection-wizard-page.h"

/* #define RUN_INSIDE_ACCOUNTS_WINDOW 1 */

/* Standard GObject macros */
#define E_TYPE_COLLECTION_WIZARD_PAGE \
	(e_collection_wizard_page_get_type ())
#define E_COLLECTION_WIZARD_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COLLECTION_WIZARD_PAGE, ECollectionWizardPage))
#define E_COLLECTION_WIZARD_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COLLECTION_WIZARD_PAGE, ECollectionWizardPageClass))
#define E_IS_COLLECTION_WIZARD_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COLLECTION_WIZARD_PAGE))
#define E_IS_COLLECTION_WIZARD_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COLLECTION_WIZARD_PAGE))
#define E_COLLECTION_WIZARD_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COLLECTION_WIZARD_PAGE, ECollectionWizardPageClass))

typedef struct _ECollectionWizardPage ECollectionWizardPage;
typedef struct _ECollectionWizardPageClass ECollectionWizardPageClass;

struct _ECollectionWizardPage {
	EExtension parent;

#ifdef RUN_INSIDE_ACCOUNTS_WINDOW
	ECollectionAccountWizard *collection_wizard;
	gint page_index;

	GtkButton *prev_button; /* not referenced */
	GtkButton *next_button; /* not referenced */
#endif
};

struct _ECollectionWizardPageClass {
	EExtensionClass parent_class;
};

GType e_collection_wizard_page_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (ECollectionWizardPage, e_collection_wizard_page, E_TYPE_EXTENSION)

#ifdef RUN_INSIDE_ACCOUNTS_WINDOW
static void
collection_wizard_page_update_button_captions (ECollectionWizardPage *page)
{
	g_return_if_fail (E_IS_COLLECTION_WIZARD_PAGE (page));

	if (gtk_notebook_get_current_page (GTK_NOTEBOOK (page->collection_wizard)))
		gtk_button_set_label (page->prev_button, _("_Previous"));
	else
		gtk_button_set_label (page->prev_button, _("_Back"));

	if (e_collection_account_wizard_is_finish_page (page->collection_wizard))
		gtk_button_set_label (page->next_button, _("_Finish"));
	else
		gtk_button_set_label (page->next_button, _("_Next"));
}
#endif

static gboolean
collection_wizard_page_add_source_cb (EAccountsWindow *accounts_window,
				      const gchar *kind,
				      gpointer user_data)
{
	ECollectionWizardPage *page = user_data;
#ifndef RUN_INSIDE_ACCOUNTS_WINDOW
	GtkWindow *window;
#endif

	g_return_val_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window), FALSE);
	g_return_val_if_fail (E_IS_COLLECTION_WIZARD_PAGE (page), FALSE);

	if (g_strcmp0 (kind, "collection") != 0)
		return FALSE;

#ifdef RUN_INSIDE_ACCOUNTS_WINDOW
	e_collection_account_wizard_reset (page->collection_wizard);
	collection_wizard_page_update_button_captions (page);

	e_accounts_window_activate_page (accounts_window, page->page_index);
#else
	window = e_collection_account_wizard_new_window (GTK_WINDOW (accounts_window), e_accounts_window_get_registry (accounts_window));

	gtk_window_present (window);
#endif

	return TRUE;
}

#ifdef RUN_INSIDE_ACCOUNTS_WINDOW
static void
collection_wizard_page_wizard_done (ECollectionWizardPage *page,
				    const gchar *uid)
{
	EAccountsWindow *accounts_window;

	g_return_if_fail (E_IS_COLLECTION_WIZARD_PAGE (page));

	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (page)));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	e_collection_account_wizard_abort (page->collection_wizard);

	e_accounts_window_select_source (accounts_window, uid);
	e_accounts_window_activate_page (accounts_window, -1);
}

static void
collection_wizard_back_button_clicked_cb (GtkButton *button,
					  gpointer user_data)
{
	ECollectionWizardPage *page = user_data;
	EAccountsWindow *accounts_window;

	g_return_if_fail (E_IS_COLLECTION_WIZARD_PAGE (page));

	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (page)));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	if (!e_collection_account_wizard_prev (page->collection_wizard)) {
		e_collection_account_wizard_abort (page->collection_wizard);
		e_accounts_window_activate_page (accounts_window, -1);
	} else {
		collection_wizard_page_update_button_captions (page);
	}
}

static void
collection_wizard_next_button_clicked_cb (GtkButton *button,
					  gpointer user_data)
{
	ECollectionWizardPage *page = user_data;
	EAccountsWindow *accounts_window;
	gboolean is_finish_page;

	g_return_if_fail (E_IS_COLLECTION_WIZARD_PAGE (page));

	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (page)));
	g_return_if_fail (E_IS_ACCOUNTS_WINDOW (accounts_window));

	is_finish_page = e_collection_account_wizard_is_finish_page (page->collection_wizard);

	if (e_collection_account_wizard_next (page->collection_wizard)) {
		if (is_finish_page) {
			collection_wizard_page_wizard_done (page, NULL);
		} else {
			collection_wizard_page_update_button_captions (page);
		}
	}
}
#endif

static void
collection_wizard_page_constructed (GObject *object)
{
	EAccountsWindow *accounts_window;
	ECollectionWizardPage *page;
#ifdef RUN_INSIDE_ACCOUNTS_WINDOW
	GtkWidget *widget;
	GtkWidget *vbox, *hbox;
#endif

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_collection_wizard_page_parent_class)->constructed (object);

	page = E_COLLECTION_WIZARD_PAGE (object);
	accounts_window = E_ACCOUNTS_WINDOW (e_extension_get_extensible (E_EXTENSION (page)));

	g_signal_connect (accounts_window, "add-source",
		G_CALLBACK (collection_wizard_page_add_source_cb), object);

#ifdef RUN_INSIDE_ACCOUNTS_WINDOW
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);

	widget = e_collection_account_wizard_new (e_accounts_window_get_registry (accounts_window));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	page->collection_wizard = E_COLLECTION_ACCOUNT_WIZARD (widget);

	g_signal_connect_swapped (page->collection_wizard, "done",
		G_CALLBACK (collection_wizard_page_wizard_done), page);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	g_object_set (G_OBJECT (hbox),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	widget = e_dialog_button_new_with_icon ("go-previous", _("_Back"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	page->prev_button = GTK_BUTTON (widget);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (collection_wizard_back_button_clicked_cb), page);

	widget = e_dialog_button_new_with_icon ("go-next", _("_Next"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		"can-default", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	page->next_button = GTK_BUTTON (widget);

	e_binding_bind_property (
		page->collection_wizard, "can-run",
		widget, "sensitive",
		G_BINDING_DEFAULT);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (collection_wizard_next_button_clicked_cb), page);

	page->page_index = e_accounts_window_add_page (accounts_window, vbox);

	gtk_widget_grab_default (GTK_WIDGET (page->next_button));
#endif
}

static void
e_collection_wizard_page_class_init (ECollectionWizardPageClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = collection_wizard_page_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_ACCOUNTS_WINDOW;
}

static void
e_collection_wizard_page_class_finalize (ECollectionWizardPageClass *class)
{
}

static void
e_collection_wizard_page_init (ECollectionWizardPage *extension)
{
}

void
e_collection_wizard_page_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_collection_wizard_page_register_type (type_module);
}
