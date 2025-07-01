/*
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
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "em-folder-properties.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>

#include <libemail-engine/libemail-engine.h>

#include <e-util/e-util.h>

#include "e-mail-backend.h"
#include "e-mail-folder-tweaks.h"
#include "e-mail-label-dialog.h"
#include "e-mail-notes.h"
#include "e-mail-ui-session.h"
#include "em-config.h"
#include "em-folder-selection-button.h"
#include "mail-vfolder-ui.h"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EFlag *flag;
	EActivity *activity;
	CamelStore *store;
	gchar *folder_name;
	CamelFolder *folder;
	GtkWindow *parent_window;
	CamelFolderQuotaInfo *quota_info;
	gint total;
	gint unread;
	gboolean cancelled;
	GSList *available_labels; /* gchar * */
};

static void
async_context_free (AsyncContext *context)
{
	e_flag_free (context->flag);

	g_clear_object (&context->activity);
	g_clear_object (&context->store);
	g_clear_object (&context->folder);
	g_clear_object (&context->parent_window);

	g_slist_free_full (context->available_labels, g_free);
	g_free (context->folder_name);

	if (context->quota_info != NULL)
		camel_folder_quota_info_free (context->quota_info);

	g_slice_free (AsyncContext, context);
}

static void
emfp_free (EConfig *ec,
           GSList *items,
           gpointer data)
{
	g_slist_free (items);
}

typedef struct _FolderTweaksData {
	gchar *folder_uri;
	EMailFolderTweaks *tweaks; /* referenced */
	GtkWidget *widget; /* not referenced */
} FolderTweaksData;

static FolderTweaksData *
folder_tweaks_data_new (const gchar *folder_uri,
			EMailFolderTweaks *tweaks,
			GtkWidget *widget)
{
	FolderTweaksData *ftd;

	ftd = g_slice_new0 (FolderTweaksData);
	ftd->folder_uri = g_strdup (folder_uri);
	ftd->tweaks = g_object_ref (tweaks);
	ftd->widget = widget;

	return ftd;
}

static void
folder_tweaks_data_free (gpointer ptr,
			 GClosure *closure)
{
	FolderTweaksData *ftd = ptr;

	if (ftd) {
		g_free (ftd->folder_uri);
		g_object_unref (ftd->tweaks);
		g_slice_free (FolderTweaksData, ftd);
	}
}

static void
tweaks_custom_icon_check_toggled_cb (GtkToggleButton *toggle_button,
				     FolderTweaksData *ftd)
{
	GtkWidget *image;

	g_return_if_fail (ftd != NULL);

	if (!gtk_toggle_button_get_active (toggle_button)) {
		e_mail_folder_tweaks_set_icon_filename (ftd->tweaks, ftd->folder_uri, NULL);
		return;
	}

	image = gtk_button_get_image (GTK_BUTTON (ftd->widget));

	if (image && gtk_image_get_storage_type (GTK_IMAGE (image))) {
		GIcon *icon = NULL;

		gtk_image_get_gicon (GTK_IMAGE (image), &icon, NULL);

		if (G_IS_FILE_ICON (icon)) {
			GFile *file;

			file = g_file_icon_get_file (G_FILE_ICON (icon));
			if (file) {
				gchar *filename;

				filename = g_file_get_path (file);
				if (filename) {
					e_mail_folder_tweaks_set_icon_filename (ftd->tweaks, ftd->folder_uri, filename);

					g_free (filename);
				}
			}
		}
	}
}

static void
tweaks_custom_icon_button_clicked_cb (GtkWidget *button,
				      FolderTweaksData *ftd)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	GFile *file;

	toplevel = gtk_widget_get_toplevel (button);
	dialog = e_image_chooser_dialog_new (_("Select Custom Icon"),
		GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL);

	file = e_image_chooser_dialog_run (E_IMAGE_CHOOSER_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	if (file) {
		gchar *filename;

		filename = g_file_get_path (file);
		if (filename) {
			GtkWidget *image;
			GIcon *custom_icon;

			image = gtk_button_get_image (GTK_BUTTON (button));
			custom_icon = g_file_icon_new (file);

			gtk_image_set_from_gicon (GTK_IMAGE (image), custom_icon, GTK_ICON_SIZE_BUTTON);

			g_clear_object (&custom_icon);

			e_mail_folder_tweaks_set_icon_filename (ftd->tweaks, ftd->folder_uri, filename);

			g_free (filename);
		}

		g_object_unref (file);
	}
}

static void
emfp_add_tweaks_custom_icon_row (GtkBox *vbox,
				 const gchar *folder_uri,
				 EMailFolderTweaks *tweaks)
{
	GtkWidget *checkbox;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *hbox;
	gchar *icon_filename;

	g_return_if_fail (GTK_IS_BOX (vbox));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (vbox, hbox, FALSE, FALSE, 0);

	checkbox = gtk_check_button_new_with_mnemonic (_("_Use custom icon"));
	gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);

	button = gtk_button_new ();
	image = gtk_image_new_from_icon_name (NULL, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);

	icon_filename = e_mail_folder_tweaks_dup_icon_filename (tweaks, folder_uri);
	if (icon_filename &&
	    g_file_test (icon_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		GFile *file;
		GIcon *custom_icon;

		file = g_file_new_for_path (icon_filename);
		custom_icon = g_file_icon_new (file);

		g_clear_object (&file);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
		gtk_image_set_from_gicon (GTK_IMAGE (image), custom_icon, GTK_ICON_SIZE_BUTTON);

		g_clear_object (&custom_icon);
	}

	g_free (icon_filename);

	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	e_binding_bind_property (
		checkbox, "active",
		button, "sensitive",
		G_BINDING_DEFAULT |
		G_BINDING_SYNC_CREATE);

	g_signal_connect_data (checkbox, "toggled",
		G_CALLBACK (tweaks_custom_icon_check_toggled_cb),
		folder_tweaks_data_new (folder_uri, tweaks, button), folder_tweaks_data_free, 0);

	g_signal_connect_data (button, "clicked",
		G_CALLBACK (tweaks_custom_icon_button_clicked_cb),
		folder_tweaks_data_new (folder_uri, tweaks, NULL), folder_tweaks_data_free, 0);

	gtk_widget_show_all (hbox);
}

static void
tweaks_text_color_check_toggled_cb (GtkToggleButton *toggle_button,
				    FolderTweaksData *ftd)
{
	g_return_if_fail (ftd != NULL);

	if (gtk_toggle_button_get_active (toggle_button)) {
		GdkRGBA rgba;

		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (ftd->widget), &rgba);

		e_mail_folder_tweaks_set_color (ftd->tweaks, ftd->folder_uri, &rgba);
	} else {
		e_mail_folder_tweaks_set_color (ftd->tweaks, ftd->folder_uri, NULL);
	}
}

static void
tweaks_text_color_button_color_set_cb (GtkColorButton *col_button,
				       FolderTweaksData *ftd)
{
	GdkRGBA rgba;

	g_return_if_fail (ftd != NULL);

	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (col_button), &rgba);

	e_mail_folder_tweaks_set_color (ftd->tweaks, ftd->folder_uri, &rgba);
}

static void
emfp_add_tweaks_text_color_row (GtkBox *vbox,
				const gchar *folder_uri,
				EMailFolderTweaks *tweaks)
{
	GtkWidget *checkbox;
	GtkWidget *button;
	GtkWidget *hbox;
	GdkRGBA rgba;

	g_return_if_fail (GTK_IS_BOX (vbox));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (vbox, hbox, FALSE, FALSE, 0);

	checkbox = gtk_check_button_new_with_mnemonic (_("Use te_xt color"));
	gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);

	button = gtk_color_button_new ();

	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	if (e_mail_folder_tweaks_get_color (tweaks, folder_uri, &rgba)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
		gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), &rgba);
	}

	e_binding_bind_property (
		checkbox, "active",
		button, "sensitive",
		G_BINDING_DEFAULT |
		G_BINDING_SYNC_CREATE);

	g_signal_connect_data (checkbox, "toggled",
		G_CALLBACK (tweaks_text_color_check_toggled_cb),
		folder_tweaks_data_new (folder_uri, tweaks, button), folder_tweaks_data_free, 0);

	g_signal_connect_data (button, "color-set",
		G_CALLBACK (tweaks_text_color_button_color_set_cb),
		folder_tweaks_data_new (folder_uri, tweaks, NULL), folder_tweaks_data_free, 0);

	gtk_widget_show_all (hbox);
}

static void
mail_identity_combo_box_changed_cb (GtkComboBox *combo_box,
                                    EMailSendAccountOverride *account_override)
{
	const gchar *folder_uri;
	gchar *identity_uid = NULL, *alias_name = NULL, *alias_address = NULL;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));

	folder_uri = g_object_get_data (G_OBJECT (combo_box), "sao-folder-uri");
	g_return_if_fail (folder_uri != NULL);

	if (!e_mail_identity_combo_box_get_active_uid (E_MAIL_IDENTITY_COMBO_BOX (combo_box),
		&identity_uid, &alias_name, &alias_address) ||
	    !identity_uid || !*identity_uid) {
		e_mail_send_account_override_remove_for_folder (account_override, folder_uri);
	} else {
		e_mail_send_account_override_set_for_folder (account_override, folder_uri, identity_uid, alias_name, alias_address);
	}

	g_free (identity_uid);
	g_free (alias_name);
	g_free (alias_address);
}

static gint
add_text_row (GtkGrid *grid,
	      gint row,
	      const gchar *description,
	      const gchar *text,
	      gboolean selectable)
{
	GtkWidget *label;

	g_return_val_if_fail (grid != NULL, row);
	g_return_val_if_fail (description != NULL, row);
	g_return_val_if_fail (text != NULL, row);

	label = gtk_label_new (description);
	gtk_widget_show (label);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_grid_attach (grid, label, 0, row, 1, 1);

	label = gtk_label_new (text);
	if (selectable) {
		gtk_label_set_selectable (GTK_LABEL (label), selectable);
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
	}
	gtk_widget_show (label);
	gtk_label_set_xalign (GTK_LABEL (label), 1.0);
	gtk_widget_set_hexpand (label, TRUE);
	gtk_grid_attach (grid, label, 1, row, 1, 1);

	return row + 1;
}

static gint
add_numbered_row (GtkGrid *grid,
                  gint row,
                  const gchar *description,
                  const gchar *format,
                  gint num)
{
	gchar *str;

	g_return_val_if_fail (grid != NULL, row);
	g_return_val_if_fail (description != NULL, row);
	g_return_val_if_fail (format != NULL, row);

	str = g_strdup_printf (format, num);

	row = add_text_row (grid, row, description, str, FALSE);

	g_free (str);

	return row;
}

typedef struct _ThreeStateData {
	CamelFolder *folder;
	gchar *property_name;
	gulong handler_id;
} ThreeStateData;

static void
three_state_data_free (gpointer data,
		       GClosure *closure)
{
	ThreeStateData *tsd = data;

	if (tsd) {
		g_clear_object (&tsd->folder);
		g_free (tsd->property_name);
		g_slice_free (ThreeStateData, tsd);
	}
}

static void
emfp_three_state_toggled_cb (GtkToggleButton *widget,
			     gpointer user_data)
{
	ThreeStateData *tsd = user_data;
	CamelThreeState set_to;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
	g_return_if_fail (tsd != NULL);

	g_signal_handler_block (widget, tsd->handler_id);

	if (gtk_toggle_button_get_inconsistent (widget) &&
	    gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_active (widget, FALSE);
		gtk_toggle_button_set_inconsistent (widget, FALSE);
		set_to = CAMEL_THREE_STATE_OFF;
	} else if (!gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_inconsistent (widget, TRUE);
		gtk_toggle_button_set_active (widget, FALSE);
		set_to = CAMEL_THREE_STATE_INCONSISTENT;
	} else {
		set_to = CAMEL_THREE_STATE_ON;
	}

	g_object_set (G_OBJECT (tsd->folder), tsd->property_name, set_to, NULL);

	g_signal_handler_unblock (widget, tsd->handler_id);
}

static GtkWidget *
emfp_get_folder_item (EConfig *ec,
                      EConfigItem *item,
                      GtkWidget *parent,
                      GtkWidget *old,
                      gint position,
                      gpointer data)
{
	GObjectClass *class;
	GParamSpec **properties;
	GtkWidget *widget, *grid;
	AsyncContext *context = data;
	guint ii, n_properties;
	gint row = 0;
	gboolean can_apply_filters = FALSE;
	CamelStore *store;
	CamelSession *session;
	CamelFolderInfoFlags fi_flags = 0;
	const gchar *folder_name;
	MailFolderCache *folder_cache;
	gboolean have_flags;
	ESourceRegistry *registry;
	EShell *shell;
	EMailBackend *mail_backend;
	EMailSendAccountOverride *account_override;
	gchar *folder_uri, *account_uid, *alias_name = NULL, *alias_address = NULL;
	GtkWidget *label;
	gboolean has_mark_seen = FALSE, has_mark_seen_timeout = FALSE;

	if (old)
		return old;

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_widget_show (grid);
	gtk_box_pack_start ((GtkBox *) parent, grid, TRUE, TRUE, 0);

	store = camel_folder_get_parent_store (context->folder);
	folder_name = camel_folder_get_full_name (context->folder);

	if (store) {
		gchar *path;

		path = g_strconcat (camel_service_get_display_name (CAMEL_SERVICE (store)), "/",
			camel_folder_get_full_display_name (context->folder), NULL);
		row = add_text_row (GTK_GRID (grid), row, _("Path:"), path, TRUE);
		g_free (path);
	}

	/* To be on the safe side, ngettext is used here,
	 * see e.g. comment #3 at bug 272567 */
	row = add_numbered_row (
		GTK_GRID (grid), row,
		ngettext (
			"Unread messages:",
			"Unread messages:",
			context->unread),
		"%d", context->unread);

	/* TODO: can this be done in a loop? */
	/* To be on the safe side, ngettext is used here,
	 * see e.g. comment #3 at bug 272567 */
	row = add_numbered_row (
		GTK_GRID (grid), row,
		ngettext (
			"Total messages:",
			"Total messages:",
			context->total),
		"%d", context->total);

	if (context->quota_info) {
		CamelFolderQuotaInfo *info;
		CamelFolderQuotaInfo *quota = context->quota_info;

		for (info = quota; info; info = info->next) {
			gchar *descr, *text, *lefts_str, *total_str;
			guint64 lefts;
			gint procs;

			/* should not happen, but anyway... */
			if (!info->total)
				continue;

			/* Show quota name only when available and we
			 * have more than one quota info. */
			if (info->name && quota->next)
				descr = g_strdup_printf (
					_("Quota usage (%s):"), _(info->name));
			else
				descr = g_strdup_printf (_("Quota usage"));

			procs = (gint) ((((gdouble) info->used) /
				((gdouble) info->total)) * 100.0 + 0.5);

			lefts = info->total - info->used;

			if (info->name && g_ascii_strcasecmp (info->name, "STORAGE") == 0) {
				lefts_str = g_format_size (lefts * 1024);
				total_str = g_format_size (info->total * 1024);
			} else {
				lefts_str = g_strdup_printf ("%" G_GUINT64_FORMAT, lefts);
				total_str = g_strdup_printf ("%" G_GUINT64_FORMAT, info->total);
			}

			/* Translators: the string is related to a quota usage, constructing a text like "75% (250 KB of 1 MB left)"; the ngettext() is on the amount of the 'left' */
			text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d%% (%s of %s left)", "%d%% (%s of %s left)", lefts), procs, lefts_str, total_str);

			g_free (lefts_str);
			g_free (total_str);

			row = add_text_row (GTK_GRID (grid), row, descr, text, FALSE);

			g_free (text);
			g_free (descr);
		}
	}

	session = camel_service_ref_session (CAMEL_SERVICE (store));

	folder_cache = e_mail_session_get_folder_cache (
		E_MAIL_SESSION (session));

	have_flags = mail_folder_cache_get_folder_info_flags (
		folder_cache, store, folder_name, &fi_flags);

	can_apply_filters =
		!CAMEL_IS_VEE_FOLDER (context->folder) &&
		have_flags &&
		(fi_flags & CAMEL_FOLDER_TYPE_MASK) != CAMEL_FOLDER_TYPE_INBOX;

	g_object_unref (session);

	class = G_OBJECT_GET_CLASS (context->folder);
	properties = g_object_class_list_properties (class, &n_properties);

	for (ii = 0; ii < n_properties; ii++) {
		const gchar *blurb;

		if ((properties[ii]->flags & CAMEL_FOLDER_PARAM_PERSISTENT) == 0)
			continue;

		if (!can_apply_filters &&
		    g_strcmp0 (properties[ii]->name, "apply-filters") == 0)
			continue;

		if (!has_mark_seen && g_strcmp0 (properties[ii]->name, "mark-seen") == 0) {
			has_mark_seen = TRUE;
			continue;
		}

		if (!has_mark_seen_timeout && g_strcmp0 (properties[ii]->name, "mark-seen-timeout") == 0) {
			has_mark_seen_timeout = TRUE;
			continue;
		}

		blurb = g_param_spec_get_blurb (properties[ii]);
		if (!blurb)
			continue;

		switch (properties[ii]->value_type) {
			case G_TYPE_BOOLEAN:
				widget = gtk_check_button_new_with_mnemonic (blurb);
				gtk_widget_set_hexpand (widget, TRUE);
				e_binding_bind_property (
					context->folder,
					properties[ii]->name,
					widget, "active",
					G_BINDING_BIDIRECTIONAL |
					G_BINDING_SYNC_CREATE);
				gtk_widget_show (widget);
				gtk_grid_attach (GTK_GRID (grid), widget, 0, row, 2, 1);
				row++;
				break;
			default:
				if (properties[ii]->value_type == CAMEL_TYPE_THREE_STATE) {
					ThreeStateData *tsd;
					GValue value = G_VALUE_INIT;
					CamelThreeState three_state;
					gboolean set_inconsistent = FALSE, set_active = FALSE;

					g_value_init (&value, properties[ii]->value_type);

					g_object_get_property (G_OBJECT (context->folder), properties[ii]->name, &value);
					three_state = g_value_get_enum (&value);
					g_value_unset (&value);

					switch (three_state) {
						case CAMEL_THREE_STATE_ON:
							set_inconsistent = FALSE;
							set_active = TRUE;
							break;
						case CAMEL_THREE_STATE_OFF:
							set_inconsistent = FALSE;
							set_active = FALSE;
							break;
						case CAMEL_THREE_STATE_INCONSISTENT:
							set_inconsistent = TRUE;
							set_active = FALSE;
							break;
					}

					widget = gtk_check_button_new_with_mnemonic (blurb);
					gtk_widget_set_hexpand (widget, TRUE);

					g_object_set (G_OBJECT (widget),
						"inconsistent", set_inconsistent,
						"active", set_active,
						NULL);

					tsd = g_slice_new0 (ThreeStateData);
					tsd->folder = g_object_ref (context->folder);
					tsd->property_name = g_strdup (properties[ii]->name);
					tsd->handler_id = g_signal_connect_data (widget, "toggled",
						G_CALLBACK (emfp_three_state_toggled_cb),
						tsd, three_state_data_free, 0);

					gtk_widget_show (widget);
					gtk_grid_attach (GTK_GRID (grid), widget, 0, row, 2, 1);
					row++;
				} else {
					g_warn_if_reached ();
				}
				break;
		}
	}

	if (has_mark_seen && has_mark_seen_timeout) {
		widget = e_dialog_new_mark_seen_box (context->folder);

		gtk_grid_attach (GTK_GRID (grid), widget, 0, row, 2, 1);
		row++;
	}

	g_free (properties);

	/* add send-account-override setting widgets */
	registry = e_shell_get_registry (e_shell_get_default ());

	/* Translators: Label of a combo with a list of configured accounts where a user can
	   choose which account to use as the sender when composing a message in this folder */
	label = gtk_label_new_with_mnemonic (_("_Send Account Override:"));
	gtk_widget_set_hexpand (label, TRUE);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 2, 1);
	row++;

	widget = g_object_new (
		E_TYPE_MAIL_IDENTITY_COMBO_BOX,
		"registry", registry,
		"allow-none", TRUE,
		"allow-aliases", TRUE,
		"hexpand", TRUE,
		NULL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_widget_set_margin_start (widget, 12);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (grid), widget, 0, row, 2, 1);
	row++;

	shell = e_shell_get_default ();
	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_val_if_fail (mail_backend != NULL, grid);

	account_override = e_mail_backend_get_send_account_override (mail_backend);
	folder_uri = e_mail_folder_uri_from_folder (context->folder);
	account_uid = e_mail_send_account_override_get_for_folder (account_override, folder_uri, &alias_name, &alias_address);

	if (!account_uid || !e_mail_identity_combo_box_set_active_uid (E_MAIL_IDENTITY_COMBO_BOX (widget), account_uid, alias_name, alias_address))
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), account_uid ? account_uid : "");

	g_object_set_data_full (G_OBJECT (widget), "sao-folder-uri", folder_uri, g_free);

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (mail_identity_combo_box_changed_cb), account_override);

	g_free (account_uid);

	return grid;
}

static GtkWidget *
emfp_get_appearance_item (EConfig *ec,
			  EConfigItem *item,
			  GtkWidget *parent,
			  GtkWidget *old,
			  gint position,
			  gpointer user_data)
{
	AsyncContext *context = user_data;
	GtkBox *vbox;
	EMailFolderTweaks *tweaks;
	gchar *folder_uri;

	if (old)
		return old;

	vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
	gtk_widget_show (GTK_WIDGET (vbox));

	gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (vbox), TRUE, TRUE, 0);

	tweaks = e_mail_folder_tweaks_new ();
	folder_uri = e_mail_folder_uri_from_folder (context->folder);

	emfp_add_tweaks_custom_icon_row (vbox, folder_uri, tweaks);
	emfp_add_tweaks_text_color_row (vbox, folder_uri, tweaks);

	g_clear_object (&tweaks);
	g_free (folder_uri);

	return GTK_WIDGET (vbox);
}

static const gchar *
emfp_autoarchive_config_to_string (EAutoArchiveConfig config)
{
	switch (config) {
		case E_AUTO_ARCHIVE_CONFIG_UNKNOWN:
			break;
		case E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE:
			return "move-to-archive";
		case E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM:
			return "move-to-custom";
		case E_AUTO_ARCHIVE_CONFIG_DELETE:
			return "delete";
	}

	return "unknown";
}

static EAutoArchiveConfig
emfp_autoarchive_config_from_string (const gchar *str)
{
	if (!str)
		return E_AUTO_ARCHIVE_CONFIG_UNKNOWN;
	if (g_ascii_strcasecmp (str, "move-to-archive") == 0)
		return E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE;
	if (g_ascii_strcasecmp (str, "move-to-custom") == 0)
		return E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM;
	if (g_ascii_strcasecmp (str, "delete") == 0)
		return E_AUTO_ARCHIVE_CONFIG_DELETE;

	return E_AUTO_ARCHIVE_CONFIG_UNKNOWN;
}

static const gchar *
emfp_autoarchive_unit_to_string (EAutoArchiveUnit unit)
{
	switch (unit) {
		case E_AUTO_ARCHIVE_UNIT_UNKNOWN:
			break;
		case E_AUTO_ARCHIVE_UNIT_DAYS:
			return "days";
		case E_AUTO_ARCHIVE_UNIT_WEEKS:
			return "weeks";
		case E_AUTO_ARCHIVE_UNIT_MONTHS:
			return "months";
	}

	return "unknown";
}

static EAutoArchiveUnit
emfp_autoarchive_unit_from_string (const gchar *str)
{
	if (!str)
		return E_AUTO_ARCHIVE_UNIT_UNKNOWN;
	if (g_ascii_strcasecmp (str, "days") == 0)
		return E_AUTO_ARCHIVE_UNIT_DAYS;
	if (g_ascii_strcasecmp (str, "weeks") == 0)
		return E_AUTO_ARCHIVE_UNIT_WEEKS;
	if (g_ascii_strcasecmp (str, "months") == 0)
		return E_AUTO_ARCHIVE_UNIT_MONTHS;

	return E_AUTO_ARCHIVE_UNIT_UNKNOWN;
}

#define AUTO_ARCHIVE_KEY_DATA	"auto-archive-key-data"

typedef struct _AutoArchiveData {
	gchar *folder_uri;
	GtkWidget *enabled_check;
	GtkWidget *n_units_spin;
	GtkWidget *unit_combo;
	GtkWidget *move_to_default_radio;
	GtkWidget *move_to_custom_radio;
	GtkWidget *custom_folder_butt;
	GtkWidget *delete_radio;
} AutoArchiveData;

static void
auto_archive_data_free (gpointer ptr)
{
	AutoArchiveData *aad = ptr;

	if (!aad)
		return;

	g_free (aad->folder_uri);
	g_slice_free (AutoArchiveData, aad);
}

static void
emfp_autoarchive_commit_cb (EConfig *ec,
			    AutoArchiveData *aad)
{
	EShell *shell;
	EMailBackend *mail_backend;
	gboolean enabled;
	EAutoArchiveConfig config = E_AUTO_ARCHIVE_CONFIG_UNKNOWN;
	gint n_units;
	EAutoArchiveUnit unit;
	const gchar *custom_target_folder_uri;

	g_return_if_fail (E_IS_CONFIG (ec));
	g_return_if_fail (aad != NULL);
	g_return_if_fail (aad->folder_uri != NULL);

	shell = e_shell_get_default ();
	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_if_fail (mail_backend != NULL);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (aad->move_to_default_radio)))
		config = E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (aad->move_to_custom_radio)))
		config = E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (aad->delete_radio)))
		config = E_AUTO_ARCHIVE_CONFIG_DELETE;
	else
		g_warn_if_reached ();

	enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (aad->enabled_check));
	n_units = gtk_spin_button_get_value (GTK_SPIN_BUTTON (aad->n_units_spin));
	unit = emfp_autoarchive_unit_from_string (gtk_combo_box_get_active_id (GTK_COMBO_BOX (aad->unit_combo)));
	custom_target_folder_uri = em_folder_selection_button_get_folder_uri (EM_FOLDER_SELECTION_BUTTON (aad->custom_folder_butt));
	if (custom_target_folder_uri && !*custom_target_folder_uri)
		custom_target_folder_uri = NULL;

	em_folder_properties_autoarchive_set (mail_backend, aad->folder_uri, enabled, config, n_units, unit, custom_target_folder_uri);
}

static GtkWidget *
emfp_get_autoarchive_item (EConfig *ec,
			   EConfigItem *item,
			   GtkWidget *parent,
			   GtkWidget *old,
			   gint position,
			   gpointer data)
{
	EShell *shell;
	EMailBackend *mail_backend;
	GtkGrid *grid;
	GtkWidget *widget, *label, *check, *radio, *hbox;
	AutoArchiveData *aad;
	AsyncContext *context = data;
	gboolean enabled;
	EAutoArchiveConfig config;
	gint n_units;
	EAutoArchiveUnit unit;
	gchar *custom_target_folder_uri;

	if (old)
		return old;

	shell = e_shell_get_default ();
	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_val_if_fail (mail_backend != NULL, NULL);

	aad = g_slice_new0 (AutoArchiveData);
	g_object_set_data_full (G_OBJECT (ec), AUTO_ARCHIVE_KEY_DATA, aad, auto_archive_data_free);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (grid), TRUE, TRUE, 0);

	label = gtk_label_new (_("Archive this folder using these settings:"));
	gtk_grid_attach (grid, label, 0, 0, 3, 1);
	g_object_set (G_OBJECT (label),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		NULL);

	label = gtk_label_new ("");
	g_object_set (G_OBJECT (label), "margin-start", 12, NULL);
	gtk_grid_attach (grid, label, 0, 1, 1, 3);

	/* Translators: This text is part of "Auto-cleanup messages older than [X] [days/weeks/months]" */
	check = gtk_check_button_new_with_mnemonic (C_("autoarchive", "Auto-_cleanup messages older than"));
	gtk_grid_attach (grid, check, 1, 1, 1, 1);
	aad->enabled_check = check;

	widget = gtk_spin_button_new_with_range (1.0, 999.0, 1.0);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 2, 1, 1, 1);
	aad->n_units_spin = widget;

	e_binding_bind_property (check, "active", widget, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	widget = gtk_combo_box_text_new ();
	/* Translators: This text is part of "Auto-cleanup messages older than [X] [days/weeks/months]" */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_DAYS), C_("autoarchive", "days"));
	/* Translators: This text is part of "Auto-cleanup messages older than [X] [days/weeks/months]" */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_WEEKS), C_("autoarchive", "weeks"));
	/* Translators: This text is part of "Auto-cleanup messages older than [X] [days/weeks/months]" */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_MONTHS), C_("autoarchive", "months"));
	gtk_grid_attach (grid, widget, 3, 1, 1, 1);
	aad->unit_combo = widget;

	e_binding_bind_property (check, "active", widget, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	radio = gtk_radio_button_new_with_mnemonic (NULL, _("Move messages to the default archive _folder"));
	gtk_grid_attach (grid, radio, 1, 2, 2, 1);
	aad->move_to_default_radio = radio;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_grid_attach (grid, hbox, 1, 3, 2, 1);

	widget = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), _("_Move messages to:"));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	aad->move_to_custom_radio = widget;

	widget = em_folder_selection_button_new (e_mail_backend_get_session (mail_backend), _("Archive folder"), _("Select folder to use for Archive"));
	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, FALSE, 0);
	aad->custom_folder_butt = widget;

	widget = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), _("_Delete messages"));
	gtk_grid_attach (grid, widget, 1, 4, 2, 1);
	aad->delete_radio = widget;

	aad->folder_uri = e_mail_folder_uri_build (
		camel_folder_get_parent_store (context->folder),
		camel_folder_get_full_name (context->folder));

	if (em_folder_properties_autoarchive_get (mail_backend, aad->folder_uri,
		&enabled, &config, &n_units, &unit, &custom_target_folder_uri)) {
		switch (config) {
			case E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (aad->move_to_default_radio), TRUE);
				break;
			case E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (aad->move_to_custom_radio), TRUE);
				break;
			case E_AUTO_ARCHIVE_CONFIG_DELETE:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (aad->delete_radio), TRUE);
				break;
			default:
				g_warn_if_reached ();
				break;
		}

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (aad->enabled_check), enabled);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (aad->n_units_spin), n_units);
		g_warn_if_fail (gtk_combo_box_set_active_id (GTK_COMBO_BOX (aad->unit_combo),
			emfp_autoarchive_unit_to_string (unit)));

		if (custom_target_folder_uri && *custom_target_folder_uri)
			em_folder_selection_button_set_folder_uri (EM_FOLDER_SELECTION_BUTTON (aad->custom_folder_butt),
				custom_target_folder_uri);

		g_free (custom_target_folder_uri);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (aad->enabled_check), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (aad->move_to_default_radio), TRUE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (aad->n_units_spin), 12.0);
		g_warn_if_fail (gtk_combo_box_set_active_id (GTK_COMBO_BOX (aad->unit_combo),
			emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_MONTHS)));
	}

	gtk_widget_show_all (GTK_WIDGET (grid));

	g_signal_connect (ec, "commit", G_CALLBACK (emfp_autoarchive_commit_cb), aad);

	return GTK_WIDGET (grid);
}

static gboolean
emfp_labels_check_selection_has_label (GtkTreeSelection *selection,
				       gboolean *anything_selected)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *label = NULL;
	gboolean has_label;

	g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), FALSE);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		if (anything_selected)
			*anything_selected = FALSE;
		return FALSE;
	}

	if (anything_selected)
		*anything_selected = TRUE;

	gtk_tree_model_get (model, &iter, 1, &label, -1);

	has_label = label && *label;

	g_free (label);

	return has_label;
}

static void
emfp_labels_sensitize_when_label_unset_cb (GtkTreeSelection *selection,
					   GtkWidget *widget)
{
	gboolean anything_selected = FALSE;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_widget_set_sensitive (widget, !emfp_labels_check_selection_has_label (selection, &anything_selected) && anything_selected);
}

static void
emfp_labels_sensitize_when_label_set_cb (GtkTreeSelection *selection,
					 GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_widget_set_sensitive (widget, emfp_labels_check_selection_has_label (selection, NULL));
}

static void
emfp_update_label_row (GtkTreeModel *model,
		       GtkTreeIter *iter,
		       const gchar *name,
		       const GdkRGBA *color)
{
	g_return_if_fail (GTK_IS_LIST_STORE (model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (!name || *name);

	gtk_list_store_set (GTK_LIST_STORE (model), iter,
		1, name,
		2, color,
		-1);
}

typedef enum {
	EMFP_ADD_LABEL,
	EMFP_EDIT_LABEL,
	EMFP_REMOVE_LABEL
} EMFPLabelsAction;

static void
emfp_labels_action (GtkWidget *parent,
		    GtkTreeSelection *selection,
		    EMFPLabelsAction action)
{
	EShell *shell;
	EMailBackend *mail_backend;
	EMailLabelListStore *label_store;
	GtkTreeIter my_iter, labels_iter;
	GtkTreeModel *model = NULL;
	gchar *tag = NULL, *label = NULL;
	gboolean tag_exists;

	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	if (!gtk_tree_selection_get_selected (selection, &model, &my_iter))
		return;

	gtk_tree_model_get (model, &my_iter,
		0, &tag,
		1, &label,
		-1);

	if (!tag || !*tag) {
		g_free (tag);
		g_free (label);

		return;
	}

	if (parent && !gtk_widget_is_toplevel (parent))
		parent = NULL;

	shell = e_shell_get_default ();
	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_if_fail (mail_backend != NULL);

	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (e_mail_backend_get_session (mail_backend)));

	tag_exists = e_mail_label_list_store_lookup (label_store, tag, &labels_iter);

	if (action == EMFP_ADD_LABEL) {
		if (!tag_exists) {
			EMailLabelDialog *label_dialog;
			GtkWidget *dialog;
			gboolean repeat = TRUE;

			dialog = e_mail_label_dialog_new (parent ? GTK_WINDOW (parent) : NULL);
			label_dialog = E_MAIL_LABEL_DIALOG (dialog);
			gtk_window_set_title (GTK_WINDOW (dialog), _("Add Label"));

			while (repeat) {
				GdkRGBA label_color;
				const gchar *label_name;

				repeat = FALSE;

				if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
					break;

				label_name = e_mail_label_dialog_get_label_name (label_dialog);
				e_mail_label_dialog_get_label_color (label_dialog, &label_color);

				if (e_mail_label_list_store_lookup_by_name (label_store, label_name, NULL)) {
					repeat = TRUE;
					e_alert_run_dialog_for_args (
						GTK_WINDOW (dialog),
						"mail:error-label-exists", label_name, NULL);
					continue;
				}

				e_mail_label_list_store_set_with_tag (label_store, NULL, tag, label_name, &label_color);

				emfp_update_label_row (model, &my_iter, label_name, &label_color);
			}

			gtk_widget_destroy (dialog);
		}
	} else if (action == EMFP_EDIT_LABEL) {
		if (tag_exists) {
			EMailLabelDialog *label_dialog;
			GtkWidget *dialog;
			const gchar *new_name;
			GdkRGBA label_color;
			gchar *label_name;
			gboolean repeat = TRUE;

			dialog = e_mail_label_dialog_new (parent ? GTK_WINDOW (parent) : NULL);
			gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Label"));

			label_dialog = E_MAIL_LABEL_DIALOG (dialog);

			label_name = e_mail_label_list_store_get_name (label_store, &labels_iter);
			e_mail_label_dialog_set_label_name (label_dialog, label_name);

			if (e_mail_label_list_store_get_color (label_store, &labels_iter, &label_color))
				e_mail_label_dialog_set_label_color (label_dialog, &label_color);

			while (repeat) {
				repeat = FALSE;

				if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
					break;

				new_name = e_mail_label_dialog_get_label_name (label_dialog);
				e_mail_label_dialog_get_label_color (label_dialog, &label_color);

				if (g_strcmp0 (label_name, new_name) != 0 &&
				    e_mail_label_list_store_lookup_by_name (label_store, new_name, NULL)) {
					repeat = TRUE;
					e_alert_run_dialog_for_args (
						GTK_WINDOW (dialog),
						"mail:error-label-exists", new_name, NULL);
					continue;
				}

				e_mail_label_list_store_set (label_store, &labels_iter, new_name, &label_color);

				emfp_update_label_row (model, &my_iter, new_name, &label_color);
			}

			g_free (label_name);
			gtk_widget_destroy (dialog);
		}
	} else if (action == EMFP_REMOVE_LABEL) {
		if (tag_exists) {
			gtk_list_store_remove (GTK_LIST_STORE (label_store), &labels_iter);
			emfp_update_label_row (model, &my_iter, NULL, NULL);
		}
	} else {
		g_warn_if_reached ();
	}

	g_free (tag);
	g_free (label);

	/* To update Add/Edit/Remove buttons */
	gtk_tree_selection_unselect_iter (selection, &my_iter);
	gtk_tree_selection_select_iter (selection, &my_iter);
}

static void
emfp_labels_add_clicked_cb (GtkWidget *button,
			    GtkTreeSelection *selection)
{
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	emfp_labels_action (gtk_widget_get_toplevel (button), selection, EMFP_ADD_LABEL);
}

static void
emfp_labels_edit_clicked_cb (GtkWidget *button,
			     GtkTreeSelection *selection)
{
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	emfp_labels_action (gtk_widget_get_toplevel (button), selection, EMFP_EDIT_LABEL);
}

static void
emfp_labels_remove_clicked_cb (GtkWidget *button,
			       GtkTreeSelection *selection)
{
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	emfp_labels_action (gtk_widget_get_toplevel (button), selection, EMFP_REMOVE_LABEL);
}

static GtkWidget *
emfp_get_labels_item (EConfig *ec,
		      EConfigItem *item,
		      GtkWidget *parent,
		      GtkWidget *old,
		      gint position,
		      gpointer data)
{
	EShell *shell;
	EMailBackend *mail_backend;
	EMailLabelListStore *label_store;
	GtkGrid *grid;
	GtkWidget *widget, *tree_view, *add_btn, *edit_btn, *remove_btn;
	GtkTreeSelection *selection;
	GtkListStore *list_store;
	GtkCellRenderer *renderer;
	GSList *link;
	AsyncContext *context = data;

	if (old)
		return old;

	shell = e_shell_get_default ();
	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_val_if_fail (mail_backend != NULL, NULL);

	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (e_mail_backend_get_session (mail_backend)));

	grid = GTK_GRID (gtk_grid_new ());
	gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (grid), TRUE, TRUE, 0);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	list_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_RGBA);

	for (link = context->available_labels; link; link = g_slist_next (link)) {
		GtkTreeIter iter, label_iter;
		GdkRGBA rgba;
		gboolean has_label_color = FALSE;
		gchar *label_name = NULL;
		const gchar *tag = link->data;

		if (!tag || !*tag)
			continue;

		if (e_mail_label_list_store_lookup (label_store, tag, &label_iter)) {
			label_name = e_mail_label_list_store_get_name (label_store, &label_iter);
			has_label_color = e_mail_label_list_store_get_color (label_store, &label_iter, &rgba);
		}

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
			0, tag,
			1, label_name,
			2, has_label_color ? &rgba : NULL,
			-1);

		g_free (label_name);
	}

	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
	g_clear_object (&list_store);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view), -1,
		_("Server Tag"), renderer, "text", 0, "foreground-rgba", 2, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view), -1,
		_("Label"), renderer, "text", 1, "foreground-rgba", 2, NULL);

	gtk_container_add (GTK_CONTAINER (widget), tree_view);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);

	add_btn = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_container_add (GTK_CONTAINER (widget), add_btn);

	edit_btn = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_container_add (GTK_CONTAINER (widget), edit_btn);

	remove_btn = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	gtk_container_add (GTK_CONTAINER (widget), remove_btn);

	gtk_widget_set_sensitive (add_btn, FALSE);
	gtk_widget_set_sensitive (edit_btn, FALSE);
	gtk_widget_set_sensitive (remove_btn, FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed",
		G_CALLBACK (emfp_labels_sensitize_when_label_unset_cb), add_btn);
	g_signal_connect (selection, "changed",
		G_CALLBACK (emfp_labels_sensitize_when_label_set_cb), edit_btn);
	g_signal_connect (selection, "changed",
		G_CALLBACK (emfp_labels_sensitize_when_label_set_cb), remove_btn);

	g_signal_connect (add_btn, "clicked",
		G_CALLBACK (emfp_labels_add_clicked_cb), selection);
	g_signal_connect (edit_btn, "clicked",
		G_CALLBACK (emfp_labels_edit_clicked_cb), selection);
	g_signal_connect (remove_btn, "clicked",
		G_CALLBACK (emfp_labels_remove_clicked_cb), selection);

	gtk_widget_show_all (GTK_WIDGET (grid));

	return GTK_WIDGET (grid);
}

static EMConfigItem emfp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", NULL },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) N_("General") },
	{ E_CONFIG_SECTION, (gchar *) "00.general/00.folder", NULL /* set by code */ },
	{ E_CONFIG_ITEM, (gchar *) "00.general/00.folder/00.info", NULL, emfp_get_folder_item },
	{ E_CONFIG_PAGE, (gchar *) "10.appearance", (gchar *) N_("Appearance") },
	{ E_CONFIG_SECTION, (gchar *) "10.appearance/00.folder", NULL },
	{ E_CONFIG_ITEM, (gchar *) "10.appearance/00.folder/00.appearance", NULL, emfp_get_appearance_item },
	/* Translators: "Archive" is a noun (This is a tab heading in the mail folder properties) */
	{ E_CONFIG_PAGE, (gchar *) "20.autoarchive", (gchar *) N_("Archive") },
	{ E_CONFIG_SECTION, (gchar *) "20.autoarchive/00.folder", NULL },
	{ E_CONFIG_ITEM, (gchar *) "20.autoarchive/00.folder/00.info", NULL, emfp_get_autoarchive_item },
	{ E_CONFIG_PAGE, (gchar *) "30.labels", (gchar *) N_("Labels") },
	{ E_CONFIG_SECTION, (gchar *) "30.labels/00.folder", NULL },
	{ E_CONFIG_ITEM, (gchar *) "30.labels/00.folder/00.labels", NULL, emfp_get_labels_item }
};

static void
emfp_dialog_run (AsyncContext *context)
{
	GtkWidget *dialog, *w;
	GtkWidget *content_area;
	GSList *l;
	gint32 i,deleted;
	EMConfig *ec;
	EMConfigTargetFolder *target;
	CamelStore *parent_store;
	CamelFolderSummary *summary;
	gboolean store_is_local;
	gboolean hide_deleted;
	GSettings *settings;
	const gchar *folder_label;
	const gchar *name;
	const gchar *uid;

	parent_store = camel_folder_get_parent_store (context->folder);

	/* Get number of VISIBLE and DELETED messages, instead of TOTAL
	 * messages.  VISIBLE+DELETED gives the correct count that matches
	 * the label below the Send & Receive button. */
	summary = camel_folder_get_folder_summary (context->folder);
	context->total = camel_folder_summary_get_visible_count (summary);
	context->unread = camel_folder_summary_get_unread_count (summary);
	deleted = camel_folder_summary_get_deleted_count (summary);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	hide_deleted = !g_settings_get_boolean (settings, "show-deleted");
	g_object_unref (settings);

	/*
	 * Do the calculation only for those accounts that support VTRASHes
	 */
	if (camel_store_get_flags (parent_store) & CAMEL_STORE_VTRASH) {
		if (CAMEL_IS_VTRASH_FOLDER (context->folder))
			context->total += deleted;
		else if (!hide_deleted && deleted > 0)
			context->total += deleted;
	}

	/*
	 * If the folder is junk folder, get total number of mails.
	 */
	if (camel_store_get_flags (parent_store) & CAMEL_STORE_VJUNK)
		context->total = camel_folder_summary_count (
			camel_folder_get_folder_summary (context->folder));

	name = camel_folder_get_display_name (context->folder);

	uid = camel_service_get_uid (CAMEL_SERVICE (parent_store));
	store_is_local = (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0);

	if (store_is_local
	    && (!strcmp (name, "Drafts")
		|| !strcmp (name, "Templates")
		|| !strcmp (name, "Inbox")
		|| !strcmp (name, "Outbox")
		|| !strcmp (name, "Sent"))) {
		folder_label = _(name);
	} else if (!strcmp (name, "INBOX"))
		folder_label = _("Inbox");
	else
		folder_label = name;

	for (i = 0; i < G_N_ELEMENTS (emfp_items); i++) {
		if (emfp_items[i].type == E_CONFIG_SECTION &&
		    g_str_has_suffix (emfp_items[i].path, "/00.folder"))
			emfp_items[i].label = (gchar *) folder_label;
	}

	dialog = gtk_dialog_new_with_buttons (
		_("Folder Properties"),
		context->parent_window,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_OK, NULL);
	gtk_window_set_default_size ((GtkWindow *) dialog, 192, 160);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);

	/** @HookPoint-EMConfig: Folder Properties Window
	 * @Id: org.gnome.evolution.mail.folderConfig
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetFolder
	 *
	 * The folder properties window.
	 */
	ec = em_config_new ("org.gnome.evolution.mail.folderConfig");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (emfp_items); i++)
		l = g_slist_prepend (l, &emfp_items[i]);
	e_config_add_items ((EConfig *) ec, l, emfp_free, context);

	target = em_config_target_new_folder (ec, context->folder);

	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
	w = e_config_create_widget ((EConfig *) ec);

	gtk_box_pack_start (GTK_BOX (content_area), w, TRUE, TRUE, 0);

	/* We do 'apply on ok', since instant apply may start some
	 * very long running tasks. */

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		e_config_commit ((EConfig *) ec);
		camel_folder_save_state (context->folder);
	} else
		e_config_abort ((EConfig *) ec);

	gtk_widget_destroy (dialog);
}

static gboolean
emfp_gather_unique_labels_cb (gpointer user_data,
			      gint ncol,
			      gchar **colvalues,
			      gchar **colnames)
{
	GHashTable *hash = user_data;

	g_return_val_if_fail (hash != NULL, FALSE);

	if (ncol == 1 && colvalues[0] && *colvalues[0]) {
		gchar **strv;

		strv = g_strsplit (colvalues[0], " ", -1);
		if (strv) {
			gint ii;

			for (ii = 0; strv[ii]; ii++) {
				gchar *copy = g_strdup (g_strstrip (strv[ii]));

				if (copy && *copy)
					g_hash_table_insert (hash, copy, NULL);
				else
					g_free (copy);
			}
		}

		g_strfreev (strv);
	}

	return TRUE;
}

/* Use g_slist_free_full (labels, g_free); to free the returned pointer */
static GSList *
emfp_gather_folder_available_labels_sync (CamelFolder *folder)
{
	GSList *labels = NULL;
	GHashTable *hash;
	GHashTableIter iter;
	CamelStore *store;
	CamelStoreDB *sdb;
	gchar *query;
	guint32 folder_id;
	gint ii;
	gpointer key;
	GError *local_error = NULL;
	/* A list of known tags, which are used internally by the evolution and thus not a real labels. */
	const gchar *skip_labels[] = {
		E_MAIL_NOTES_USER_FLAG,
		"$has_cal",
		"receipt-handled",
		NULL
	};

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	store = camel_folder_get_parent_store (folder);
	if (!store)
		return NULL;

	sdb = camel_store_get_db (store);
	if (!sdb)
		return NULL;

	folder_id = camel_store_db_get_folder_id (sdb, camel_folder_get_full_name (folder));
	hash = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

	query = g_strdup_printf ("SELECT DISTINCT labels FROM messages WHERE folder_id=%u AND labels NOT LIKE ''", folder_id);
	camel_db_exec_select (CAMEL_DB (sdb), query, emfp_gather_unique_labels_cb, hash, &local_error);

	if (local_error) {
		g_debug ("%s: Failed to execute '%s': %s\n", G_STRFUNC, query, local_error->message);
		g_clear_error (&local_error);
	}

	g_free (query);

	for (ii = 0; skip_labels[ii]; ii++) {
		g_hash_table_remove (hash, skip_labels[ii]);
	}

	g_hash_table_iter_init (&iter, hash);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		labels = g_slist_prepend (labels, g_strdup (key));
	}

	g_hash_table_destroy (hash);

	return g_slist_sort (labels, e_collate_compare);
}

static void
emfp_prepare_dialog_data_done (gpointer ptr)
{
	AsyncContext *context = ptr;

	g_return_if_fail (context != NULL);

	/* Destroy the activity before the dialog is shown, otherwise
	   it's on the status bar until the dialog is closed. */
	g_clear_object (&context->activity);

	if (context->folder && !context->cancelled)
		emfp_dialog_run (context);

	async_context_free (context);
}

static void
emfp_prepare_dialog_data_thread (EAlertSinkThreadJobData *job_data,
				 gpointer user_data,
				 GCancellable *cancellable,
				 GError **error)
{
	AsyncContext *context = user_data;
	GError *local_error = NULL;

	g_return_if_fail (context != NULL);

	e_flag_wait (context->flag);

	context->folder = camel_store_get_folder_sync (context->store, context->folder_name, 0, cancellable, error);
	if (!context->folder)
		return;

	context->quota_info = camel_folder_get_quota_info_sync (context->folder, cancellable, &local_error);

	/* If the folder does not implement quota info, just continue. */
	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)) {
		g_warn_if_fail (context->quota_info == NULL);
		g_clear_error (&local_error);
	} else if (local_error) {
		g_debug ("%s: Failed to get quota information: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
	}

	context->available_labels = emfp_gather_folder_available_labels_sync (context->folder);

	context->cancelled = g_cancellable_is_cancelled (cancellable);
}

/**
 * em_folder_properties_show:
 * @store: a #CamelStore
 * @folder_name: a folder name
 * @alert_sink: an #EAlertSink
 * @parent_window: a parent #GtkWindow
 *
 * Show folder properties for @folder_name.
 **/
void
em_folder_properties_show (CamelStore *store,
                           const gchar *folder_name,
                           EAlertSink *alert_sink,
                           GtkWindow *parent_window)
{
	CamelService *service;
	CamelSession *session;
	AsyncContext *context;
	const gchar *uid;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);
	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));
	g_return_if_fail (GTK_IS_WINDOW (parent_window));

	service = CAMEL_SERVICE (store);
	uid = camel_service_get_uid (service);
	session = camel_service_ref_session (service);

	/* Show the Edit Rule dialog for Search Folders. */
	if (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0) {
		gchar *folder_uri;

		folder_uri = e_mail_folder_uri_build (
			store, folder_name);
		vfolder_edit_rule (
			E_MAIL_SESSION (session),
			folder_uri, alert_sink);
		g_free (folder_uri);

		goto exit;
	}

	/* Open the folder asynchronously. */

	context = g_slice_new0 (AsyncContext);
	context->flag = e_flag_new ();
	context->parent_window = g_object_ref (parent_window);
	context->store = g_object_ref (store);
	context->folder_name = g_strdup (folder_name);

	context->activity = e_alert_sink_submit_thread_job (alert_sink,
		_("Gathering folder properties"), "mail:folder-open", NULL,
		emfp_prepare_dialog_data_thread, context, emfp_prepare_dialog_data_done);

	e_mail_ui_session_add_activity (E_MAIL_UI_SESSION (session), context->activity);

	e_flag_set (context->flag);

 exit:
	g_object_unref (session);
}

/*
  out-parameters are valid only if TRUE is returned;
  returned custom_target_folder_uri, if not NULL, should be freed with g_free()
 */
gboolean
em_folder_properties_autoarchive_get (EMailBackend *mail_backend,
				      const gchar *folder_uri,
				      gboolean *enabled,
				      EAutoArchiveConfig *config,
				      gint *n_units,
				      EAutoArchiveUnit *unit,
				      gchar **custom_target_folder_uri)
{
	EMailProperties *properties;
	ENamedParameters *parameters;
	gchar *stored;
	const gchar *value;
	gboolean success;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (mail_backend), FALSE);
	g_return_val_if_fail (folder_uri != NULL, FALSE);
	g_return_val_if_fail (enabled != NULL, FALSE);
	g_return_val_if_fail (config != NULL, FALSE);
	g_return_val_if_fail (n_units != NULL, FALSE);
	g_return_val_if_fail (unit != NULL, FALSE);
	g_return_val_if_fail (custom_target_folder_uri != NULL, FALSE);

	properties = e_mail_backend_get_mail_properties (mail_backend);
	g_return_val_if_fail (properties != NULL, FALSE);

	stored = e_mail_properties_get_for_folder_uri (properties, folder_uri, "autoarchive");
	if (!stored)
		return FALSE;

	parameters = e_named_parameters_new_string (stored);

	g_free (stored);

	if (!parameters)
		return FALSE;

	*enabled = g_strcmp0 (e_named_parameters_get (parameters, "enabled"), "1") == 0;
	*config = emfp_autoarchive_config_from_string (e_named_parameters_get (parameters, "config"));
	*unit = emfp_autoarchive_unit_from_string (e_named_parameters_get (parameters, "unit"));

	value = e_named_parameters_get (parameters, "n-units");
	if (value && *value)
		*n_units = (gint) g_ascii_strtoll (value, NULL, 10);
	else
		*n_units = -1;

	success = *config != E_AUTO_ARCHIVE_CONFIG_UNKNOWN &&
		  *unit != E_AUTO_ARCHIVE_UNIT_UNKNOWN &&
		  *n_units > 0;

	if (success)
		*custom_target_folder_uri = g_strdup (e_named_parameters_get (parameters, "custom-target"));

	e_named_parameters_free (parameters);

	return success;
}

void
em_folder_properties_autoarchive_set (EMailBackend *mail_backend,
				      const gchar *folder_uri,
				      gboolean enabled,
				      EAutoArchiveConfig config,
				      gint n_units,
				      EAutoArchiveUnit unit,
				      const gchar *custom_target_folder_uri)
{
	EMailProperties *properties;
	ENamedParameters *parameters;
	gchar *value, *stored_value;

	g_return_if_fail (E_IS_MAIL_BACKEND (mail_backend));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (config != E_AUTO_ARCHIVE_CONFIG_UNKNOWN);
	g_return_if_fail (n_units > 0);
	g_return_if_fail (unit != E_AUTO_ARCHIVE_UNIT_UNKNOWN);

	properties = e_mail_backend_get_mail_properties (mail_backend);
	g_return_if_fail (properties != NULL);

	parameters = e_named_parameters_new ();

	e_named_parameters_set (parameters, "enabled", enabled ? "1" : "0");
	e_named_parameters_set (parameters, "config", emfp_autoarchive_config_to_string (config));
	e_named_parameters_set (parameters, "unit", emfp_autoarchive_unit_to_string (unit));

	value = g_strdup_printf ("%d", n_units);
	e_named_parameters_set (parameters, "n-units", value);
	g_free (value);

	if (custom_target_folder_uri && *custom_target_folder_uri)
		e_named_parameters_set (parameters, "custom-target", custom_target_folder_uri);

	value = e_named_parameters_to_string (parameters);
	stored_value = e_mail_properties_get_for_folder_uri (properties, folder_uri, "autoarchive");
	if (!stored_value) {
		/* If nothing is stored, then use the defaults */
		e_named_parameters_set (parameters, "enabled", "0");
		e_named_parameters_set (parameters, "config", emfp_autoarchive_config_to_string (E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE));
		e_named_parameters_set (parameters, "unit", emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_MONTHS));
		e_named_parameters_set (parameters, "n-units", "12");
		e_named_parameters_set (parameters, "custom-target", NULL);

		stored_value = e_named_parameters_to_string (parameters);
	}

	/* To avoid overwriting unchanged values or adding default values. */
	if (g_strcmp0 (stored_value, value) != 0)
		e_mail_properties_set_for_folder_uri (properties, folder_uri, "autoarchive", value);

	e_named_parameters_free (parameters);
	g_free (stored_value);
	g_free (value);
}
