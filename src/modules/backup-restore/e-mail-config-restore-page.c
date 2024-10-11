/*
 * e-mail-config-restore-page.c
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-config-restore-page.h"

struct _EMailConfigRestorePagePrivate {
	GtkWidget *toggle_button;  /* not referenced */
	GtkWidget *file_chooser;   /* not referenced */
	GtkWidget *alert_bar;      /* not referenced */
	gchar *filename;
};

enum {
	PROP_0,
	PROP_FILENAME
};

/* Forward Declarations */
static void	e_mail_config_restore_page_alert_sink_init
					(EAlertSinkInterface *iface);
static void	e_mail_config_restore_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailConfigRestorePage, e_mail_config_restore_page, GTK_TYPE_SCROLLED_WINDOW, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailConfigRestorePage)
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_ALERT_SINK, e_mail_config_restore_page_alert_sink_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_restore_page_interface_init))

static void
mail_config_restore_page_update_filename (EMailConfigRestorePage *page)
{
	GtkToggleButton *toggle_button;
	GtkFileChooser *file_chooser;
	gchar *filename = NULL;
	GError *local_error = NULL;

	file_chooser = GTK_FILE_CHOOSER (page->priv->file_chooser);
	toggle_button = GTK_TOGGLE_BUTTON (page->priv->toggle_button);

	e_alert_bar_clear (E_ALERT_BAR (page->priv->alert_bar));

	if (gtk_toggle_button_get_active (toggle_button))
		filename = gtk_file_chooser_get_filename (file_chooser);

	if (!evolution_backup_restore_validate_backup_file (filename, &local_error)) {
		if (filename != NULL) {
			if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
				e_alert_submit (
					E_ALERT_SINK (page),
					"org.gnome.backup-restore:cannot-restore",
					local_error->message, NULL);
			} else {
				e_alert_submit (
					E_ALERT_SINK (page),
					"org.gnome.backup-restore:invalid-backup",
					filename, NULL);
			}
			g_free (filename);
			filename = NULL;
		}
	}

	g_clear_error (&local_error);

	g_free (page->priv->filename);
	page->priv->filename = filename;

	g_object_notify (G_OBJECT (page), "filename");

	e_mail_config_page_changed (E_MAIL_CONFIG_PAGE (page));
}

static void
mail_config_restore_page_toggled_cb (GtkToggleButton *toggle_button,
                                     EMailConfigRestorePage *page)
{
	mail_config_restore_page_update_filename (page);
}

static void
mail_config_restore_page_file_set_cb (GtkFileChooser *file_chooser,
                                      EMailConfigRestorePage *page)
{
	mail_config_restore_page_update_filename (page);
}

static void
mail_config_restore_page_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			g_value_set_string (
				value,
				e_mail_config_restore_page_get_filename (
				E_MAIL_CONFIG_RESTORE_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_restore_page_finalize (GObject *object)
{
	EMailConfigRestorePage *self = E_MAIL_CONFIG_RESTORE_PAGE (object);

	g_free (self->priv->filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_restore_page_parent_class)->finalize (object);
}

static gboolean
is_xz_available (void)
{
	gchar *path;

	path = g_find_program_in_path ("xz");

	g_free (path);

	return path != NULL;
}

static void
mail_config_restore_page_constructed (GObject *object)
{
	EMailConfigRestorePage *page;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *main_box;
	GtkFileFilter *filter, *backup_filter;
	const gchar *text;

	page = E_MAIL_CONFIG_RESTORE_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_restore_page_parent_class)->constructed (object);

	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);

	text = _("You can restore Evolution from a backup file.\n\n"
		 "This will restore all your personal data, settings "
		 "mail filters, etc.");
	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("_Restore from a backup file:");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page->priv->toggle_button = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (mail_config_restore_page_toggled_cb), page);

	widget = gtk_file_chooser_button_new (
		_("Choose a backup file to restore"),
		GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (widget), TRUE);

	backup_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (backup_filter, "*.tar.gz");
	if (is_xz_available ())
		gtk_file_filter_add_pattern (backup_filter, "*.tar.xz");
	gtk_file_filter_set_name (backup_filter, _("Backup Files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), backup_filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_filter_set_name (filter, _("All Files (*)"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), filter);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (widget), backup_filter);

	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	page->priv->file_chooser = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "file-set",
		G_CALLBACK (mail_config_restore_page_file_set_cb), page);

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	/* Visibility is bound to the EActivityBar. */

	container = widget;

	widget = e_alert_bar_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	page->priv->alert_bar = widget;  /* not referenced */
	/* EActivityBar controls its own visibility. */

	e_binding_bind_property (
		widget, "visible",
		container, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		page->priv->toggle_button, "active",
		page->priv->file_chooser, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);
}

static void
mail_config_restore_page_submit_alert (EAlertSink *alert_sink,
                                       EAlert *alert)
{
	EMailConfigRestorePage *self = E_MAIL_CONFIG_RESTORE_PAGE (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static gboolean
mail_config_restore_page_check_complete (EMailConfigPage *page)
{
	EMailConfigRestorePage *self = E_MAIL_CONFIG_RESTORE_PAGE (page);
	GtkToggleButton *toggle_button;
	gboolean complete;

	toggle_button = GTK_TOGGLE_BUTTON (self->priv->toggle_button);

	complete =
		!gtk_toggle_button_get_active (toggle_button) ||
		(self->priv->filename != NULL && *self->priv->filename != '\0');

	return complete;
}

static void
e_mail_config_restore_page_class_init (EMailConfigRestorePageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_config_restore_page_get_property;
	object_class->finalize = mail_config_restore_page_finalize;
	object_class->constructed = mail_config_restore_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			"Filename",
			"Selected filename to restore from",
			NULL,
			G_PARAM_READABLE));
}

static void
e_mail_config_restore_page_class_finalize (EMailConfigRestorePageClass *class)
{
}

static void
e_mail_config_restore_page_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = mail_config_restore_page_submit_alert;
}

static void
e_mail_config_restore_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Restore from Backup");
	iface->sort_order = E_MAIL_CONFIG_RESTORE_PAGE_SORT_ORDER;
	iface->check_complete = mail_config_restore_page_check_complete;
}

static void
e_mail_config_restore_page_init (EMailConfigRestorePage *page)
{
	page->priv = e_mail_config_restore_page_get_instance_private (page);
}

void
e_mail_config_restore_page_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_restore_page_register_type (type_module);
}

EMailConfigPage *
e_mail_config_restore_page_new (void)
{
	return g_object_new (E_TYPE_MAIL_CONFIG_RESTORE_PAGE, NULL);
}

const gchar *
e_mail_config_restore_page_get_filename (EMailConfigRestorePage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_RESTORE_PAGE (page), NULL);

	return page->priv->filename;
}

gboolean
evolution_backup_restore_check_prog_exists (const gchar *prog,
					    GError **error)
{
	gchar *path;

	path = g_find_program_in_path (prog);

	if (!path) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, _("Program '%s' not found"), prog);
		return FALSE;
	}

	g_free (path);

	return TRUE;
}

gboolean
evolution_backup_restore_validate_backup_file (const gchar *filename,
					       GError **error)
{
	gchar *command;
	gint result;
	gchar *quotedfname;
	gchar *toolfname;
	const gchar *basedir;

	if (filename == NULL || *filename == '\0')
		return FALSE;

	if (!evolution_backup_restore_check_prog_exists ("tar", error) ||
	    (g_str_has_suffix (filename, ".xz") && !evolution_backup_restore_check_prog_exists ("xz", error)) ||
	    (!g_str_has_suffix (filename, ".xz") && !evolution_backup_restore_check_prog_exists ("gzip", error)))
		return FALSE;

	/* FIXME We should be using g_spawn_command_line_sync() here. */

	basedir = EVOLUTION_TOOLSDIR;
	quotedfname = g_shell_quote (filename);
	toolfname = g_build_filename (basedir, "evolution-backup", NULL);

	command = g_strdup_printf ("%s --check %s", toolfname, quotedfname);
	result = system (command);

	g_free (command);
	g_free (quotedfname);
	g_free (toolfname);

#ifdef HAVE_SYS_WAIT_H
	g_message (
		"Sanity check result %d:%d %d",
		WIFEXITED (result), WEXITSTATUS (result), result);

	return WIFEXITED (result) && (WEXITSTATUS (result) == 0);
#else
	return (result == 0);
#endif
}

