/*
 * evolution-book-config-ldap.c
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
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include <e-util/e-util.h>

/* Combo box ordering */
#define LDAP_PORT  389
#define LDAPS_PORT 636
#define MSGC_PORT  3268
#define MSGCS_PORT 3269

typedef ESourceConfigBackend EBookConfigLDAP;
typedef ESourceConfigBackendClass EBookConfigLDAPClass;

typedef struct _Closure Closure;
typedef struct _Context Context;

struct _Closure {
	ESourceConfigBackend *backend;
	ESource *scratch_source;
};

struct _Context {
	GtkWidget *auth_combo;
	GtkWidget *auth_entry;
	GtkWidget *host_entry;
	GtkWidget *port_combo;
	GtkWidget *port_error_image;
	GtkWidget *security_combo;
	GtkWidget *search_base_combo;
	GtkWidget *search_base_button;
	GtkWidget *search_scope_combo;
	GtkWidget *search_filter_entry;
	GtkWidget *limit_spinbutton;
	GtkWidget *can_browse_toggle;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_book_config_ldap_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EBookConfigLDAP,
	e_book_config_ldap,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static Closure *
book_config_ldap_closure_new (ESourceConfigBackend *backend,
                              ESource *scratch_source)
{
	Closure *closure;

	closure = g_slice_new (Closure);
	closure->backend = g_object_ref (backend);
	closure->scratch_source = g_object_ref (scratch_source);

	return closure;
}

static void
book_config_ldap_closure_free (Closure *closure)
{
	g_object_unref (closure->backend);
	g_object_unref (closure->scratch_source);

	g_slice_free (Closure, closure);
}

static void
book_config_ldap_context_free (Context *context)
{
	g_object_unref (context->auth_combo);
	g_object_unref (context->auth_entry);
	g_object_unref (context->host_entry);
	g_object_unref (context->port_combo);
	g_object_unref (context->port_error_image);
	g_object_unref (context->security_combo);
	g_object_unref (context->search_base_combo);
	g_object_unref (context->search_base_button);
	g_object_unref (context->search_scope_combo);
	g_object_unref (context->search_filter_entry);
	g_object_unref (context->limit_spinbutton);
	g_object_unref (context->can_browse_toggle);

	g_slice_free (Context, context);
}

static gboolean
book_config_ldap_port_to_active (GBinding *binding,
                                 const GValue *source_value,
                                 GValue *target_value,
                                 gpointer unused)
{
	guint port;
	gint active;

	port = g_value_get_uint (source_value);

	switch (port) {
		case 0:  /* initialize to LDAP_PORT */
		case LDAP_PORT:
			active = 0;
			break;

		case LDAPS_PORT:
			active = 1;
			break;

		case MSGC_PORT:
			active = 2;
			break;

		case MSGCS_PORT:
			active = 3;
			break;

		default:
			active = -1;
			break;
	}

	g_value_set_int (target_value, active);

	if (active == -1) {
		GObject *target;
		GtkWidget *entry;
		gchar *text;

		target = g_binding_dup_target (binding);
		entry = gtk_bin_get_child (GTK_BIN (target));

		text = g_strdup_printf ("%u", port);
		gtk_entry_set_text (GTK_ENTRY (entry), text);
		g_free (text);
		g_clear_object (&target);
	}

	return TRUE;
}

static gboolean
book_config_ldap_active_to_port (GBinding *binding,
                                 const GValue *source_value,
                                 GValue *target_value,
                                 gpointer unused)
{
	guint port = LDAP_PORT;
	gint active;

	active = g_value_get_int (source_value);

	switch (active) {
		case 0:
			port = LDAP_PORT;
			break;

		case 1:
			port = LDAPS_PORT;
			break;

		case 2:
			port = MSGC_PORT;
			break;

		case 3:
			port = MSGCS_PORT;
			break;

		default:
			active = -1;
			break;
	}

	if (active == -1) {
		GObject *target;
		GtkWidget *entry;
		const gchar *text;
		glong v_long;

		target = g_binding_dup_target (binding);
		entry = gtk_bin_get_child (GTK_BIN (target));
		text = gtk_entry_get_text (GTK_ENTRY (entry));
		g_clear_object (&target);

		v_long = text ? strtol (text, NULL, 10) : 0;
		if (v_long != 0 && v_long == CLAMP (v_long, 0, G_MAXUINT16))
			port = (guint) v_long;
	}

	g_value_set_uint (target_value, port);

	return TRUE;
}

static void
book_config_ldap_port_combo_changed (GtkComboBox *combo_box)
{
	if (gtk_combo_box_get_active (combo_box) == -1)
		g_object_notify (G_OBJECT (combo_box), "active");
}

static gboolean
book_config_ldap_port_to_security (GBinding *binding,
                                   const GValue *source_value,
                                   GValue *target_value,
                                   gpointer unused)
{
	switch (g_value_get_int (source_value)) {
		case 0:  /* LDAP_PORT -> StartTLS */
			g_value_set_int (
				target_value,
				E_SOURCE_LDAP_SECURITY_STARTTLS);
			return TRUE;

		case 1:  /* LDAPS_PORT -> LDAP over SSL */
			g_value_set_int (
				target_value,
				E_SOURCE_LDAP_SECURITY_LDAPS);
			return TRUE;

		case 2:  /* MSGC_PORT -> StartTLS */
			g_value_set_int (
				target_value,
				E_SOURCE_LDAP_SECURITY_STARTTLS);
			return TRUE;

		case 3:  /* MSGCS_PORT -> LDAP over SSL */
			g_value_set_int (
				target_value,
				E_SOURCE_LDAP_SECURITY_LDAPS);
			return TRUE;

		default:
			break;
	}

	return FALSE;
}

typedef struct _SearchBaseData {
	GtkWindow *parent; /* not referenced */
	GtkWidget *search_base_combo;
	GtkWidget *dialog;
	GCancellable *cancellable;
	ESource *source;
	gchar **root_dse;
	GError *error;
} SearchBaseData;

static void
search_base_data_free (gpointer ptr)
{
	SearchBaseData *sbd = ptr;

	if (sbd) {
		if (sbd->dialog)
			gtk_widget_destroy (sbd->dialog);
		g_clear_object (&sbd->search_base_combo);
		g_clear_object (&sbd->cancellable);
		g_clear_object (&sbd->source);
		g_clear_error (&sbd->error);
		g_strfreev (sbd->root_dse);
		g_slice_free (SearchBaseData, sbd);
	}
}

static void
book_config_ldap_search_base_thread (ESimpleAsyncResult *result,
				     gpointer source_object,
				     GCancellable *cancellable)
{
	ESourceAuthentication *auth_extension;
	ESourceLDAP *ldap_extension;
	SearchBaseData *sbd;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	sbd = e_simple_async_result_get_user_data (result);

	g_return_if_fail (sbd != NULL);

	auth_extension = e_source_get_extension (sbd->source, E_SOURCE_EXTENSION_AUTHENTICATION);
	ldap_extension = e_source_get_extension (sbd->source, E_SOURCE_EXTENSION_LDAP_BACKEND);

	if (!e_util_query_ldap_root_dse_sync (
		e_source_authentication_get_host (auth_extension),
		e_source_authentication_get_port (auth_extension),
		e_source_ldap_get_security (ldap_extension),
		&sbd->root_dse, cancellable, &sbd->error)) {
		sbd->root_dse = NULL;
	}
}

static void
book_config_ldap_search_base_done (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	SearchBaseData *sbd = user_data;
	gboolean was_cancelled = FALSE;

	g_return_if_fail (E_IS_SOURCE_CONFIG_BACKEND (source_object));
	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));

	sbd = e_simple_async_result_get_user_data (E_SIMPLE_ASYNC_RESULT (result));
	g_return_if_fail (sbd != NULL);

	if (!g_cancellable_is_cancelled (sbd->cancellable)) {
		g_clear_pointer (&sbd->dialog, gtk_widget_destroy);
	} else {
		was_cancelled = TRUE;
	}

	if (!was_cancelled) {
		if (sbd->error) {
			const gchar *alert_id;

			if (g_error_matches (sbd->error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT))
				alert_id = "addressbook:ldap-init";
			else if (g_error_matches (sbd->error, G_IO_ERROR, G_IO_ERROR_FAILED))
				alert_id = "addressbook:ldap-search-base";
			else
				alert_id = "addressbook:ldap-communicate";

			e_alert_run_dialog_for_args (sbd->parent, alert_id, sbd->error->message, NULL);
		} else if (sbd->root_dse) {
			GtkComboBox *combo_box;
			GtkListStore *store;
			gint ii;

			store = gtk_list_store_new (1, G_TYPE_STRING);

			for (ii = 0; sbd->root_dse[ii]; ii++) {
				GtkTreeIter iter;

				gtk_list_store_append (store, &iter);
				gtk_list_store_set (store, &iter, 0, sbd->root_dse[ii], -1);
			}

			combo_box = GTK_COMBO_BOX (sbd->search_base_combo);
			gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
			gtk_combo_box_set_active (combo_box, 0);

			g_clear_object (&store);
		}
	}
}

static void
search_base_data_response_cb (GtkWidget *dialog,
			      gint response_id,
			      gpointer user_data)
{
	SearchBaseData *sbd = user_data;

	g_return_if_fail (sbd != NULL);
	g_return_if_fail (sbd->dialog == dialog);

	sbd->dialog = NULL;

	g_cancellable_cancel (sbd->cancellable);
	gtk_widget_destroy (dialog);
}

static void
book_config_ldap_search_base_button_clicked_cb (GtkButton *button,
                                                Closure *closure)
{
	GtkWidget *dialog, *label, *content, *spinner, *box;
	GtkWindow *parent;
	ESimpleAsyncResult *result;
	Context *context;
	SearchBaseData *sbd;
	const gchar *uid;

	uid = e_source_get_uid (closure->scratch_source);
	context = g_object_get_data (G_OBJECT (closure->backend), uid);
	g_return_if_fail (context != NULL);

	dialog = gtk_widget_get_toplevel (context->search_base_combo);
	parent = GTK_IS_WINDOW (dialog) ? GTK_WINDOW (dialog) : NULL;

	dialog = gtk_dialog_new_with_buttons ("",
		parent,
		GTK_DIALOG_MODAL,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		NULL);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	spinner = e_spinner_new ();
	e_spinner_start (E_SPINNER (spinner));
	gtk_box_pack_start (GTK_BOX (box), spinner, FALSE, FALSE, 0);

	label = gtk_label_new (_("Looking up server search bases, please wait…"));
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

	gtk_widget_show_all (box);

	content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_container_add (GTK_CONTAINER (content), box);
	gtk_container_set_border_width (GTK_CONTAINER (content), 12);

	sbd = g_slice_new0 (SearchBaseData);
	sbd->parent = parent;
	sbd->search_base_combo = g_object_ref (context->search_base_combo);
	sbd->dialog = dialog;
	sbd->cancellable = g_cancellable_new ();
	sbd->source = g_object_ref (closure->scratch_source);

	result = e_simple_async_result_new (G_OBJECT (closure->backend),
		book_config_ldap_search_base_done, NULL, book_config_ldap_search_base_done);

	e_simple_async_result_set_user_data (result, sbd, search_base_data_free);

	g_signal_connect (dialog, "response", G_CALLBACK (search_base_data_response_cb), sbd);

	e_simple_async_result_run_in_thread (result, G_PRIORITY_DEFAULT,
		book_config_ldap_search_base_thread, sbd->cancellable);

	g_object_unref (result);

	gtk_dialog_run (GTK_DIALOG (dialog));
}

static gboolean
book_config_ldap_query_port_tooltip_cb (GtkComboBox *combo_box,
                                        gint x,
                                        gint y,
                                        gboolean keyboard_mode,
                                        GtkTooltip *tooltip)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text;

	/* XXX This only works if the port number was selected from
	 *     the drop down menu.  No tooltip is shown if the user
	 *     types the port number, even if the same port number
	 *     is listed in the drop down menu.  That's fixable but
	 *     the code would be a lot messier, and is arguably a
	 *     job for GtkComboBox. */

	if (!gtk_combo_box_get_active_iter (combo_box, &iter))
		return FALSE;

	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter, 1, &text, -1);
	gtk_tooltip_set_text (tooltip, text);
	g_free (text);

	return TRUE;
}

static GtkWidget *
book_config_build_port_combo (void)
{
	GtkWidget *widget;
	GtkComboBox *combo_box;
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GtkTreeIter iter;

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (
		store, &iter,
		0, G_STRINGIFY (LDAP_PORT),
		1, _("Standard LDAP Port"), -1);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (
		store, &iter,
		0, G_STRINGIFY (LDAPS_PORT),
		1, _("LDAP over SSL/TLS (deprecated)"), -1);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (
		store, &iter,
		0, G_STRINGIFY (MSGC_PORT),
		1, _("Microsoft Global Catalog"), -1);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (
		store, &iter,
		0, G_STRINGIFY (MSGCS_PORT),
		1, _("Microsoft Global Catalog over SSL/TLS"), -1);

	widget = gtk_combo_box_new_with_entry ();

	combo_box = GTK_COMBO_BOX (widget);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
	gtk_combo_box_set_entry_text_column (combo_box, 0);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "sensitive", FALSE, NULL);
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (widget), renderer, FALSE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (widget), renderer, "text", 1);

	gtk_widget_set_has_tooltip (widget, TRUE);

	g_signal_connect (
		widget, "query-tooltip",
		G_CALLBACK (book_config_ldap_query_port_tooltip_cb), NULL);

	g_object_unref (store);

	return widget;
}

static GtkWidget *
book_config_ldap_insert_notebook_widget (GtkWidget *vbox,
                                         GtkSizeGroup *size_group,
                                         const gchar *caption,
                                         GtkWidget *widget)
{
	GtkWidget *hbox;
	GtkWidget *label;

	/* This is similar to e_source_config_insert_widget(),
	 * but instead adds the widget to the LDAP notebook. */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new (caption);
	gtk_label_set_xalign (GTK_LABEL (label), 1.0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	gtk_size_group_add_widget (size_group, label);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);

	return hbox;
}

static void
book_config_ldap_insert_widgets (ESourceConfigBackend *backend,
                                 ESource *scratch_source)
{
	ESourceConfig *config;
	ESourceExtension *extension;
	GtkSizeGroup *size_group;
	GtkNotebook *notebook;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *page;
	GtkWidget *hbox;
	Context *context;
	PangoAttribute *attr;
	PangoAttrList *attr_list;
	const gchar *extension_name;
	const gchar *tab_label;
	const gchar *uid;
	GtkTreeIter iter;
	GtkListStore *list_store;
	GtkCellRenderer *cell;
	gchar *tmp;
	gboolean is_new_source;

	context = g_slice_new (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) book_config_ldap_context_free);

	e_book_source_config_add_offline_toggle (
		E_BOOK_SOURCE_CONFIG (config), scratch_source);

	container = e_source_config_get_page (config, scratch_source);

	widget = gtk_notebook_new ();
	gtk_widget_set_margin_top (widget, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	notebook = GTK_NOTEBOOK (widget);

	/* For bold section headers. */
	attr_list = pango_attr_list_new ();
	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (attr_list, attr);

	/* Page 1 */

	tab_label = _("Connecting to LDAP");
	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_notebook_append_page (notebook, page, NULL);
	gtk_notebook_set_tab_label_text (notebook, page, tab_label);
	gtk_widget_show (page);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* Page 1 : Server Information */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (_("Server Information"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_entry_new ();
	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Server:"), widget);
	context->host_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = book_config_build_port_combo ();
	hbox = book_config_ldap_insert_notebook_widget (
		container, size_group, _("Port:"), widget);
	context->port_combo = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_BUTTON);
	g_object_set (G_OBJECT (widget),
		"visible", FALSE,
		"has-tooltip", TRUE,
		"tooltip-text", _("Port number is not valid"),
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	context->port_error_image = g_object_ref (widget);

	/* This must follow the order of ESourceLDAPSecurity. */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget),
		_("None"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget),
		_("LDAP over SSL/TLS (deprecated)"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget),
		_("StartTLS (recommended)"));
	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Encryption:"), widget);
	context->security_combo = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		context->port_combo, "active",
		context->security_combo, "active",
		G_BINDING_DEFAULT,
		book_config_ldap_port_to_security,
		NULL,  /* binding is one-way */
		NULL, (GDestroyNotify) NULL);

	/* If this is a new source, initialize security to StartTLS. */
	if (e_source_config_get_original_source (config) == NULL)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);

	/* Page 1 : Authentication */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (_("Authentication"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_combo_box_new ();
	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (list_store));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), cell, "markup", 0, NULL);

	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (widget), 0);
	gtk_combo_box_set_id_column (GTK_COMBO_BOX (widget), 1);

	/* This must follow the order of ESourceLDAPAuthentication. */
	tmp = g_markup_printf_escaped ("%s\n<span font_size=\"x-small\">%s</span>",
		_("Anonymous"),
		_("Username can be left empty"));
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, tmp, -1);
	g_free (tmp);

	tmp = g_markup_printf_escaped ("%s\n<span font_size=\"x-small\">%s</span>",
		_("Using email address"),
		_("requires anonymous access to your LDAP server"));
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, tmp, -1);
	g_free (tmp);

	tmp = g_markup_printf_escaped ("%s\n<span font_size=\"x-small\">%s</span>",
		_("Using distinguished name (DN)"),
		_("for example: uid=user,dc=example,dc=com"));
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, tmp, -1);
	g_free (tmp);

	g_object_unref (list_store);

	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Method:"), widget);
	context->auth_combo = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_widget_set_tooltip_text (widget, _("This is the method Evolution will use to authenticate you."));

	widget = gtk_entry_new ();
	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Username:"), widget);
	context->auth_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	g_object_unref (size_group);

	/* Page 2 */

	tab_label = _("Using LDAP");
	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_notebook_append_page (notebook, page, NULL);
	gtk_notebook_set_tab_label_text (notebook, page, tab_label);
	gtk_widget_show (page);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* Page 2 : Searching */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (_("Searching"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_combo_box_new_with_entry ();
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (widget), 0);
	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Search Base:"), widget);
	context->search_base_combo = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_dialog_button_new_with_icon ("edit-find", _("Find Possible Search Bases"));
	book_config_ldap_insert_notebook_widget (
		container, size_group, NULL, widget);
	context->search_base_button = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Only sensitive when we have complete
	 * server and authentication details. */
	e_binding_bind_property (
		config, "complete",
		context->search_base_button, "sensitive",
		G_BINDING_DEFAULT);

	g_signal_connect_data (
		widget, "clicked",
		G_CALLBACK (book_config_ldap_search_base_button_clicked_cb),
		book_config_ldap_closure_new (backend, scratch_source),
		(GClosureNotify) book_config_ldap_closure_free, 0);

	/* This must follow the order of ESourceLDAPScope. */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("One Level"));
	gtk_combo_box_text_append_text (
		GTK_COMBO_BOX_TEXT (widget), _("Subtree"));
	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Search Scope:"), widget);
	context->search_scope_combo = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_widget_set_tooltip_text (
		widget, _("The search scope defines how deep you would "
		"like the search to extend down the directory tree.  A "
		"search scope of “Subtree” will include all entries "
		"below your search base.  A search scope of “One Level” "
		"will only include the entries one level beneath your "
		"search base."));

	widget = gtk_entry_new ();
	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Search Filter:"), widget);
	context->search_filter_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Page 2 : Downloading */

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (_("Downloading"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	book_config_ldap_insert_notebook_widget (
		container, size_group, _("Limit:"), widget);
	gtk_widget_show (widget);

	hbox = widget;

	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);
	context->limit_spinbutton = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (_("contacts"));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_check_button_new_with_label (
		_("Browse until limit is reached"));
	book_config_ldap_insert_notebook_widget (
		container, size_group, NULL, widget);
	context->can_browse_toggle = g_object_ref (widget);
	gtk_widget_show (widget);

	g_object_unref (size_group);

	pango_attr_list_unref (attr_list);

	/* Bind widgets to extension properties. */

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	is_new_source = !e_source_has_extension (scratch_source, extension_name);
	extension = e_source_get_extension (scratch_source, extension_name);

	e_binding_bind_object_text_property (
		extension, "host",
		context->host_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property_full (
		extension, "port",
		context->port_combo, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		book_config_ldap_port_to_active,
		book_config_ldap_active_to_port,
		NULL, (GDestroyNotify) NULL);

	/* "active" doesn't change when setting custom port
	 * in entry, so check also on the "changed" signal. */
	g_signal_connect (
		context->port_combo, "changed",
		G_CALLBACK (book_config_ldap_port_combo_changed), NULL);

	e_binding_bind_object_text_property (
		extension, "user",
		context->auth_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	extension_name = E_SOURCE_EXTENSION_LDAP_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	e_binding_bind_property (
		extension, "authentication",
		context->auth_combo, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		extension, "can-browse",
		context->can_browse_toggle, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		extension, "limit",
		context->limit_spinbutton, "value",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_bin_get_child (GTK_BIN (context->search_base_combo));

	e_binding_bind_object_text_property (
		extension, "root-dn",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		extension, "scope",
		context->search_scope_combo, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_object_text_property (
		extension, "filter",
		context->search_filter_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		extension, "security",
		context->security_combo, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Initialize values from UI into extension, if the source
	 * is a fresh new source; bindings will take care of proper
	 * values setting into extension properties. */
	if (is_new_source) {
		g_object_notify (G_OBJECT (context->host_entry), "text");
		g_object_notify (G_OBJECT (context->port_combo), "active");
		g_object_notify (G_OBJECT (context->auth_entry), "text");
		g_object_notify (G_OBJECT (context->auth_combo), "active");
	}
}

static gboolean
book_config_ldap_check_complete (ESourceConfigBackend *backend,
                                 ESource *scratch_source)
{
	ESourceLDAPAuthentication auth;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *host;
	const gchar *user;
	guint16 port;
	Context *context;
	gboolean correct, complete = TRUE;

	context = g_object_get_data (G_OBJECT (backend), e_source_get_uid (scratch_source));
	g_return_val_if_fail (context != NULL, FALSE);

	extension_name = E_SOURCE_EXTENSION_LDAP_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	auth = e_source_ldap_get_authentication (E_SOURCE_LDAP (extension));

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (scratch_source, extension_name);

	host = e_source_authentication_get_host (
		E_SOURCE_AUTHENTICATION (extension));
	port = e_source_authentication_get_port (
		E_SOURCE_AUTHENTICATION (extension));
	user = e_source_authentication_get_user (
		E_SOURCE_AUTHENTICATION (extension));

	correct = host != NULL && *host != '\0';
	complete = complete && correct;

	e_util_set_entry_issue_hint (context->host_entry, correct ? NULL : _("Server address cannot be empty"));

	correct = port != 0;
	complete = complete && correct;

	gtk_widget_set_visible (context->port_error_image, !correct);

	correct = TRUE;

	if (auth != E_SOURCE_LDAP_AUTHENTICATION_NONE)
		if (user == NULL || *user == '\0')
			correct = FALSE;

	complete = complete && correct;

	e_util_set_entry_issue_hint (context->auth_entry, correct ?
		(camel_string_is_all_ascii (user) ? NULL : _("User name contains letters, which can prevent log in. Make sure the server accepts such written user name."))
		: _("User name cannot be empty"));

	return complete;
}

static void
e_book_config_ldap_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_BOOK_SOURCE_CONFIG;

	class->parent_uid = "ldap-stub";
	class->backend_name = "ldap";
	class->insert_widgets = book_config_ldap_insert_widgets;
	class->check_complete = book_config_ldap_check_complete;
}

static void
e_book_config_ldap_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_book_config_ldap_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_book_config_ldap_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
