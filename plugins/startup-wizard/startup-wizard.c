/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  JP Rosevear <jpr@novell.com>
 *  Copyright (C) 2005 Novell, Inc.
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

#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include "widgets/e-timezone-dialog/e-timezone-dialog.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-gtk-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-import.h"
#include "shell/es-event.h"
#include "mail/em-config.h"
#include "mail/em-account-editor.h"
#include "calendar/gui/calendar-config.h"

#define IMPORT_TIMEZONE_DIALOG "StartupWizard::TimezoneDialog"

void startup_wizard (EPlugin *ep, ESEventTargetUpgrade *target);
GtkWidget *startup_wizard_timezone_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
GtkWidget *startup_wizard_importer_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
gboolean startup_wizard_check (EPlugin *ep, EConfigHookPageCheckData *check_data);
void startup_wizard_commit (EPlugin *ep, EMConfigTargetAccount *target);
void startup_wizard_abort (EPlugin *ep, EMConfigTargetAccount *target);

static EImport *import;
static EImportTargetHome *import_target;
static EImportImporter *import_importer;
static GtkWidget *import_dialog, *import_progress, *import_label;
static GSList *import_iterator, *import_importers;

static void
startup_wizard_delete (void) {
	gtk_main_quit ();
	_exit (0);
}

void
startup_wizard (EPlugin *ep, ESEventTargetUpgrade *target)
{
	GConfClient *client;
	GSList *accounts;
	EMAccountEditor *emae;
	GnomeDruidPageEdge *start_page;
	
	client = gconf_client_get_default ();
	accounts = gconf_client_get_list (client, "/apps/evolution/mail/accounts", GCONF_VALUE_STRING, NULL);
	g_object_unref (client);
	
	if (accounts != NULL) {
		g_slist_foreach (accounts, (GFunc) g_free, NULL);
		g_slist_free (accounts);

		return;
	}	

	/** @HookPoint-EMConfig: New Mail Account Wizard
	 * @Id: org.gnome.evolution.mail.config.accountWizard
	 * @Type: E_CONFIG_DRUID
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetAccount
	 *
	 * The new mail account druid.
	 */
	emae = em_account_editor_new (NULL, EMAE_DRUID, "org.gnome.evolution.mail.config.accountWizard");

	gtk_window_set_title (GTK_WINDOW (emae->editor), _("Evolution Setup Assistant"));

	start_page = GNOME_DRUID_PAGE_EDGE (e_config_page_get ((EConfig *) emae->config, "0.start"));
	gnome_druid_page_edge_set_title (start_page, _("Welcome"));
	gnome_druid_page_edge_set_text (start_page, _(""
					"Welcome to Evolution. The next few screens will allow Evolution to connect "
					"to your email accounts, and to import files from other applications. \n"
					"\n"
					"Please click the \"Forward\" button to continue. "));
	g_signal_connect (emae->editor, "delete-event", G_CALLBACK (startup_wizard_delete), NULL);
	
	gtk_widget_show (emae->editor);
	gtk_main ();
}

GtkWidget *
startup_wizard_timezone_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	ETimezoneDialog *etd;
	GtkWidget *page;
	icaltimezone *zone;
	
	etd = e_timezone_dialog_new ();
	g_object_set_data (G_OBJECT (hook_data->config), IMPORT_TIMEZONE_DIALOG, etd);
	
	page = gnome_druid_page_standard_new_with_vals ("Timezone", NULL, NULL);
	e_timezone_dialog_reparent (etd, GNOME_DRUID_PAGE_STANDARD (page)->vbox);

	zone = calendar_config_get_icaltimezone	();
	if (zone)
		e_timezone_dialog_set_timezone (etd, zone);
	
	gnome_druid_append_page (GNOME_DRUID (hook_data->parent), GNOME_DRUID_PAGE (page));

	return GTK_WIDGET (page);
}

GtkWidget *
startup_wizard_importer_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	GtkWidget *page, *label, *sep, *table;
	GSList *l;
	int row=0;

	if (import == NULL) {
		import = e_import_new("org.gnome.evolution.shell.importer");
		import_target = e_import_target_new_home(import, g_get_home_dir());
		import_importers = e_import_get_importers(import, (EImportTarget *)import_target);
	}

	if (import_importers == NULL)
		return NULL;

	page = gnome_druid_page_standard_new_with_vals ("Importing files", NULL, NULL);

	label = gtk_label_new (_("Please select the information that you would like to import:"));
	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (page)->vbox), label, FALSE, FALSE, 3);

	sep = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (page)->vbox), sep, FALSE, FALSE, 3);
	
	table = gtk_table_new(g_slist_length(import_importers), 2, FALSE);
	for (l = import_importers; l; l = l->next) {
		EImportImporter *eii = l->data;
		char *str;
		GtkWidget *w, *label;

		w = e_import_get_widget(import, (EImportTarget *)import_target, eii);

		str = g_strdup_printf(_("From %s:"), eii->name);
		label = gtk_label_new(str);
		gtk_widget_show(label);
		g_free(str);

		gtk_misc_set_alignment((GtkMisc *)label, 0, .5); 

		gtk_table_attach((GtkTable *)table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
		if (w)
			gtk_table_attach((GtkTable *)table, w, 1, 2, row, row+1, GTK_FILL, 0, 3, 0);
		row++;
	}
	gtk_widget_show(table);
	gtk_box_pack_start((GtkBox *)((GnomeDruidPageStandard *)page)->vbox, table, FALSE, FALSE, 3);
	
	gnome_druid_append_page (GNOME_DRUID (hook_data->parent), GNOME_DRUID_PAGE (page));

	return page;
}

static void
import_status(EImport *import, const char *what, int pc, void *d)
{
	gtk_progress_bar_set_fraction((GtkProgressBar *)import_progress, (gfloat)(pc/100.0));
	gtk_progress_bar_set_text((GtkProgressBar *)import_progress, what);
}

static void
import_dialog_response(GtkDialog *d, guint button, void *data)
{
	if (button == GTK_RESPONSE_CANCEL)
		e_import_cancel(import, (EImportTarget *)import_target, import_importer);
}

static void
import_done(EImport *ei, void *d)
{
	if (import_iterator && (import_iterator = import_iterator->next)) {
		import_status(ei, "", 0, NULL);
		import_importer = import_iterator->data;
		e_import_import(import, (EImportTarget *)import_target, import_importer, import_status, import_done, NULL);
	} else {
		gtk_widget_destroy(import_dialog);

		g_slist_free(import_importers);
		import_importers = NULL;
		import_importer = NULL;
		e_import_target_free(import, (EImportTarget *)import_target);
		import_target = NULL;
		g_object_unref(import);
		import = NULL;

		gtk_main_quit();
	}
}

void
startup_wizard_commit (EPlugin *ep, EMConfigTargetAccount *target)
{
	EConfig *ec = ((EConfigTarget *)target)->config;
	ETimezoneDialog *etd;
	icaltimezone *zone;

	/* Set Timezone */
	etd = g_object_get_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG);
	if (etd) {
		zone = e_timezone_dialog_get_timezone (E_TIMEZONE_DIALOG (etd));
		if (zone)
			calendar_config_set_timezone (icaltimezone_get_display_name (zone));

		/* Need to do this otherwise the timezone widget gets destroyed but the
		   timezone object isn't, and we can get a crash like #22047.  */
		g_object_unref (etd);
		g_object_set_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG, NULL);
	}

	if (import_importers) {
		import_iterator = import_importers;
		import_importer = import_iterator->data;

		import_dialog = e_error_new(NULL, "shell:importing", _("Importing data."), NULL);
		g_signal_connect(import_dialog, "response", G_CALLBACK(import_dialog_response), NULL);
		import_label = gtk_label_new(_("Please wait"));
		import_progress = gtk_progress_bar_new();
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), import_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), import_progress, FALSE, FALSE, 0);
		gtk_widget_show_all(import_dialog);

		e_import_import(import, (EImportTarget *)import_target, import_importer, import_status, import_done, NULL);
	} else {
		gtk_main_quit();
	}
}

void
startup_wizard_abort (EPlugin *ep, EMConfigTargetAccount *target)
{
	EConfig *ec = ((EConfigTarget *)target)->config;
	ETimezoneDialog *etd;

	/* We're doing an _exit(), so i dont see why we're bothering to do
	   any cleanup whatsoever here ... */

	etd = g_object_get_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG);
	if (etd) {
		/* Need to do this otherwise the timezone widget gets destroyed but the
		   timezone object isn't, and we can get a crash like #22047.  */
		g_object_unref (etd);
		g_object_set_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG, NULL);
	}

	gtk_main_quit ();
	_exit (0);
}
