/*
 * e-auth-combo-box.c
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

#include "e-auth-combo-box.h"

struct _EAuthComboBoxPrivate {
	CamelProvider *provider;
};

enum {
	PROP_0,
	PROP_PROVIDER
};

enum {
	COLUMN_MECHANISM,
	COLUMN_DISPLAY_NAME,
	COLUMN_STRIKETHROUGH,
	COLUMN_AUTHTYPE,
	NUM_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (EAuthComboBox, e_auth_combo_box, GTK_TYPE_COMBO_BOX)

static void
auth_combo_box_rebuild_model (EAuthComboBox *combo_box)
{
	GtkComboBox *gtk_combo_box;
	CamelProvider *provider;
	GtkTreeModel *model;
	GList *link;
	const gchar *active_id;

	provider = e_auth_combo_box_get_provider (combo_box);

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	model = gtk_combo_box_get_model (gtk_combo_box);
	active_id = gtk_combo_box_get_active_id (gtk_combo_box);

	gtk_list_store_clear (GTK_LIST_STORE (model));

	if (provider == NULL)
		return;

	for (link = provider->authtypes; link != NULL; link = link->next) {
		CamelServiceAuthType *authtype = link->data;
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			COLUMN_MECHANISM, authtype->authproto,
			COLUMN_DISPLAY_NAME, authtype->name,
			COLUMN_AUTHTYPE, authtype,
			-1);
	}

	/* Try selecting the previous mechanism. */
	if (active_id != NULL)
		gtk_combo_box_set_active_id (gtk_combo_box, active_id);

	/* Or else fall back to the first mechanism. */
	if (gtk_combo_box_get_active (gtk_combo_box) == -1)
		gtk_combo_box_set_active (gtk_combo_box, 0);
}

static void
auth_combo_box_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PROVIDER:
			e_auth_combo_box_set_provider (
				E_AUTH_COMBO_BOX (object),
				g_value_get_pointer (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
auth_combo_box_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PROVIDER:
			g_value_set_pointer (
				value,
				e_auth_combo_box_get_provider (
				E_AUTH_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
auth_combo_box_constructed (GObject *object)
{
	GtkComboBox *combo_box;
	GtkListStore *list_store;
	GtkCellLayout *cell_layout;
	GtkCellRenderer *cell_renderer;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_auth_combo_box_parent_class)->constructed (object);

	list_store = gtk_list_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_MECHANISM */
		G_TYPE_STRING,		/* COLUMN_DISPLAY_NAME */
		G_TYPE_BOOLEAN,		/* COLUMN_STRIKETHROUGH */
		G_TYPE_POINTER);	/* COLUMN_AUTHTYPE */

	combo_box = GTK_COMBO_BOX (object);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_id_column (combo_box, COLUMN_MECHANISM);
	g_object_unref (list_store);

	cell_layout = GTK_CELL_LAYOUT (object);
	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (cell_layout, cell_renderer, TRUE);

	gtk_cell_layout_set_attributes (
		cell_layout, cell_renderer,
		"text", COLUMN_DISPLAY_NAME,
		"strikethrough", COLUMN_STRIKETHROUGH,
		NULL);
}

static void
e_auth_combo_box_class_init (EAuthComboBoxClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = auth_combo_box_set_property;
	object_class->get_property = auth_combo_box_get_property;
	object_class->constructed = auth_combo_box_constructed;

	g_object_class_install_property (
		object_class,
		PROP_PROVIDER,
		g_param_spec_pointer (
			"provider",
			"Provider",
			"The provider to query for auth mechanisms",
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_auth_combo_box_init (EAuthComboBox *combo_box)
{
	combo_box->priv = e_auth_combo_box_get_instance_private (combo_box);
}

GtkWidget *
e_auth_combo_box_new (void)
{
	return g_object_new (E_TYPE_AUTH_COMBO_BOX, NULL);
}

CamelProvider *
e_auth_combo_box_get_provider (EAuthComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_AUTH_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->provider;
}

void
e_auth_combo_box_set_provider (EAuthComboBox *combo_box,
                               CamelProvider *provider)
{
	g_return_if_fail (E_IS_AUTH_COMBO_BOX (combo_box));

	if (provider == combo_box->priv->provider)
		return;

	combo_box->priv->provider = provider;

	g_object_notify (G_OBJECT (combo_box), "provider");

	auth_combo_box_rebuild_model (combo_box);
}

static gint
e_auth_combo_box_get_preference_level (const gchar *authproto)
{
	/* In order of preference, from the least to the best */
	const gchar *protos[] = {
		"CRAM-MD5",
		"DIGEST-MD5",
		"NTLM",
		"GSSAPI",
		"XOAUTH2"
	};
	gint ii;

	if (!authproto)
		return -1;

	for (ii = 0; ii < G_N_ELEMENTS (protos); ii++) {
		if (g_ascii_strcasecmp (protos[ii], authproto) == 0 ||
		    (g_ascii_strcasecmp (protos[ii], "XOAUTH2") == 0 &&
		     camel_sasl_is_xoauth2_alias (authproto)))
			return ii;
	}

	return -1;
}

void
e_auth_combo_box_update_available (EAuthComboBox *combo_box,
                                   GList *available_authtypes)
{
	GtkComboBox *gtk_combo_box;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *xoauth2_available;
	gint active_index;
	gint available_index = -1;
	gint chosen_preference_level = -1;
	gint index = 0;
	gboolean iter_set;

	g_return_if_fail (E_IS_AUTH_COMBO_BOX (combo_box));

	for (xoauth2_available = available_authtypes; xoauth2_available; xoauth2_available = g_list_next (xoauth2_available)) {
		CamelServiceAuthType *auth_type = xoauth2_available->data;

		if (auth_type && (g_strcmp0 (auth_type->authproto, "XOAUTH2") == 0 ||
		    camel_sasl_is_xoauth2_alias (auth_type->authproto))) {
			break;
		}
	}

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	model = gtk_combo_box_get_model (gtk_combo_box);
	active_index = gtk_combo_box_get_active (gtk_combo_box);

	iter_set = gtk_tree_model_get_iter_first (model, &iter);

	while (iter_set) {
		CamelServiceAuthType *authtype;
		gboolean available;
		gint preference_level = -1;

		gtk_tree_model_get (
			model, &iter, COLUMN_AUTHTYPE, &authtype, -1);

		available = g_list_find (available_authtypes, authtype) ||
			(xoauth2_available && camel_sasl_is_xoauth2_alias (authtype->authproto));

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			COLUMN_STRIKETHROUGH, !available, -1);

		if (authtype)
			preference_level = e_auth_combo_box_get_preference_level (authtype->authproto);

		if (index == active_index && !available)
			active_index = -1;

		if (available && (available_index == -1 || chosen_preference_level < preference_level)) {
			available_index = index;
			chosen_preference_level = preference_level;
		}

		iter_set = gtk_tree_model_iter_next (model, &iter);
		index++;
	}

	/* If the active combo_box item turned out to be unavailable
	 * (or there was no active item), select the first available. */
	if (active_index == -1 && available_index != -1)
		gtk_combo_box_set_active (gtk_combo_box, available_index);
}

void
e_auth_combo_box_pick_highest_available (EAuthComboBox *combo_box)
{
	GtkComboBox *gtk_combo_box;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint highest_available_index = -1, index = 0;
	gint chosen_preference_level = -1;

	g_return_if_fail (E_IS_AUTH_COMBO_BOX (combo_box));

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	model = gtk_combo_box_get_model (gtk_combo_box);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			CamelServiceAuthType *authtype = NULL;
			gboolean unavailable = TRUE;
			gint preference_level = -1;

			gtk_tree_model_get (model, &iter,
				COLUMN_STRIKETHROUGH, &unavailable,
				COLUMN_AUTHTYPE, &authtype,
				-1);

			if (authtype)
				preference_level = e_auth_combo_box_get_preference_level (authtype->authproto);

			if (!unavailable && (highest_available_index == -1 || chosen_preference_level < preference_level)) {
				highest_available_index = index;
				chosen_preference_level = preference_level;
			}

			index++;
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	if (highest_available_index != -1)
		gtk_combo_box_set_active (gtk_combo_box, highest_available_index);
}

void
e_auth_combo_box_add_auth_type (EAuthComboBox *combo_box,
				CamelServiceAuthType *auth_type)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_AUTH_COMBO_BOX (combo_box));
	g_return_if_fail (auth_type != NULL);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		COLUMN_MECHANISM, auth_type->authproto,
		COLUMN_DISPLAY_NAME, auth_type->name,
		COLUMN_AUTHTYPE, auth_type,
		-1);
}

void
e_auth_combo_box_remove_auth_type (EAuthComboBox *combo_box,
				   CamelServiceAuthType *auth_type)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_AUTH_COMBO_BOX (combo_box));
	g_return_if_fail (auth_type != NULL);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		CamelServiceAuthType *stored_type = NULL;

		gtk_tree_model_get (model, &iter, COLUMN_AUTHTYPE, &stored_type, -1);
		if (stored_type == auth_type) {
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			break;
		}
	} while (gtk_tree_model_iter_next (model, &iter));
}
