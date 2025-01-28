/*
 * e-mail-config-service-page.c
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

#include "e-mail-config-page.h"
#include "e-mail-config-service-notebook.h"

#include "e-mail-config-service-page.h"

typedef struct _Candidate Candidate;

struct _EMailConfigServicePagePrivate {
	ESourceRegistry *registry;
	EMailConfigServiceBackend *active_backend;
	gchar *email_address;

	GHashTable *backends;
	GPtrArray *candidates;

	/* Hidden candidates are not listed in the
	 * combo box but can still be accessed through
	 * e_mail_config_service_page_lookup_backend(). */
	GPtrArray *hidden_candidates;

	GtkWidget *type_combo;
	GtkWidget *type_label;
	GtkWidget *desc_label;
	GtkWidget *notebook;

	/* Combo box list store */
	GtkListStore *list_store;
};

struct _Candidate {
	gchar *name;
	EMailConfigServiceBackend *backend;

	CamelProvider *provider;
	CamelSettings *settings;
	gulong settings_notify_handler_id;

	GtkWidget *widget;
};

enum {
	PROP_0,
	PROP_ACTIVE_BACKEND,
	PROP_EMAIL_ADDRESS,
	PROP_REGISTRY
};

enum {
	COLUMN_BACKEND_NAME,
	COLUMN_DISPLAY_NAME,
	COLUMN_SELECTABLE,
	NUM_COLUMNS
};

/* Forward Declarations */
static void	e_mail_config_service_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (EMailConfigServicePage, e_mail_config_service_page, 	E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE,
	G_ADD_PRIVATE (EMailConfigServicePage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_service_page_interface_init))

static void
mail_config_service_page_settings_notify_cb (CamelSettings *settings,
                                             GParamSpec *pspec,
                                             EMailConfigPage *page)
{
	e_mail_config_page_changed (page);
}

static Candidate *
mail_config_service_page_new_candidate (EMailConfigServicePage *page,
                                        ESource *scratch_source,
                                        ESource *opt_collection)
{
	Candidate *candidate;
	CamelProvider *provider;
	CamelSettings *settings;
	ESourceBackend *extension;
	EMailConfigServiceBackend *backend;
	EMailConfigServicePageClass *class;
	const gchar *extension_name;
	const gchar *backend_name;
	gulong handler_id;

	/* Get the backend name for this scratch source. */
	class = E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS (page);
	g_return_val_if_fail (class != NULL, NULL);

	extension_name = class->extension_name;
	extension = e_source_get_extension (scratch_source, extension_name);
	backend_name = e_source_backend_get_backend_name (extension);
	g_return_val_if_fail (backend_name != NULL, NULL);

	/* Make sure we have a corresponding EMailConfigServicePageBackend. */
	backend = g_hash_table_lookup (page->priv->backends, backend_name);
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	/* Make sure we have a corresponding CamelProvider. */
	provider = e_mail_config_service_backend_get_provider (backend);
	g_return_val_if_fail (provider != NULL, NULL);

	/* Need to give the backend a scratch source and (if provided) a
	 * scratch collection source before we can extract a CamelSettings
	 * instance, since the CamelSettings instance comes from either the
	 * scratch collection source or else the scratch source. */
	e_mail_config_service_backend_set_source (backend, scratch_source);
	if (opt_collection != NULL)
		e_mail_config_service_backend_set_collection (
			backend, opt_collection);

	/* Backend may have created its own collection source,
	 * so we need to get it from the backend before binding. */
	opt_collection = e_mail_config_service_backend_get_collection (backend);

	/* Keep display names synchronized. */
	if (opt_collection != NULL)
		e_binding_bind_property (
			scratch_source, "display-name",
			opt_collection, "display-name",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

	/* Make sure we have a corresponding CamelSettings. */
	settings = e_mail_config_service_backend_get_settings (backend);
	g_return_val_if_fail (CAMEL_IS_SETTINGS (settings), NULL);

	candidate = g_slice_new0 (Candidate);
	candidate->name = g_strdup (backend_name);
	candidate->backend = g_object_ref (backend);
	candidate->provider = provider;
	candidate->settings = g_object_ref (settings);

	/* Remove the backend so it can't be reused.  If another scratch
	 * source with the same backend name gets added, the hash table
	 * lookup will fail and emit a runtime warning, which we want. */
	g_hash_table_remove (page->priv->backends, backend_name);

	/* Emit "changed" signals for subsequent CamelSettings changes. */
	handler_id = g_signal_connect (
		candidate->settings, "notify",
		G_CALLBACK (mail_config_service_page_settings_notify_cb), page);
	candidate->settings_notify_handler_id = handler_id;

	return candidate;
}

static void
mail_config_service_page_free_candidate (Candidate *candidate)
{
	g_free (candidate->name);

	if (candidate->backend != NULL)
		g_object_unref (candidate->backend);

	if (candidate->settings != NULL) {
		g_signal_handler_disconnect (
			candidate->settings,
			candidate->settings_notify_handler_id);
		g_object_unref (candidate->settings);
	}

	if (candidate->widget != NULL)
		g_object_unref (candidate->widget);

	g_slice_free (Candidate, candidate);
}

static void
mail_config_service_page_init_backends (EMailConfigServicePage *page)
{
	GList *list, *iter;

	page->priv->backends = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	e_extensible_load_extensions (E_EXTENSIBLE (page));

	list = e_extensible_list_extensions (
		E_EXTENSIBLE (page), E_TYPE_MAIL_CONFIG_SERVICE_BACKEND);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		EMailConfigServiceBackend *backend;
		EMailConfigServiceBackendClass *class;

		backend = E_MAIL_CONFIG_SERVICE_BACKEND (iter->data);
		class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);

		if (class->backend_name != NULL)
			g_hash_table_insert (
				page->priv->backends,
				g_strdup (class->backend_name),
				g_object_ref (backend));
	}

	g_list_free (list);
}

static gboolean
mail_config_service_page_backend_to_id (GBinding *binding,
                                        const GValue *source_value,
                                        GValue *target_value,
                                        gpointer user_data)
{
	EMailConfigServiceBackend *backend;
	EMailConfigServiceBackendClass *backend_class;

	backend = g_value_get_object (source_value);
	g_return_val_if_fail (backend != NULL, FALSE);

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);
	g_value_set_string (target_value, backend_class->backend_name);

	return TRUE;
}

static gboolean
mail_config_service_page_id_to_backend (GBinding *binding,
                                        const GValue *source_value,
                                        GValue *target_value,
                                        gpointer user_data)
{
	EMailConfigServiceBackend *backend = NULL;
	GObject *source_object;
	const gchar *backend_name;

	source_object = g_binding_dup_source (binding);
	backend_name = g_value_get_string (source_value);

	if (backend_name != NULL)
		backend = e_mail_config_service_page_lookup_backend (
			E_MAIL_CONFIG_SERVICE_PAGE (source_object),
			backend_name);

	g_value_set_object (target_value, backend);
	g_clear_object (&source_object);

	return TRUE;
}

static gboolean
mail_config_service_page_backend_name_to_description (GBinding *binding,
                                                      const GValue *source_value,
                                                      GValue *target_value,
                                                      gpointer user_data)
{
	CamelProvider *provider;
	const gchar *description;
	const gchar *backend_name;

	backend_name = g_value_get_string (source_value);

	/* XXX Silly special case. */
	if (backend_name == NULL)
		backend_name = "none";

	provider = camel_provider_get (backend_name, NULL);

	if (provider != NULL && provider->description != NULL)
		description = g_dgettext (
			provider->translation_domain,
			provider->description);
	else
		description = "";

	g_value_set_string (target_value, description);

	return TRUE;
}

static void
mail_config_service_page_set_registry (EMailConfigServicePage *page,
                                       ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (page->priv->registry == NULL);

	page->priv->registry = g_object_ref (registry);
}

static void
mail_config_service_page_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_BACKEND:
			e_mail_config_service_page_set_active_backend (
				E_MAIL_CONFIG_SERVICE_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_EMAIL_ADDRESS:
			e_mail_config_service_page_set_email_address (
				E_MAIL_CONFIG_SERVICE_PAGE (object),
				g_value_get_string (value));
			return;

		case PROP_REGISTRY:
			mail_config_service_page_set_registry (
				E_MAIL_CONFIG_SERVICE_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_service_page_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_service_page_get_active_backend (
				E_MAIL_CONFIG_SERVICE_PAGE (object)));
			return;

		case PROP_EMAIL_ADDRESS:
			g_value_set_string (
				value,
				e_mail_config_service_page_get_email_address (
				E_MAIL_CONFIG_SERVICE_PAGE (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_config_service_page_get_registry (
				E_MAIL_CONFIG_SERVICE_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_service_page_dispose (GObject *object)
{
	EMailConfigServicePage *self = E_MAIL_CONFIG_SERVICE_PAGE (object);

	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->active_backend);

	g_hash_table_remove_all (self->priv->backends);
	g_ptr_array_set_size (self->priv->candidates, 0);
	g_ptr_array_set_size (self->priv->hidden_candidates, 0);

	g_clear_object (&self->priv->list_store);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_service_page_parent_class)->dispose (object);
}

static void
mail_config_service_page_finalize (GObject *object)
{
	EMailConfigServicePage *self = E_MAIL_CONFIG_SERVICE_PAGE (object);

	g_free (self->priv->email_address);
	g_hash_table_destroy (self->priv->backends);
	g_ptr_array_free (self->priv->candidates, TRUE);
	g_ptr_array_free (self->priv->hidden_candidates, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_service_page_parent_class)->finalize (object);
}

static void
mail_config_service_page_constructed (GObject *object)
{
	EMailConfigServicePage *page;
	GPtrArray *candidates;
	GPtrArray *hidden_candidates;
	PangoAttribute *attr;
	PangoAttrList *attr_list;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *main_box;
	GtkTreeModel *tree_model;
	GtkCellRenderer *renderer;

	page = E_MAIL_CONFIG_SERVICE_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_service_page_parent_class)->constructed (object);

	/* The candidates array holds scratch ESources, one for each
	 * item in the "type" combo box.  Scratch ESources are never
	 * added to the registry, so backend extensions can make any
	 * changes they want to them.  Whichever scratch ESource is
	 * "active" (selected in the "type" combo box) when the user
	 * clicks OK wins and is written to disk.  The others are
	 * discarded. */
	candidates = g_ptr_array_new_with_free_func (
		(GDestroyNotify) mail_config_service_page_free_candidate);

	/* Hidden candidates are not listed in the "type" combo box
	 * but their scratch ESource can still be "active".  This is
	 * a hack to accommodate groupware backends.  Usually when a
	 * hidden candidate is active the service page will not be
	 * visible anyway. */
	hidden_candidates = g_ptr_array_new_with_free_func (
		(GDestroyNotify) mail_config_service_page_free_candidate);

	main_box = e_mail_config_activity_page_get_internal_box (E_MAIL_CONFIG_ACTIVITY_PAGE (page));
	gtk_box_set_spacing (GTK_BOX (main_box), 12);

	page->priv->candidates = candidates;
	page->priv->hidden_candidates = hidden_candidates;

	/* Build a filtered model for the combo box, where row
	 * visibility is determined by the "selectable" column. */

	page->priv->list_store = gtk_list_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_BACKEND_NAME */
		G_TYPE_STRING,		/* COLUMN_DISPLAY_NAME */
		G_TYPE_BOOLEAN);	/* COLUMN_SELECTABLE */

	tree_model = gtk_tree_model_filter_new (
		GTK_TREE_MODEL (page->priv->list_store), NULL);

	gtk_tree_model_filter_set_visible_column (
		GTK_TREE_MODEL_FILTER (tree_model), COLUMN_SELECTABLE);

	/* Either the combo box or the label is shown, never both.
	 * But we create both widgets and keep them both up-to-date
	 * regardless just because it makes the logic simpler. */

	container = GTK_WIDGET (main_box);

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	attr_list = pango_attr_list_new ();

	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (attr_list, attr);

	widget = gtk_label_new_with_mnemonic (_("Server _Type:"));
	gtk_widget_set_margin_end (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_combo_box_new_with_model (tree_model);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_combo_box_set_id_column (
		GTK_COMBO_BOX (widget), COLUMN_BACKEND_NAME);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	page->priv->type_combo = widget;  /* not referenced */
	gtk_widget_show (widget);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (widget), renderer,
		"text", COLUMN_DISPLAY_NAME);

	widget = gtk_label_new (NULL);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 0, 1, 1);
	page->priv->type_label = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_label_new (_("Description:"));
	gtk_widget_set_margin_end (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_label_set_yalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 2, 1);
	page->priv->desc_label = widget;  /* not referenced */
	gtk_widget_show (widget);

	pango_attr_list_unref (attr_list);

	container = GTK_WIDGET (main_box);

	widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_mail_config_service_notebook_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	page->priv->notebook = widget;  /* not referenced */
	gtk_widget_show (widget);

	/* Keep the notebook's active page number synchronized with our
	 * own "active-backend" property.  Avoid G_BINDING_SYNC_CREATE
	 * since we haven't added any notebook pages. */
	e_binding_bind_property (
		page, "active-backend",
		page->priv->notebook, "active-backend",
		G_BINDING_BIDIRECTIONAL);

	/* Keep the combo box's active row number synchronized with our
	 * own "active-backend" property.  Avoid G_BINDING_SYNC_CREATE
	 * since we haven't added any combo box rows. */
	e_binding_bind_property_full (
		page, "active-backend",
		page->priv->type_combo, "active-id",
		G_BINDING_BIDIRECTIONAL,
		mail_config_service_page_backend_to_id,
		mail_config_service_page_id_to_backend,
		NULL, (GDestroyNotify) NULL);

	/* This keeps the description field up-to-date. */
	e_binding_bind_property_full (
		page->priv->type_combo, "active-id",
		page->priv->desc_label, "label",
		G_BINDING_DEFAULT,
		mail_config_service_page_backend_name_to_description,
		NULL,
		NULL, (GDestroyNotify) NULL);

	/* For the "Server Type", either the combo
	 * box or the label is visible, never both. */
	e_binding_bind_property (
		page->priv->type_combo, "visible",
		page->priv->type_label, "visible",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_INVERT_BOOLEAN);

	g_signal_connect_swapped (
		page->priv->type_combo, "changed",
		G_CALLBACK (e_mail_config_page_changed), page);

	g_object_unref (tree_model);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	mail_config_service_page_init_backends (page);
}

static void
mail_config_service_page_setup_defaults (EMailConfigPage *page)
{
	EMailConfigServicePageClass *class;
	EMailConfigServicePage *self;
	guint ii;

	class = E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS (page);
	g_return_if_fail (class != NULL);

	self = E_MAIL_CONFIG_SERVICE_PAGE (page);

	for (ii = 0; ii < self->priv->candidates->len; ii++) {
		Candidate *candidate;

		candidate = self->priv->candidates->pdata[ii];
		g_return_if_fail (candidate != NULL);

		e_mail_config_service_backend_setup_defaults (
			candidate->backend);
	}

	/* XXX Not sure if we need to call setup_defaults() for
	 *     hidden candidates.  Hold off until a need arises. */

	if (class->default_backend_name != NULL)
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (self->priv->type_combo),
			class->default_backend_name);
}

static gboolean
mail_config_service_page_check_complete (EMailConfigPage *page)
{
	EMailConfigServicePage *self;
	EMailConfigServiceBackend *backend;
	GtkComboBox *type_combo;
	const gchar *backend_name;

	self = E_MAIL_CONFIG_SERVICE_PAGE (page);

	type_combo = GTK_COMBO_BOX (self->priv->type_combo);
	backend_name = gtk_combo_box_get_active_id (type_combo);

	if (backend_name == NULL)
		return FALSE;

	backend = e_mail_config_service_page_lookup_backend (
		E_MAIL_CONFIG_SERVICE_PAGE (page), backend_name);

	return e_mail_config_service_backend_check_complete (backend);
}

static void
mail_config_service_page_commit_changes (EMailConfigPage *page,
                                         GQueue *source_queue)
{
	EMailConfigServicePage *self;
	EMailConfigServiceBackend *backend;
	GtkComboBox *type_combo;
	const gchar *backend_name;

	self = E_MAIL_CONFIG_SERVICE_PAGE (page);

	type_combo = GTK_COMBO_BOX (self->priv->type_combo);
	backend_name = gtk_combo_box_get_active_id (type_combo);
	g_return_if_fail (backend_name != NULL);

	backend = e_mail_config_service_page_lookup_backend (
		E_MAIL_CONFIG_SERVICE_PAGE (page), backend_name);

	e_mail_config_service_backend_commit_changes (backend);
}

static void
e_mail_config_service_page_class_init (EMailConfigServicePageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_service_page_set_property;
	object_class->get_property = mail_config_service_page_get_property;
	object_class->dispose = mail_config_service_page_dispose;
	object_class->finalize = mail_config_service_page_finalize;
	object_class->constructed = mail_config_service_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_BACKEND,
		g_param_spec_object (
			"active-backend",
			"Active Backend",
			"The active service backend",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_EMAIL_ADDRESS,
		g_param_spec_string (
			"email-address",
			"Email Address",
			"The user's email address",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_service_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->setup_defaults = mail_config_service_page_setup_defaults;
	iface->check_complete = mail_config_service_page_check_complete;
	iface->commit_changes = mail_config_service_page_commit_changes;
}

static void
e_mail_config_service_page_init (EMailConfigServicePage *page)
{
	page->priv = e_mail_config_service_page_get_instance_private (page);
}

EMailConfigServiceBackend *
e_mail_config_service_page_get_active_backend (EMailConfigServicePage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page), NULL);

	return page->priv->active_backend;
}

void
e_mail_config_service_page_set_active_backend (EMailConfigServicePage *page,
                                               EMailConfigServiceBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page));

	if (page->priv->active_backend == backend)
		return;

	if (backend != NULL) {
		g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));
		g_object_ref (backend);
	}

	if (page->priv->active_backend != NULL)
		g_object_unref (page->priv->active_backend);

	page->priv->active_backend = backend;

	g_object_notify (G_OBJECT (page), "active-backend");
}

const gchar *
e_mail_config_service_page_get_email_address (EMailConfigServicePage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page), NULL);

	return page->priv->email_address;
}

void
e_mail_config_service_page_set_email_address (EMailConfigServicePage *page,
                                              const gchar *email_address)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page));

	if (g_strcmp0 (page->priv->email_address, email_address) == 0)
		return;

	g_free (page->priv->email_address);
	page->priv->email_address = g_strdup (email_address);

	g_object_notify (G_OBJECT (page), "email-address");
}

ESourceRegistry *
e_mail_config_service_page_get_registry (EMailConfigServicePage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page), NULL);

	return page->priv->registry;
}

EMailConfigServiceBackend *
e_mail_config_service_page_add_scratch_source (EMailConfigServicePage *page,
                                               ESource *scratch_source,
                                               ESource *opt_collection)
{
	GtkWidget *widget;
	GtkLabel *type_label;
	GtkComboBox *type_combo;
	GtkTreeIter iter;
	Candidate *candidate;
	const gchar *display_name;
	gboolean selectable;
	gint page_num;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page), NULL);
	g_return_val_if_fail (E_IS_SOURCE (scratch_source), NULL);

	if (opt_collection != NULL)
		g_return_val_if_fail (E_IS_SOURCE (opt_collection), NULL);

	type_label = GTK_LABEL (page->priv->type_label);
	type_combo = GTK_COMBO_BOX (page->priv->type_combo);

	candidate = mail_config_service_page_new_candidate (
		page, scratch_source, opt_collection);
	g_return_val_if_fail (candidate != NULL, NULL);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	e_mail_config_service_backend_insert_widgets (
		candidate->backend, GTK_BOX (widget));
	candidate->widget = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	g_ptr_array_add (page->priv->candidates, candidate);

	display_name = g_dgettext (
		candidate->provider->translation_domain,
		candidate->provider->name);

	page_num = e_mail_config_service_notebook_add_page (
		E_MAIL_CONFIG_SERVICE_NOTEBOOK (page->priv->notebook),
		candidate->backend, widget);

	selectable = e_mail_config_service_backend_get_selectable (
		candidate->backend);

	gtk_list_store_append (page->priv->list_store, &iter);

	gtk_list_store_set (
		page->priv->list_store, &iter,
		COLUMN_BACKEND_NAME, candidate->name,
		COLUMN_DISPLAY_NAME, display_name,
		COLUMN_SELECTABLE, selectable,
		-1);

	/* The type label is only visible if we have one scratch source,
	 * so just always set the label text to the most recently added
	 * scratch source. */
	gtk_label_set_text (type_label, display_name);

	/* If no combo box row is active yet, choose the new row. */
	if (gtk_combo_box_get_active_id (type_combo) == NULL)
		gtk_combo_box_set_active_id (type_combo, candidate->name);

	/* If the page number of the newly-added notebook page is zero,
	 * show the "type" label.  Otherwise show the "type" combo box.
	 * There's an inverted "visible" binding between the combo box
	 * and label, so we only need to change one of the widgets. */
	gtk_widget_set_visible (GTK_WIDGET (type_combo), page_num > 0);

	return candidate->backend;
}

EMailConfigServiceBackend *
e_mail_config_service_page_lookup_backend (EMailConfigServicePage *page,
                                           const gchar *backend_name)
{
	guint index;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page), NULL);
	g_return_val_if_fail (backend_name != NULL, NULL);

	for (index = 0; index < page->priv->candidates->len; index++) {
		Candidate *candidate;

		candidate = page->priv->candidates->pdata[index];

		if (g_strcmp0 (backend_name, candidate->name) == 0)
			return candidate->backend;
	}

	return NULL;
}

gboolean
e_mail_config_service_page_auto_configure (EMailConfigServicePage *page,
					   EConfigLookup *config_lookup,
					   gboolean *out_is_complete)
{
	EMailConfigServiceBackend *select_backend = NULL;
	gint selected_priority = G_MAXINT;
	gboolean selected_is_complete = FALSE;
	gboolean any_configured = FALSE;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_PAGE (page), FALSE);
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), FALSE);

	for (ii = 0; ii < page->priv->candidates->len; ii++) {
		EMailConfigServiceBackend *backend;
		Candidate *candidate;
		gboolean configured;
		gint priority = G_MAXINT;
		gboolean is_complete = FALSE;

		candidate = page->priv->candidates->pdata[ii];
		backend = candidate->backend;

		configured = e_mail_config_service_backend_auto_configure (backend, config_lookup, &priority, &is_complete);

		if (configured && priority < selected_priority) {
			selected_priority = priority;
			selected_is_complete = is_complete;
			select_backend = backend;
		}

		any_configured = any_configured || configured;
	}

	if (select_backend)
		e_mail_config_service_page_set_active_backend (page, select_backend);

	if (out_is_complete)
		*out_is_complete = selected_is_complete;

	return any_configured;
}

