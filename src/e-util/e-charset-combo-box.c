/*
 * e-charset-combo-box.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-charset-combo-box.h"

#include <glib/gi18n.h>

#include "e-charset.h"
#include "e-misc-utils.h"

#define E_CHARSET_COMBO_BOX_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CHARSET_COMBO_BOX, ECharsetComboBoxPrivate))

#define DEFAULT_CHARSET "UTF-8"
#define OTHER_VALUE G_MAXINT

struct _ECharsetComboBoxPrivate {
	GtkActionGroup *action_group;
	GtkRadioAction *other_action;
	GHashTable *charset_index;

	/* Used when the user clicks Cancel in the character set
	 * dialog. Reverts to the previous combo box setting. */
	gint previous_index;

	/* When setting the character set programmatically, this
	 * prevents the custom character set dialog from running. */
	guint block_dialog : 1;
};

enum {
	PROP_0,
	PROP_CHARSET
};

G_DEFINE_TYPE (
	ECharsetComboBox,
	e_charset_combo_box,
	E_TYPE_ACTION_COMBO_BOX)

static void
charset_combo_box_entry_changed_cb (GtkEntry *entry,
                                    GtkDialog *dialog)
{
	const gchar *text;
	gboolean sensitive;

	text = gtk_entry_get_text (entry);
	sensitive = (text != NULL && *text != '\0');
	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, sensitive);
}

static void
charset_combo_box_run_dialog (ECharsetComboBox *combo_box)
{
	GtkDialog *dialog;
	GtkEntry *entry;
	GtkWidget *container;
	GtkWidget *widget;
	GObject *object;
	gpointer parent;
	const gchar *charset;

	/* FIXME Using a dialog for this is lame.  Selecting "Other..."
	 *       should unlock an entry directly in the Preferences tab.
	 *       Unfortunately space in Preferences is at a premium right
	 *       now, but we should revisit this when the space issue is
	 *       finally resolved. */

	parent = gtk_widget_get_toplevel (GTK_WIDGET (combo_box));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	object = G_OBJECT (combo_box->priv->other_action);
	charset = g_object_get_data (object, "charset");

	widget = gtk_dialog_new_with_buttons (
		_("Character Encoding"), parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);

	dialog = GTK_DIALOG (widget);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);

	widget = gtk_dialog_get_action_area (dialog);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

	widget = gtk_dialog_get_content_area (dialog);
	gtk_box_set_spacing (GTK_BOX (widget), 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

	container = widget;

	widget = gtk_label_new (_("Enter the character set to use"));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_entry_new ();
	entry = GTK_ENTRY (widget);
	gtk_entry_set_activates_default (entry, TRUE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (
		entry, "changed",
		G_CALLBACK (charset_combo_box_entry_changed_cb), dialog);

	/* Set the default text -after- connecting the signal handler.
	 * This will initialize the "OK" button to the proper state. */
	gtk_entry_set_text (entry, charset);

	if (gtk_dialog_run (dialog) != GTK_RESPONSE_OK) {
		gint active;

		/* Revert to the previously selected character set. */
		combo_box->priv->block_dialog = TRUE;
		active = combo_box->priv->previous_index;
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
		combo_box->priv->block_dialog = FALSE;

		goto exit;
	}

	charset = gtk_entry_get_text (entry);
	g_return_if_fail (charset != NULL && charset != '\0');

	g_object_set_data_full (
		object, "charset", g_strdup (charset),
		(GDestroyNotify) g_free);

exit:
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
charset_combo_box_notify_charset_cb (ECharsetComboBox *combo_box)
{
	GtkToggleAction *action;

	action = GTK_TOGGLE_ACTION (combo_box->priv->other_action);
	if (!gtk_toggle_action_get_active (action))
		return;

	if (combo_box->priv->block_dialog)
		return;

	/* "Other" action was selected by user. */
	charset_combo_box_run_dialog (combo_box);
}

static void
charset_combo_box_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHARSET:
			e_charset_combo_box_set_charset (
				E_CHARSET_COMBO_BOX (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
charset_combo_box_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHARSET:
			g_value_set_string (
				value, e_charset_combo_box_get_charset (
				E_CHARSET_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
charset_combo_box_dispose (GObject *object)
{
	ECharsetComboBoxPrivate *priv;

	priv = E_CHARSET_COMBO_BOX_GET_PRIVATE (object);

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	if (priv->other_action != NULL) {
		g_object_unref (priv->other_action);
		priv->other_action = NULL;
	}

	g_hash_table_remove_all (priv->charset_index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_charset_combo_box_parent_class)->dispose (object);
}

static void
charset_combo_box_finalize (GObject *object)
{
	ECharsetComboBoxPrivate *priv;

	priv = E_CHARSET_COMBO_BOX_GET_PRIVATE (object);

	g_hash_table_destroy (priv->charset_index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_charset_combo_box_parent_class)->finalize (object);
}

static void
charset_combo_box_constructed (GObject *object)
{
	ECharsetComboBoxPrivate *priv;
	GtkRadioAction *radio_action;
	GSList *group;

	priv = E_CHARSET_COMBO_BOX_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_charset_combo_box_parent_class)->constructed (object);

	radio_action = priv->other_action;
	group = gtk_radio_action_get_group (radio_action);

	e_action_combo_box_set_action (
		E_ACTION_COMBO_BOX (object), radio_action);

	e_action_combo_box_add_separator_after (
		E_ACTION_COMBO_BOX (object), g_slist_length (group));

	e_signal_connect_notify (
		object, "notify::charset",
		G_CALLBACK (charset_combo_box_notify_charset_cb), NULL);
}

static void
charset_combo_box_changed (GtkComboBox *combo_box)
{
	ECharsetComboBoxPrivate *priv;

	priv = E_CHARSET_COMBO_BOX_GET_PRIVATE (combo_box);

	/* Chain up to parent's changed() method. */
	GTK_COMBO_BOX_CLASS (e_charset_combo_box_parent_class)->
		changed (combo_box);

	/* Notify -before- updating previous index. */
	g_object_notify (G_OBJECT (combo_box), "charset");
	priv->previous_index = gtk_combo_box_get_active (combo_box);
}

static void
e_charset_combo_box_class_init (ECharsetComboBoxClass *class)
{
	GObjectClass *object_class;
	GtkComboBoxClass *combo_box_class;

	g_type_class_add_private (class, sizeof (ECharsetComboBoxPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = charset_combo_box_set_property;
	object_class->get_property = charset_combo_box_get_property;
	object_class->dispose = charset_combo_box_dispose;
	object_class->finalize = charset_combo_box_finalize;
	object_class->constructed = charset_combo_box_constructed;

	combo_box_class = GTK_COMBO_BOX_CLASS (class);
	combo_box_class->changed = charset_combo_box_changed;

	g_object_class_install_property (
		object_class,
		PROP_CHARSET,
		g_param_spec_string (
			"charset",
			"Charset",
			"The selected character set",
			"UTF-8",
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_charset_combo_box_init (ECharsetComboBox *combo_box)
{
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GHashTable *charset_index;
	GSList *group, *iter;

	action_group = gtk_action_group_new ("charset-combo-box-internal");

	charset_index = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	combo_box->priv = E_CHARSET_COMBO_BOX_GET_PRIVATE (combo_box);
	combo_box->priv->action_group = action_group;
	combo_box->priv->charset_index = charset_index;

	group = e_charset_add_radio_actions (
		action_group, "charset-", NULL, NULL, NULL);

	/* Populate the character set index. */
	for (iter = group; iter != NULL; iter = iter->next) {
		GObject *object = iter->data;
		const gchar *charset;

		charset = g_object_get_data (object, "charset");
		g_return_if_fail (charset != NULL);

		g_hash_table_insert (
			charset_index, g_strdup (charset),
			g_object_ref (object));
	}

	/* Note the "other" action is not included in the index. */

	radio_action = gtk_radio_action_new (
		"charset-other", _("Other..."), NULL, NULL, OTHER_VALUE);

	g_object_set_data (G_OBJECT (radio_action), "charset", (gpointer) "");

	gtk_radio_action_set_group (radio_action, group);

	combo_box->priv->other_action = radio_action;
}

GtkWidget *
e_charset_combo_box_new (void)
{
	return g_object_new (E_TYPE_CHARSET_COMBO_BOX, NULL);
}

const gchar *
e_charset_combo_box_get_charset (ECharsetComboBox *combo_box)
{
	GtkRadioAction *radio_action;

	g_return_val_if_fail (E_IS_CHARSET_COMBO_BOX (combo_box), NULL);

	radio_action = combo_box->priv->other_action;
	radio_action = e_radio_action_get_current_action (radio_action);

	return g_object_get_data (G_OBJECT (radio_action), "charset");
}

void
e_charset_combo_box_set_charset (ECharsetComboBox *combo_box,
                                 const gchar *charset)
{
	GHashTable *charset_index;
	GtkRadioAction *radio_action;

	g_return_if_fail (E_IS_CHARSET_COMBO_BOX (combo_box));

	if (charset == NULL || *charset == '\0')
		charset = "UTF-8";

	charset_index = combo_box->priv->charset_index;
	radio_action = g_hash_table_lookup (charset_index, charset);

	if (radio_action == NULL) {
		radio_action = combo_box->priv->other_action;
		g_object_set_data_full (
			G_OBJECT (radio_action),
			"charset", g_strdup (charset),
			(GDestroyNotify) g_free);
	}

	combo_box->priv->block_dialog = TRUE;
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (radio_action), TRUE);
	combo_box->priv->block_dialog = FALSE;
}
