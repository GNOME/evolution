/*
 * e-mail-config-security-page.c
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

#include <string.h>

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>
#include <libebackend/libebackend.h>

#if defined (ENABLE_SMIME)
#include <smime/gui/e-cert-selector.h>
#endif /* ENABLE_SMIME */

#include "e-mail-config-security-page.h"

struct _EMailConfigSecurityPagePrivate {
	ESource *identity_source;
};

enum {
	PROP_0,
	PROP_IDENTITY_SOURCE
};

/* Forward Declarations */
static void	e_mail_config_security_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailConfigSecurityPage, e_mail_config_security_page, GTK_TYPE_SCROLLED_WINDOW,
	G_ADD_PRIVATE (EMailConfigSecurityPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_security_page_interface_init))

#ifdef ENABLE_SMIME
static void
mail_config_security_page_cert_selected (ECertSelector *selector,
                                         const gchar *key,
                                         GtkEntry *entry)
{
	if (key != NULL)
		gtk_entry_set_text (entry, key);

	gtk_widget_destroy (GTK_WIDGET (selector));
}

static void
mail_config_security_page_select_encrypt_cert (GtkButton *button,
                                               GtkEntry *entry)
{
	GtkWidget *selector;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = GTK_IS_WIDGET (parent) ? parent : NULL;

	selector = e_cert_selector_new (
		E_CERT_SELECTOR_RECIPIENT,
		gtk_entry_get_text (entry));
	gtk_window_set_transient_for (
		GTK_WINDOW (selector), parent);
	gtk_widget_show (selector);

	g_signal_connect (
		selector, "selected",
		G_CALLBACK (mail_config_security_page_cert_selected),
		entry);
}

static void
mail_config_security_page_select_sign_cert (GtkButton *button,
                                            GtkEntry *entry)
{
	GtkWidget *selector;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = GTK_IS_WIDGET (parent) ? parent : NULL;

	selector = e_cert_selector_new (
		E_CERT_SELECTOR_SIGNER,
		gtk_entry_get_text (entry));
	gtk_window_set_transient_for (
		GTK_WINDOW (selector), parent);
	gtk_widget_show (selector);

	g_signal_connect (
		selector, "selected",
		G_CALLBACK (mail_config_security_page_cert_selected),
		entry);
}

static void
mail_config_security_page_clear_cert (GtkButton *button,
                                      GtkEntry *entry)
{
	gtk_entry_set_text (entry, "");
}
#endif /* ENABLE_SMIME */

static void
mail_config_security_page_set_identity_source (EMailConfigSecurityPage *page,
                                               ESource *identity_source)
{
	g_return_if_fail (E_IS_SOURCE (identity_source));
	g_return_if_fail (page->priv->identity_source == NULL);

	page->priv->identity_source = g_object_ref (identity_source);
}

static void
mail_config_security_page_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			mail_config_security_page_set_identity_source (
				E_MAIL_CONFIG_SECURITY_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_security_page_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_security_page_get_identity_source (
				E_MAIL_CONFIG_SECURITY_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_security_page_dispose (GObject *object)
{
	EMailConfigSecurityPage *self = E_MAIL_CONFIG_SECURITY_PAGE (object);

	g_clear_object (&self->priv->identity_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_security_page_parent_class)->dispose (object);
}

static GHashTable * /* gchar *keyid ~> gchar *display_name */
mail_security_page_list_seahorse_keys (void)
{
	enum {
		KEY_FLAG_IS_VALID =    0x00000001,
		KEY_FLAG_CAN_ENCRYPT = 0x00000002,
		KEY_FLAG_CAN_SIGN =    0x00000004,
		KEY_FLAG_EXPIRED =     0x00000100,
		KEY_FLAG_REVOKED =     0x00000200,
		KEY_FLAG_DISABLED =    0x00000400,
		KEY_FLAG_TRUSTED =     0x00001000,
		KEY_FLAG_EXPORTABLE =  0x00100000
	};
	GDBusProxy *proxy;
	GError *error = NULL;
	GHashTable *keys = NULL;
	GVariant *keysres;

	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
		G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL,
		"org.gnome.seahorse",
		"/org/gnome/seahorse/keys/openpgp",
		"org.gnome.seahorse.Keys",
		NULL,
		&error);

	if (!proxy) {
		g_debug ("%s: Failed to create proxy: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
		return NULL;
	}

	keysres = g_dbus_proxy_call_sync (proxy, "ListKeys", NULL, G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &error);
	if (keysres) {
		gchar **strv = NULL;

		g_variant_get (keysres, "(^as)", &strv);
		if (strv) {
			const gchar *fields[] = { "key-id", "display-name", "flags", NULL };
			gint ii;

			for (ii = 0; strv[ii]; ii++) {
				const gchar *keyid = strv[ii];

				/* Expected result is "openpgp:key-id:subkey-index", but
				   care only of those without subkey index */
				if (*keyid && strchr (keyid, ':') == strrchr (keyid, ':')) {
					GVariant *keyinfo;

					keyinfo = g_dbus_proxy_call_sync (proxy, "GetKeyFields",
						g_variant_new ("(s^as)", keyid, fields),
						G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &error);

					if (keyinfo) {
						GVariantDict *dict;
						GVariant *val = NULL;

						g_variant_get (keyinfo, "(@a{sv})", &val);
						if (!val) {
							g_variant_unref (keyinfo);
							g_debug ("%s: Cannot get keyinfo value", G_STRFUNC);
							continue;
						}

						dict = g_variant_dict_new (val);
						g_variant_unref (val);

						if (!dict) {
							g_variant_unref (keyinfo);
							g_debug ("%s: Cannot create dictionary from keyinfo value", G_STRFUNC);
							continue;
						}

						val = g_variant_dict_lookup_value (dict, "flags", G_VARIANT_TYPE_UINT32);
						if (val) {
							guint32 flags = g_variant_get_uint32 (val);

							g_variant_unref (val);

							if ((flags & KEY_FLAG_CAN_SIGN) != 0 &&
							    (flags & KEY_FLAG_IS_VALID) != 0 &&
							    (flags & (KEY_FLAG_EXPIRED | KEY_FLAG_REVOKED | KEY_FLAG_DISABLED)) == 0) {
								gchar *val_keyid = NULL, *display_name = NULL;

								val = g_variant_dict_lookup_value (dict, "key-id", G_VARIANT_TYPE_STRING);
								if (val) {
									val_keyid = g_variant_dup_string (val, NULL);
									g_variant_unref (val);
								}

								val = g_variant_dict_lookup_value (dict, "display-name", G_VARIANT_TYPE_STRING);
								if (val) {
									display_name = g_variant_dup_string (val, NULL);
									g_variant_unref (val);
								}

								if (val_keyid && *val_keyid && display_name && *display_name) {
									if (!keys)
										keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

									g_hash_table_insert (keys, val_keyid, display_name);
								} else {
									g_free (val_keyid);
									g_free (display_name);
								}
							}
						}

						g_variant_dict_unref (dict);
						g_variant_unref (keyinfo);
					} else {
						g_debug ("%s: Failed to get key fields for '%s': %s", G_STRFUNC, keyid, error ? error->message : "Unknown error");
						g_clear_error (&error);
					}
				}
			}

			g_strfreev (strv);
		}

		g_variant_unref (keysres);
	} else {
		g_debug ("%s: Failed to call ListKeys: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	g_clear_object (&proxy);

	return keys;
}

static gint
compare_by_display_name (gconstpointer v1,
			 gconstpointer v2,
			 gpointer user_data)
{
	const gchar *dn1, *dn2;

	if (!v1 || !v2) {
		if (v1 == v2)
			return 0;

		return v1 ? 1 : -1;
	}

	dn1 = g_hash_table_lookup (user_data, v1);
	dn2 = g_hash_table_lookup (user_data, v2);

	if (!dn1 || !dn2) {
		if (dn1 == dn2)
			return 0;

		return dn1 ? 1 : -1;
	}

	return g_utf8_collate (dn1, dn2);
}

static GtkWidget *
mail_security_page_get_openpgpg_combo (void)
{
	GtkWidget *widget, *child;
	GtkListStore *store;
	GtkCellRenderer *cell;
	GHashTable *keys_hash;
	GList *keys, *link;

	keys_hash = mail_security_page_list_seahorse_keys ();
	if (!keys_hash || !g_hash_table_size (keys_hash)) {
		if (keys_hash)
			g_hash_table_destroy (keys_hash);
		return NULL;
	}

	store = GTK_LIST_STORE (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING));

	keys = g_hash_table_get_keys (keys_hash);
	keys = g_list_sort_with_data (keys, compare_by_display_name, keys_hash);

	for (link = keys; link; link = g_list_next (link)) {
		const gchar *keyid = link->data, *display_name;
		gchar *description;

		display_name = g_hash_table_lookup (keys_hash, keyid);

		if (keyid && *keyid && display_name && *display_name) {
			GtkTreeIter iter;

			/* Translators: This string is to describe a PGP key in a combo box in mail account's preferences.
					The first '%s' is a key ID, the second '%s' is a display name of the key. */
			description = g_strdup_printf (C_("PGPKeyDescription", "%s — %s"), keyid, display_name);

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
				0, keyid,
				1, description,
				-1);

			g_free (description);
		}
	}

	g_list_free (keys);
	g_hash_table_destroy (keys_hash);

	widget = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (widget), 0);
	gtk_cell_layout_clear (GTK_CELL_LAYOUT (widget));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), cell, "text", 1, NULL);

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (GTK_IS_ENTRY (child))
		gtk_entry_set_placeholder_text (GTK_ENTRY (child), _("Use sender e-mail address"));

	return widget;
}

static void
mail_config_security_page_constructed (GObject *object)
{
	EMailConfigSecurityPage *page;
	ESource *source;
	ESourceMailComposition *composition_ext;
	ESourceOpenPGP *openpgp_ext;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *main_box;
	GtkWidget *expander;
	GtkWidget *expander_vbox;
	GtkSizeGroup *size_group;
	const gchar *extension_name;
	const gchar *text;
	gchar *markup;

#if defined (ENABLE_SMIME)
	ESourceSMIME *smime_ext;
	GtkEntry *entry;
#endif /* ENABLE_SMIME */

	page = E_MAIL_CONFIG_SECURITY_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_security_page_parent_class)->constructed (object);

	source = e_mail_config_security_page_get_identity_source (page);

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	composition_ext = e_source_get_extension (source, extension_name);

	extension_name = E_SOURCE_EXTENSION_OPENPGP;
	openpgp_ext = e_source_get_extension (source, extension_name);

#if defined (ENABLE_SMIME)
	extension_name = E_SOURCE_EXTENSION_SMIME;
	smime_ext = e_source_get_extension (source, extension_name);
#endif /* ENABLE_SMIME */

	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/*** General ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("General");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("_Do not sign meeting requests (for Outlook compatibility)");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		composition_ext, "sign-imip",
		widget, "active",
		G_BINDING_INVERT_BOOLEAN |
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/*** Pretty Good Privacy (OpenPGP) ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Pretty Good Privacy (OpenPGP)");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("OpenPGP _Key ID:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = mail_security_page_get_openpgpg_combo ();
	if (!widget) {
		widget = gtk_entry_new ();
		gtk_entry_set_placeholder_text (GTK_ENTRY (widget), _("Use sender e-mail address"));
	}

	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	if (!GTK_IS_ENTRY (widget)) {
		/* There's expected an entry, thus provide it. */
		widget = gtk_bin_get_child (GTK_BIN (widget));
		g_warn_if_fail (GTK_IS_ENTRY (widget));
	}

	e_binding_bind_object_text_property (
		openpgp_ext, "key-id",
		widget, "text",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("Si_gning algorithm:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"", _("Default"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha1", _("SHA1"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha256", _("SHA256"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha384", _("SHA384"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha512", _("SHA512"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "signing-algorithm",
		widget, "active-id",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	/* Make sure the combo box has an active item. */
	if (gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget)) == NULL)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	text = _("Al_ways sign outgoing messages when using this account");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "sign-by-default",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("Always enc_rypt outgoing messages when using this account");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 4, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "encrypt-by-default",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("Always encrypt to _myself when sending encrypted messages");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 5, 2, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "encrypt-to-self",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	expander = gtk_expander_new_with_mnemonic (_("Ad_vanced Options"));
	gtk_widget_set_margin_start (expander, 12);
	widget = gtk_expander_get_label_widget (GTK_EXPANDER (expander));
	if (widget) {
		PangoAttrList *bold;

		bold = pango_attr_list_new ();
		pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

		gtk_label_set_attributes (GTK_LABEL (widget), bold);

		pango_attr_list_unref (bold);
	}
	gtk_expander_set_expanded (GTK_EXPANDER (expander), FALSE);

	gtk_widget_show (expander);

	expander_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_spacing (GTK_BOX (expander_vbox), 6);
	gtk_widget_set_margin_start (expander_vbox, 24);
	gtk_widget_show (expander_vbox);

	gtk_grid_attach (GTK_GRID (container), expander, 0, 6, 2, 1);
	gtk_grid_attach (GTK_GRID (container), expander_vbox, 0, 7, 2, 1);

	e_binding_bind_property (
		expander, "expanded",
		expander_vbox, "visible",
		G_BINDING_SYNC_CREATE);

	text = _("Always _trust keys in my keyring when encrypting");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (expander_vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "always-trust",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("Prefer _inline sign/encrypt for plain text messages");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (expander_vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "prefer-inline",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("_Lookup keys for encryption in Web Key Directory (WKD)");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (expander_vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "locate-keys",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("Send own _public key in outgoing mails");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (expander_vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "send-public-key",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("Advertise encryption is pre_ferred");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (expander_vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "send-prefer-encrypt",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	e_binding_bind_property (
		openpgp_ext, "send-public-key",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("A_sk before sending mails with own public key");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (expander_vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		openpgp_ext, "ask-send-public-key",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	e_binding_bind_property (
		openpgp_ext, "send-public-key",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

#if defined (ENABLE_SMIME)

	/*** Security MIME (S/MIME) ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Secure MIME (S/MIME)");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 4, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Sig_ning certificate:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		smime_ext, "signing-certificate",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	entry = GTK_ENTRY (widget);

	widget = gtk_button_new_with_label (_("Select"));
	gtk_grid_attach (GTK_GRID (container), widget, 2, 1, 1, 1);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (mail_config_security_page_select_sign_cert),
		entry);

	widget = e_dialog_button_new_with_icon ("edit-clear", _("_Clear"));
	gtk_grid_attach (GTK_GRID (container), widget, 3, 1, 1, 1);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (mail_config_security_page_clear_cert),
		entry);

	text = _("Signing _algorithm:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"", _("Default"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha1", _("SHA1"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha256", _("SHA256"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha384", _("SHA384"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"sha512", _("SHA512"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		smime_ext, "signing-algorithm",
		widget, "active-id",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	/* Make sure the combo box has an active item. */
	if (gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget)) == NULL)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	text = _("Always sign outgoing messages when using this account");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 4, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		smime_ext, "sign-by-default",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	/* Add extra padding between signing stuff and encryption stuff. */
	gtk_widget_set_margin_bottom (widget, 6);

	text = _("Encryption certificate:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 4, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 4, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		smime_ext, "encryption-certificate",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	entry = GTK_ENTRY (widget);

	widget = gtk_button_new_with_label (_("Select"));
	gtk_grid_attach (GTK_GRID (container), widget, 2, 4, 1, 1);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (mail_config_security_page_select_encrypt_cert),
		entry);

	widget = e_dialog_button_new_with_icon ("edit-clear", _("_Clear"));
	gtk_grid_attach (GTK_GRID (container), widget, 3, 4, 1, 1);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (mail_config_security_page_clear_cert),
		entry);

	text = _("Always encrypt outgoing messages when using this account");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 5, 4, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		smime_ext, "encrypt-by-default",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

	text = _("Always encrypt to myself when sending encrypted messages");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 6, 4, 1);
	gtk_widget_show (widget);

	e_binding_bind_property (
		smime_ext, "encrypt-to-self",
		widget, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

#endif /* ENABLE_SMIME */

	g_object_unref (size_group);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	e_extensible_load_extensions (E_EXTENSIBLE (page));
}

static void
e_mail_config_security_page_class_init (EMailConfigSecurityPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_security_page_set_property;
	object_class->get_property = mail_config_security_page_get_property;
	object_class->dispose = mail_config_security_page_dispose;
	object_class->constructed = mail_config_security_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_SOURCE,
		g_param_spec_object (
			"identity-source",
			"Identity Source",
			"Mail identity source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_security_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Security");
	iface->sort_order = E_MAIL_CONFIG_SECURITY_PAGE_SORT_ORDER;
}

static void
e_mail_config_security_page_init (EMailConfigSecurityPage *page)
{
	page->priv = e_mail_config_security_page_get_instance_private (page);
}

EMailConfigPage *
e_mail_config_security_page_new (ESource *identity_source)
{
	g_return_val_if_fail (E_IS_SOURCE (identity_source), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_SECURITY_PAGE,
		"identity-source", identity_source, NULL);
}

ESource *
e_mail_config_security_page_get_identity_source (EMailConfigSecurityPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SECURITY_PAGE (page), NULL);

	return page->priv->identity_source;
}

