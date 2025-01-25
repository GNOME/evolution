/*
 * e-mail-junk-options.c
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

#include "e-mail-junk-options.h"

struct _EMailJunkOptionsPrivate {
	EMailSession *session;

	GtkWidget *label;		/* not referenced */
	GtkWidget *combo_box;		/* not referenced */
	GtkWidget *option_box;		/* not referenced */
	GPtrArray *widgets;		/* not referenced */

	GBinding *active_id_binding;
};

enum {
	PROP_0,
	PROP_SESSION
};

enum {
	COLUMN_FILTER_NAME,
	COLUMN_DISPLAY_NAME
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailJunkOptions, e_mail_junk_options, GTK_TYPE_GRID)

static gboolean
mail_junk_options_junk_filter_to_name (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer user_data)
{
	CamelJunkFilter *junk_filter;
	gboolean success = FALSE;

	junk_filter = g_value_get_object (source_value);

	if (E_IS_MAIL_JUNK_FILTER (junk_filter)) {
		EMailJunkFilterClass *class;

		class = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter);
		g_value_set_string (target_value, class->filter_name);
		success = TRUE;
	}

	return success;
}

static gboolean
mail_junk_options_name_to_junk_filter (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer user_data)
{
	const gchar *filter_name;
	gboolean success = FALSE;

	filter_name = g_value_get_string (source_value);

	if (filter_name != NULL) {
		EMailJunkFilter *junk_filter;

		junk_filter = e_mail_session_get_junk_filter_by_name (
			E_MAIL_SESSION (user_data), filter_name);
		g_value_set_object (target_value, junk_filter);
		success = (junk_filter != NULL);
	}

	return success;
}

static void
mail_junk_options_combo_box_changed_cb (GtkComboBox *combo_box,
                                        EMailJunkOptions *options)
{
	GPtrArray *array;
	gint active;
	guint ii;

	array = options->priv->widgets;
	active = gtk_combo_box_get_active (combo_box);

	for (ii = 0; ii < array->len; ii++) {
		GtkWidget *widget = GTK_WIDGET (array->pdata[ii]);
		gtk_widget_set_visible (widget, ii == active);
	}
}

static void
mail_junk_options_rebuild (EMailJunkOptions *options)
{
	EMailSession *session;
	GtkComboBox *combo_box;
	GtkTreeModel *model;
	GtkBox *option_box;
	GList *list = NULL;
	GList *link;
	guint n_filters;

	session = e_mail_junk_options_get_session (options);
	combo_box = GTK_COMBO_BOX (options->priv->combo_box);
	option_box = GTK_BOX (options->priv->option_box);

	/* Remove the GtkComboBox:active-id binding so it doesn't
	 * affect EMailSession:junk-filter-name when we clear the
	 * combo box's list model. */
	g_clear_object (&options->priv->active_id_binding);

	model = gtk_combo_box_get_model (combo_box);
	gtk_list_store_clear (GTK_LIST_STORE (model));

	g_ptr_array_foreach (
		options->priv->widgets,
		(GFunc) gtk_widget_destroy, NULL);
	g_ptr_array_set_size (options->priv->widgets, 0);

	if (session != NULL)
		list = e_mail_session_get_available_junk_filters (session);

	for (link = list; link != NULL; link = g_list_next (link)) {
		EMailJunkFilter *junk_filter;
		EMailJunkFilterClass *class;
		GtkWidget *widget;
		GtkTreeIter iter;

		junk_filter = E_MAIL_JUNK_FILTER (link->data);
		class = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter);

		gtk_list_store_append (GTK_LIST_STORE (model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			COLUMN_FILTER_NAME, class->filter_name,
			COLUMN_DISPLAY_NAME, class->display_name,
			-1);

		/* Create a configuration widget for this junk filter,
		 * or else just create an empty placeholder widget. */
		widget = e_mail_junk_filter_new_config_widget (junk_filter);
		if (widget == NULL)
			widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

		g_ptr_array_add (options->priv->widgets, widget);

		/* Set extra padding to 12px, since only one child of
		 * 'option_box' is visible at a time, and we still want
		 * the extra padding if the first grid row is invisible. */
		gtk_box_pack_start (option_box, widget, FALSE, FALSE, 12);
	}

	/* Synchronize the combo box with the active junk filter. */
	if (session != NULL) {
		GBinding *binding;

		binding = e_binding_bind_property_full (
			session, "junk-filter",
			combo_box, "active-id",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE,
			mail_junk_options_junk_filter_to_name,
			mail_junk_options_name_to_junk_filter,
			session, (GDestroyNotify) NULL);
		options->priv->active_id_binding = binding;
	}

	/* Select the first combo box item if we need to.  If there's
	 * no first item to select, this will silently do nothing. */
	if (gtk_combo_box_get_active (combo_box) < 0)
		gtk_combo_box_set_active (combo_box, 0);

	/* Update visibility of widgets. */
	n_filters = g_list_length (list);
	gtk_widget_set_visible (GTK_WIDGET (options), n_filters > 0);
	gtk_widget_set_visible (options->priv->label, n_filters > 1);
	gtk_widget_set_visible (options->priv->combo_box, n_filters > 1);

	g_list_free (list);
}

static void
mail_junk_options_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			e_mail_junk_options_set_session (
				E_MAIL_JUNK_OPTIONS (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_junk_options_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_junk_options_get_session (
				E_MAIL_JUNK_OPTIONS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_junk_options_dispose (GObject *object)
{
	EMailJunkOptions *self = E_MAIL_JUNK_OPTIONS (object);

	g_clear_object (&self->priv->session);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_junk_options_parent_class)->dispose (object);
}

static void
mail_junk_options_finalize (GObject *object)
{
	EMailJunkOptions *self = E_MAIL_JUNK_OPTIONS (object);

	g_ptr_array_free (self->priv->widgets, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_junk_options_parent_class)->finalize (object);
}

static void
mail_junk_options_constructed (GObject *object)
{
	EMailJunkOptions *self = E_MAIL_JUNK_OPTIONS (object);
	GtkCellRenderer *cell_renderer;
	GtkCellLayout *cell_layout;
	GtkListStore *list_store;
	GtkWidget *widget;

	/* XXX The margins we're using here are tailored to its
	 *     placement in the Junk tab of Mail Preferences.
	 *     EMailJunkOptions is not really reusable as is. */

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_junk_options_parent_class)->constructed (object);

	gtk_grid_set_column_spacing (GTK_GRID (object), 6);

	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	widget = gtk_label_new (_("Junk filtering software:"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (object), widget, 0, 0, 1, 1);
	self->priv->label = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_id_column (
		GTK_COMBO_BOX (widget), COLUMN_FILTER_NAME);
	gtk_grid_attach (GTK_GRID (object), widget, 1, 0, 1, 1);
	self->priv->combo_box = widget;  /* not referenced */
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (mail_junk_options_combo_box_changed_cb), object);

	/* The config widgets that come from EMailJunkFilter have no
	 * left margin, since they usually include a bold header and
	 * interactive widgets with their own left margin. */
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_grid_attach (GTK_GRID (object), widget, 0, 1, 2, 1);
	self->priv->option_box = widget;  /* not referenced */
	gtk_widget_show (widget);

	cell_layout = GTK_CELL_LAYOUT (self->priv->combo_box);
	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (cell_layout, cell_renderer, FALSE);

	gtk_cell_layout_add_attribute (
		cell_layout, cell_renderer,
		"text", COLUMN_DISPLAY_NAME);

	g_object_unref (list_store);
}

static void
mail_junk_options_map (GtkWidget *widget)
{
	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (e_mail_junk_options_parent_class)->map (widget);

	mail_junk_options_rebuild (E_MAIL_JUNK_OPTIONS (widget));
}

static void
e_mail_junk_options_class_init (EMailJunkOptionsClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_junk_options_set_property;
	object_class->get_property = mail_junk_options_get_property;
	object_class->dispose = mail_junk_options_dispose;
	object_class->finalize = mail_junk_options_finalize;
	object_class->constructed = mail_junk_options_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = mail_junk_options_map;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_junk_options_init (EMailJunkOptions *options)
{
	options->priv = e_mail_junk_options_get_instance_private (options);

	options->priv->widgets = g_ptr_array_new ();
}

GtkWidget *
e_mail_junk_options_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		E_TYPE_MAIL_JUNK_OPTIONS, "session", session, NULL);
}

EMailSession *
e_mail_junk_options_get_session (EMailJunkOptions *options)
{
	g_return_val_if_fail (E_IS_MAIL_JUNK_OPTIONS (options), NULL);

	return options->priv->session;
}

void
e_mail_junk_options_set_session (EMailJunkOptions *options,
                                 EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_JUNK_OPTIONS (options));

	if (options->priv->session == session)
		return;

	if (session != NULL) {
		g_return_if_fail (E_IS_MAIL_SESSION (session));
		g_object_ref (session);
	}

	if (options->priv->session != NULL)
		g_object_unref (options->priv->session);

	options->priv->session = session;

	g_object_notify (G_OBJECT (options), "session");

	mail_junk_options_rebuild (options);
}
