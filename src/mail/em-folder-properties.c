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
#include "e-mail-ui-session.h"
#include "em-config.h"
#include "em-folder-selection-button.h"
#include "mail-vfolder-ui.h"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	CamelFolder *folder;
	GtkWindow *parent_window;
	CamelFolderQuotaInfo *quota_info;
	gint total;
	gint unread;
};

static void
async_context_free (AsyncContext *context)
{
	if (context->activity != NULL)
		g_object_unref (context->activity);

	if (context->folder != NULL)
		g_object_unref (context->folder);

	if (context->parent_window != NULL)
		g_object_unref (context->parent_window);

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

static void
mail_identity_combo_box_changed_cb (GtkComboBox *combo_box,
                                    EMailSendAccountOverride *account_override)
{
	const gchar *active_id, *folder_uri;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));

	folder_uri = g_object_get_data (G_OBJECT (combo_box), "sao-folder-uri");
	g_return_if_fail (folder_uri != NULL);

	active_id = gtk_combo_box_get_active_id (combo_box);
	if (!active_id || !*active_id)
		e_mail_send_account_override_remove_for_folder (account_override, folder_uri);
	else
		e_mail_send_account_override_set_for_folder (account_override, folder_uri, active_id);
}

static gint
add_numbered_row (GtkTable *table,
                  gint row,
                  const gchar *description,
                  const gchar *format,
                  gint num)
{
	gchar *str;
	GtkWidget *label;

	g_return_val_if_fail (table != NULL, row);
	g_return_val_if_fail (description != NULL, row);
	g_return_val_if_fail (format != NULL, row);

	label = gtk_label_new (description);
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (
		table, label, 0, 1, row, row + 1,
		GTK_FILL, 0, 0, 0);

	str = g_strdup_printf (format, num);

	label = gtk_label_new (str);
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
	gtk_table_attach (
		table, label, 1, 2, row, row + 1,
		GTK_FILL | GTK_EXPAND, 0, 0, 0);

	g_free (str);

	return row + 1;
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
		g_free (tsd);
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
	GtkWidget *widget, *table;
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
	gchar *folder_uri, *account_uid;
	GtkWidget *label;

	if (old)
		return old;

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings ((GtkTable *) table, 6);
	gtk_table_set_col_spacings ((GtkTable *) table, 12);
	gtk_widget_show (table);
	gtk_box_pack_start ((GtkBox *) parent, table, TRUE, TRUE, 0);

	/* To be on the safe side, ngettext is used here,
	 * see e.g. comment #3 at bug 272567 */
	row = add_numbered_row (
		GTK_TABLE (table), row,
		ngettext (
			"Unread messages:",
			"Unread messages:",
			context->unread),
		"%d", context->unread);

	/* TODO: can this be done in a loop? */
	/* To be on the safe side, ngettext is used here,
	 * see e.g. comment #3 at bug 272567 */
	row = add_numbered_row (
		GTK_TABLE (table), row,
		ngettext (
			"Total messages:",
			"Total messages:",
			context->total),
		"%d", context->total);

	if (context->quota_info) {
		CamelFolderQuotaInfo *info;
		CamelFolderQuotaInfo *quota = context->quota_info;

		for (info = quota; info; info = info->next) {
			gchar *descr;
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

			row = add_numbered_row (
				GTK_TABLE (table), row,
				descr, "%d%%", procs);

			g_free (descr);
		}
	}

	store = camel_folder_get_parent_store (context->folder);
	folder_name = camel_folder_get_full_name (context->folder);

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

		if ((properties[ii]->flags & CAMEL_PARAM_PERSISTENT) == 0)
			continue;

		if (!can_apply_filters &&
		    g_strcmp0 (properties[ii]->name, "apply-filters") == 0)
			continue;

		blurb = g_param_spec_get_blurb (properties[ii]);

		switch (properties[ii]->value_type) {
			case G_TYPE_BOOLEAN:
				widget = gtk_check_button_new_with_mnemonic (blurb);
				e_binding_bind_property (
					context->folder,
					properties[ii]->name,
					widget, "active",
					G_BINDING_BIDIRECTIONAL |
					G_BINDING_SYNC_CREATE);
				gtk_widget_show (widget);
				gtk_table_attach (
					GTK_TABLE (table), widget,
					0, 2, row, row + 1,
					GTK_FILL | GTK_EXPAND, 0, 0, 0);
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

					g_object_set (G_OBJECT (widget),
						"inconsistent", set_inconsistent,
						"active", set_active,
						NULL);

					tsd = g_new0 (ThreeStateData, 1);
					tsd->folder = g_object_ref (context->folder);
					tsd->property_name = g_strdup (properties[ii]->name);
					tsd->handler_id = g_signal_connect_data (widget, "toggled",
						G_CALLBACK (emfp_three_state_toggled_cb),
						tsd, three_state_data_free, 0);

					gtk_widget_show (widget);
					gtk_table_attach (
						GTK_TABLE (table), widget,
						0, 2, row, row + 1,
						GTK_FILL | GTK_EXPAND, 0, 0, 0);
					row++;
				} else {
					g_warn_if_reached ();
				}
				break;
		}
	}

	g_free (properties);

	/* add send-account-override setting widgets */
	registry = e_shell_get_registry (e_shell_get_default ());

	/* Translators: Label of a combo with a list of configured accounts where a user can
	   choose which account to use as the sender when composing a message in this folder */
	label = gtk_label_new_with_mnemonic (_("_Send Account Override:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_show (label);
	gtk_table_attach (
		GTK_TABLE (table), label,
		0, 2, row, row + 1,
		GTK_FILL, 0, 0, 0);
	row++;

	widget = g_object_new (
		E_TYPE_MAIL_IDENTITY_COMBO_BOX,
		"registry", registry,
		"allow-none", TRUE,
		NULL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_widget_set_margin_left (widget, 12);
	gtk_widget_show (widget);
	gtk_table_attach (
		GTK_TABLE (table), widget,
		0, 2, row, row + 1,
		GTK_FILL | GTK_EXPAND, 0, 0, 0);
	row++;

	shell = e_shell_get_default ();
	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_val_if_fail (mail_backend != NULL, table);

	account_override = e_mail_backend_get_send_account_override (mail_backend);
	folder_uri = e_mail_folder_uri_from_folder (context->folder);
	account_uid = e_mail_send_account_override_get_for_folder (account_override, folder_uri);

	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), account_uid ? account_uid : "");
	g_object_set_data_full (G_OBJECT (widget), "sao-folder-uri", folder_uri, g_free);

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (mail_identity_combo_box_changed_cb), account_override);

	g_free (account_uid);

	return table;
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
	g_free (aad);
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

	aad = g_new0 (AutoArchiveData, 1);
	g_object_set_data_full (G_OBJECT (ec), AUTO_ARCHIVE_KEY_DATA, aad, auto_archive_data_free);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (grid), TRUE, TRUE, 0);

	check = gtk_check_button_new_with_mnemonic (_("_Archive this folder using these settings:"));
	gtk_grid_attach (grid, check, 0, 0, 3, 1);
	aad->enabled_check = check;

	label = gtk_label_new ("");
	g_object_set (G_OBJECT (label), "margin-left", 12, NULL);
	gtk_grid_attach (grid, label, 0, 1, 1, 3);

	/* Translators: This text is part of "Cleanup messages older than [X] [days/weeks/months]" */
	label = gtk_label_new_with_mnemonic (C_("autoarchive", "_Cleanup messages older than"));
	gtk_grid_attach (grid, label, 1, 1, 1, 1);

	e_binding_bind_property (check, "active", label, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	widget = gtk_spin_button_new_with_range (1.0, 999.0, 1.0);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 2, 1, 1, 1);
	aad->n_units_spin = widget;

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	e_binding_bind_property (check, "active", widget, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	widget = gtk_combo_box_text_new ();
	/* Translators: This text is part of "Cleanup messages older than [X] [days/weeks/months]" */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_DAYS), C_("autoarchive", "days"));
	/* Translators: This text is part of "Cleanup messages older than [X] [days/weeks/months]" */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_WEEKS), C_("autoarchive", "weeks"));
	/* Translators: This text is part of "Cleanup messages older than [X] [days/weeks/months]" */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), emfp_autoarchive_unit_to_string (E_AUTO_ARCHIVE_UNIT_MONTHS), C_("autoarchive", "months"));
	gtk_grid_attach (grid, widget, 3, 1, 1, 1);
	aad->unit_combo = widget;

	e_binding_bind_property (check, "active", widget, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	radio = gtk_radio_button_new_with_mnemonic (NULL, _("Move old messages to the default archive _folder"));
	gtk_grid_attach (grid, radio, 1, 2, 2, 1);
	aad->move_to_default_radio = radio;

	e_binding_bind_property (check, "active", radio, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_grid_attach (grid, hbox, 1, 3, 2, 1);

	e_binding_bind_property (check, "active", hbox, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	widget = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), _("_Move old messages to:"));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	aad->move_to_custom_radio = widget;

	widget = em_folder_selection_button_new (e_mail_backend_get_session (mail_backend), _("AutoArchive folder"), _("Select folder to use for AutoArchive"));
	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, FALSE, 0);
	aad->custom_folder_butt = widget;

	e_binding_bind_property (aad->move_to_custom_radio, "active", widget, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	widget = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (radio), _("_Delete old messages"));
	gtk_grid_attach (grid, widget, 1, 4, 2, 1);
	aad->delete_radio = widget;

	e_binding_bind_property (check, "active", widget, "sensitive", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

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

#define EMFP_FOLDER_SECTION (2)

static EMConfigItem emfp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", NULL },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) N_("General") },
	{ E_CONFIG_SECTION, (gchar *) "00.general/00.folder", NULL /* set by code */ },
	{ E_CONFIG_ITEM, (gchar *) "00.general/00.folder/00.info", NULL, emfp_get_folder_item },
	{ E_CONFIG_PAGE, (gchar *) "10.autoarchive", (gchar *) N_("AutoArchive") },
	{ E_CONFIG_SECTION, (gchar *) "10.autoarchive/00.folder", NULL },
	{ E_CONFIG_ITEM, (gchar *) "10.autoarchive/00.folder/00.info", NULL, emfp_get_autoarchive_item }
};
static gboolean emfp_items_translated = FALSE;

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
	const gchar *name;
	const gchar *uid;

	parent_store = camel_folder_get_parent_store (context->folder);

	/* Get number of VISIBLE and DELETED messages, instead of TOTAL
	 * messages.  VISIBLE+DELETED gives the correct count that matches
	 * the label below the Send & Receive button. */
	summary = context->folder->summary;
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
			context->folder->summary);

	name = camel_folder_get_display_name (context->folder);

	uid = camel_service_get_uid (CAMEL_SERVICE (parent_store));
	store_is_local = (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0);

	if (store_is_local
	    && (!strcmp (name, "Drafts")
		|| !strcmp (name, "Templates")
		|| !strcmp (name, "Inbox")
		|| !strcmp (name, "Outbox")
		|| !strcmp (name, "Sent"))) {
		emfp_items[EMFP_FOLDER_SECTION].label = gettext (name);
		if (!emfp_items_translated) {
			for (i = 0; i < G_N_ELEMENTS (emfp_items); i++) {
				if (emfp_items[i].label)
					emfp_items[i].label = _(emfp_items[i].label);
			}
			emfp_items_translated = TRUE;
		}
	} else if (!strcmp (name, "INBOX"))
		emfp_items[EMFP_FOLDER_SECTION].label = _("Inbox");
	else
		emfp_items[EMFP_FOLDER_SECTION].label = (gchar *) name;

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
		camel_object_state_write (CAMEL_OBJECT (context->folder));
	} else
		e_config_abort ((EConfig *) ec);

	gtk_widget_destroy (dialog);
}

static void
emfp_dialog_got_quota_info (CamelFolder *folder,
                            GAsyncResult *result,
                            AsyncContext *context)
{
	EAlertSink *alert_sink;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (context->activity);

	context->quota_info =
		camel_folder_get_quota_info_finish (folder, result, &error);

	/* If the folder does not implement quota info, just continue. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)) {
		g_warn_if_fail (context->quota_info == NULL);
		g_error_free (error);

	} else if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (context->quota_info == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL && context->folder != NULL) {
		g_debug ("%s: Failed to get quota information: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else if (error != NULL) {
		g_warn_if_fail (context->folder == NULL);
		e_alert_submit (
			alert_sink, "mail:folder-open",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	/* Quota info may still be NULL here if not supported. */

	/* Finalize the activity here so we don't leave a message
	 * in the task bar while the properties window is shown. */
	e_activity_set_state (context->activity, E_ACTIVITY_COMPLETED);
	g_object_unref (context->activity);
	context->activity = NULL;

	emfp_dialog_run (context);

	async_context_free (context);
}

static void
emfp_dialog_got_folder (CamelStore *store,
                        GAsyncResult *result,
                        AsyncContext *context)
{
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (context->activity);
	cancellable = e_activity_get_cancellable (context->activity);

	context->folder = camel_store_get_folder_finish (
		store, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (context->folder == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (context->folder == NULL);
		e_alert_submit (
			alert_sink, "mail:folder-open",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_FOLDER (context->folder));

	camel_folder_get_quota_info (
		context->folder, G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) emfp_dialog_got_quota_info, context);
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
	GCancellable *cancellable;
	AsyncContext *context;
	const gchar *uid;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);
	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));
	g_return_if_fail (GTK_IS_WINDOW (parent_window));

	service = CAMEL_SERVICE (store);
	uid = camel_service_get_uid (service);
	session = camel_service_ref_session (service);

	/* Show the Edit Rule dialog for Search Folders, but not "Unmatched".
	 * "Unmatched" is a special Search Folder which can't be modified. */
	if (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0) {
		if (g_strcmp0 (folder_name, CAMEL_UNMATCHED_NAME) != 0) {
			gchar *folder_uri;

			folder_uri = e_mail_folder_uri_build (
				store, folder_name);
			vfolder_edit_rule (
				E_MAIL_SESSION (session),
				folder_uri, alert_sink);
			g_free (folder_uri);

			goto exit;
		}
	}

	/* Open the folder asynchronously. */

	context = g_slice_new0 (AsyncContext);
	context->activity = e_activity_new ();
	context->parent_window = g_object_ref (parent_window);

	e_activity_set_alert_sink (context->activity, alert_sink);

	cancellable = camel_operation_new ();
	e_activity_set_cancellable (context->activity, cancellable);

	e_mail_ui_session_add_activity (
		E_MAIL_UI_SESSION (session), context->activity);

	camel_store_get_folder (
		store, folder_name, 0, G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) emfp_dialog_got_folder, context);

	g_object_unref (cancellable);

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
