/*
 * e-mail-config-local-accounts.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <libebackend/libebackend.h>

#include <mail/e-mail-config-service-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_LOCAL_BACKEND \
	(e_mail_config_local_backend_get_type ())
#define E_MAIL_CONFIG_LOCAL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_LOCAL_BACKEND, EMailConfigLocalBackend))
#define E_MAIL_CONFIG_LOCAL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_LOCAL_BACKEND, EMailConfigLocalBackendClass))
#define E_IS_MAIL_CONFIG_LOCAL_BACKEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_LOCAL_BACKEND))
#define E_IS_MAIL_CONFIG_LOCAL_BACKEND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_LOCAL_BACKEND))
#define E_MAIL_CONFIG_LOCAL_BACKEND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_LOCAL_BACKEND, EMailConfigLocalBackendClass))

typedef struct _EMailConfigLocalBackend EMailConfigLocalBackend;
typedef struct _EMailConfigLocalBackendClass EMailConfigLocalBackendClass;

typedef EMailConfigLocalBackend EMailConfigMhBackend;
typedef EMailConfigLocalBackendClass EMailConfigMhBackendClass;

typedef EMailConfigLocalBackend EMailConfigMboxBackend;
typedef EMailConfigLocalBackendClass EMailConfigMboxBackendClass;

typedef EMailConfigLocalBackend EMailConfigMaildirBackend;
typedef EMailConfigLocalBackendClass EMailConfigMaildirBackendClass;

typedef EMailConfigLocalBackend EMailConfigSpoolDirBackend;
typedef EMailConfigLocalBackendClass EMailConfigSpoolDirBackendClass;

typedef EMailConfigLocalBackend EMailConfigSpoolFileBackend;
typedef EMailConfigLocalBackendClass EMailConfigSpoolFileBackendClass;

/* XXX For lack of a better place for this... */
typedef EMailConfigServiceBackend EMailConfigNoneBackend;
typedef EMailConfigServiceBackendClass EMailConfigNoneBackendClass;

struct _EMailConfigLocalBackend {
	EMailConfigServiceBackend parent;

	GtkWidget *path_error_image;
};

struct _EMailConfigLocalBackendClass {
	EMailConfigServiceBackendClass parent_class;

	const gchar *file_chooser_label;
	const gchar *file_chooser_title;
	GtkFileChooserAction file_chooser_action;
	const gchar *file_error_message;

};

/* Forward Declarations */
void		e_mail_config_local_accounts_register_types
						(GTypeModule *type_module);
GType		e_mail_config_local_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_mh_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_mbox_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_maildir_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_spool_dir_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_spool_file_backend_get_type
						(void) G_GNUC_CONST;
GType		e_mail_config_none_backend_get_type
						(void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailConfigLocalBackend,
	e_mail_config_local_backend,
	E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
	G_TYPE_FLAG_ABSTRACT,
	/* no custom code */)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigMhBackend,
	e_mail_config_mh_backend,
	E_TYPE_MAIL_CONFIG_LOCAL_BACKEND)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigMboxBackend,
	e_mail_config_mbox_backend,
	E_TYPE_MAIL_CONFIG_LOCAL_BACKEND)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigMaildirBackend,
	e_mail_config_maildir_backend,
	E_TYPE_MAIL_CONFIG_LOCAL_BACKEND)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigSpoolDirBackend,
	e_mail_config_spool_dir_backend,
	E_TYPE_MAIL_CONFIG_LOCAL_BACKEND)

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigSpoolFileBackend,
	e_mail_config_spool_file_backend,
	E_TYPE_MAIL_CONFIG_LOCAL_BACKEND)

/* XXX For lack of a better place for this... */
G_DEFINE_DYNAMIC_TYPE (
	EMailConfigNoneBackend,
	e_mail_config_none_backend,
	E_TYPE_MAIL_CONFIG_SERVICE_BACKEND)

static void
mail_config_local_backend_file_set_cb (GtkFileChooserButton *file_chooser_button,
                                       CamelLocalSettings *local_settings)
{
	GtkFileChooser *file_chooser;
	gchar *path;

	file_chooser = GTK_FILE_CHOOSER (file_chooser_button);

	path = gtk_file_chooser_get_filename (file_chooser);
	camel_local_settings_set_path (local_settings, path);
	g_free (path);
}

static void
mail_config_local_backend_insert_widgets (EMailConfigServiceBackend *backend,
                                          GtkBox *parent)
{
	CamelSettings *settings;
	EMailConfigLocalBackend *local_backend;
	EMailConfigLocalBackendClass *class;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	const gchar *path;

	class = E_MAIL_CONFIG_LOCAL_BACKEND_GET_CLASS (backend);
	local_backend = E_MAIL_CONFIG_LOCAL_BACKEND (backend);
	settings = e_mail_config_service_backend_get_settings (backend);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (parent, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (class->file_chooser_label);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_file_chooser_button_new (
		class->file_chooser_title,
		class->file_chooser_action);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "file-set",
		G_CALLBACK (mail_config_local_backend_file_set_cb),
		CAMEL_LOCAL_SETTINGS (settings));

	path = camel_local_settings_get_path (CAMEL_LOCAL_SETTINGS (settings));
	if (path != NULL)
		gtk_file_chooser_set_filename (
			GTK_FILE_CHOOSER (widget), path);

	widget = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_BUTTON);
	g_object_set (G_OBJECT (widget),
		"visible", FALSE,
		"has-tooltip", TRUE,
		"tooltip-text", class->file_error_message,
		NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	local_backend->path_error_image = widget;  /* do not reference */
}

static gboolean
mail_config_local_backend_check_complete (EMailConfigServiceBackend *backend)
{
	EMailConfigLocalBackend *local_backend;
	CamelSettings *settings;
	CamelLocalSettings *local_settings;
	const gchar *path;
	gboolean complete;

	local_backend = E_MAIL_CONFIG_LOCAL_BACKEND (backend);
	settings = e_mail_config_service_backend_get_settings (backend);

	local_settings = CAMEL_LOCAL_SETTINGS (settings);
	path = camel_local_settings_get_path (local_settings);

	complete = (path != NULL && *path != '\0');

	gtk_widget_set_visible (local_backend->path_error_image, !complete);

	return complete;
}

static void
mail_config_local_backend_commit_changes (EMailConfigServiceBackend *backend)
{
	/* CamelLocalSettings "path" property is already up-to-date,
	 * and it's bound to the appropriate ESourceExtension property,
	 * so nothing to do here. */
}

static void
e_mail_config_local_backend_class_init (EMailConfigLocalBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->insert_widgets = mail_config_local_backend_insert_widgets;
	backend_class->check_complete = mail_config_local_backend_check_complete;
	backend_class->commit_changes = mail_config_local_backend_commit_changes;
}

static void
e_mail_config_local_backend_class_finalize (EMailConfigLocalBackendClass *class)
{
}

static void
e_mail_config_local_backend_init (EMailConfigLocalBackend *backend)
{
}

static void
e_mail_config_mh_backend_class_init (EMailConfigLocalBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "mh";

	class->file_chooser_label = _("Mail _Directory:");
	class->file_chooser_title = _("Choose a MH mail directory");
	class->file_chooser_action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
	class->file_error_message = _("MH mail directory cannot be empty");
}

static void
e_mail_config_mh_backend_class_finalize (EMailConfigLocalBackendClass *class)
{
}

static void
e_mail_config_mh_backend_init (EMailConfigLocalBackend *backend)
{
}

static void
e_mail_config_mbox_backend_class_init (EMailConfigLocalBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "mbox";

	class->file_chooser_label = _("Local Delivery _File:");
	class->file_chooser_title = _("Choose a local delivery file");
	class->file_chooser_action = GTK_FILE_CHOOSER_ACTION_OPEN;
	class->file_error_message = _("Local delivery file cannot be empty");
}

static void
e_mail_config_mbox_backend_class_finalize (EMailConfigLocalBackendClass *class)
{
}

static void
e_mail_config_mbox_backend_init (EMailConfigLocalBackend *backend)
{
}

static void
e_mail_config_maildir_backend_class_init (EMailConfigLocalBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "maildir";

	class->file_chooser_label = _("Mail _Directory:");
	class->file_chooser_title = _("Choose a Maildir mail directory");
	class->file_chooser_action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
	class->file_error_message = _("Maildir mail directory cannot be empty");
}

static void
e_mail_config_maildir_backend_class_finalize (EMailConfigLocalBackendClass *class)
{
}

static void
e_mail_config_maildir_backend_init (EMailConfigLocalBackend *backend)
{
}

static void
e_mail_config_spool_dir_backend_class_init (EMailConfigLocalBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "spool";

	class->file_chooser_label = _("Spool _File:");
	class->file_chooser_title = _("Choose a mbox spool file");
	class->file_chooser_action = GTK_FILE_CHOOSER_ACTION_OPEN;
	class->file_error_message = _("Mbox spool file cannot be empty");
}

static void
e_mail_config_spool_dir_backend_class_finalize (EMailConfigLocalBackendClass *class)
{
}

static void
e_mail_config_spool_dir_backend_init (EMailConfigLocalBackend *backend)
{
}

static void
e_mail_config_spool_file_backend_class_init (EMailConfigLocalBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "spooldir";

	class->file_chooser_label = _("Spool _Directory:");
	class->file_chooser_title = _("Choose a mbox spool directory");
	class->file_chooser_action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
	class->file_error_message = _("Mbox spool directory cannot be empty");
}

static void
e_mail_config_spool_file_backend_class_finalize (EMailConfigLocalBackendClass *class)
{
}

static void
e_mail_config_spool_file_backend_init (EMailConfigLocalBackend *backend)
{
}

static gboolean
e_mail_config_none_backend_get_selectable (EMailConfigServiceBackend *backend)
{
	return TRUE;
}

static void
e_mail_config_none_backend_class_init (EMailConfigServiceBackendClass *class)
{
	class->backend_name = "none";
	class->get_selectable = e_mail_config_none_backend_get_selectable;
}

static void
e_mail_config_none_backend_class_finalize (EMailConfigServiceBackendClass *class)
{
}

static void
e_mail_config_none_backend_init (EMailConfigServiceBackend *backend)
{
}

void
e_mail_config_local_accounts_register_types (GTypeModule *type_module)
{
	/* Abstract base type */
	e_mail_config_local_backend_register_type (type_module);

	/* Concrete sub-types */
	e_mail_config_mh_backend_register_type (type_module);
	e_mail_config_mbox_backend_register_type (type_module);
	e_mail_config_maildir_backend_register_type (type_module);
	e_mail_config_spool_dir_backend_register_type (type_module);
	e_mail_config_spool_file_backend_register_type (type_module);

	/* XXX For lack of a better place for this... */
	e_mail_config_none_backend_register_type (type_module);
}

