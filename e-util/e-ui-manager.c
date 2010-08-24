/*
 * e-ui-manager.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

/**
 * SECTION: e-ui-manager
 * @short_description: construct menus and toolbars from a UI definition
 * @include: e-util/e-ui-manager.h
 *
 * This is a #GtkUIManager with support for Evolution's "express" mode,
 * which influences the parsing of UI definitions.
 **/

#include "e-ui-manager.h"
#include "e-util-private.h"

#include <string.h>

#define E_UI_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_UI_MANAGER, EUIManagerPrivate))

/*
 * --- NOTE TO SELF ---
 *
 * While creating this class I was tempted to add an "id" property which
 * EPluginUI could extract from a given EUIManager instead of having the
 * public EPluginUI functions take a separate "id" argument.  Seemed like
 * a nice cleanup until I remembered that an EUIManager instance can have
 * multiple IDs ("aliases"), as in the case of EShellWindow's UI manager.
 * So the UI Manager ID and the instance still need to be kept separate.
 *
 * Mentioning it here in case I forget why I didn't go through with it.
 */

struct _EUIManagerPrivate {
	guint express_mode : 1;
};

enum {
	PROP_0,
	PROP_EXPRESS_MODE
};

G_DEFINE_TYPE (
	EUIManager,
	e_ui_manager,
	GTK_TYPE_UI_MANAGER)

static void
ui_manager_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPRESS_MODE:
			e_ui_manager_set_express_mode (
				E_UI_MANAGER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ui_manager_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPRESS_MODE:
			g_value_set_boolean (
				value, e_ui_manager_get_express_mode (
				E_UI_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static gchar *
ui_manager_filter_ui (EUIManager *ui_manager,
                      const gchar *ui_definition)
{
	gchar **lines;
	gchar *filtered;
	gboolean express_mode;
	gboolean in_conditional = FALSE;
	gboolean include = TRUE;
	gint ii;

	express_mode = e_ui_manager_get_express_mode (ui_manager);

	/*
	 * Very simple C style pre-processing in-line in the XML:
	 * #if [!]EXPRESS\n ... \n#endif\n
	 */
	lines = g_strsplit (ui_definition, "\n", -1);

	for (ii = 0; lines[ii] != NULL; ii++) {
		if (lines[ii][0] == '#') {
			if (!strncmp (lines[ii], "#if ", 4)) {
				gboolean not_express = lines[ii][4] == '!';
				include = express_mode ^ not_express;
				lines[ii][0] = '\0';
				in_conditional = TRUE;
			} else if (!strncmp (lines[ii], "#endif", 6)) {
				lines[ii][0] = '\0';
				include = TRUE;
				in_conditional = FALSE;
			}
		}
		if (!include)
			lines[ii][0] = '\0';
	}

	filtered = g_strjoinv ("\n", lines);

	g_strfreev (lines);

	return filtered;
}

static void
e_ui_manager_class_init (EUIManagerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EUIManagerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = ui_manager_set_property;
	object_class->get_property = ui_manager_get_property;

	class->filter_ui = ui_manager_filter_ui;

	g_object_class_install_property (
		object_class,
		PROP_EXPRESS_MODE,
		g_param_spec_boolean (
			"express-mode",
			"Express Mode",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_ui_manager_init (EUIManager *ui_manager)
{
	ui_manager->priv = E_UI_MANAGER_GET_PRIVATE (ui_manager);
}

/**
 * e_ui_manager_new:
 *
 * Returns a new #EUIManager instance.
 *
 * Returns: a new #EUIManager instance
 **/
GtkUIManager *
e_ui_manager_new (void)
{
	return g_object_new (E_TYPE_UI_MANAGER, NULL);
}

/**
 * e_ui_manager_get_express_mode:
 * @ui_manager: an #EUIManager
 *
 * Returns the "express mode" flag in @ui_manager.
 *
 * Returns: %TRUE if @ui_manager is set to express mode
 **/
gboolean
e_ui_manager_get_express_mode (EUIManager *ui_manager)
{
	g_return_val_if_fail (E_IS_UI_MANAGER (ui_manager), FALSE);

	return ui_manager->priv->express_mode;
}

/**
 * e_ui_manager_set_express_mode:
 * @ui_manager: an #EUIManager
 * @express_mode: express mode flag
 *
 * Sets the "express mode" flag in @ui_manager, which influences how
 * UI definitions are loaded.
 **/
void
e_ui_manager_set_express_mode (EUIManager *ui_manager,
                               gboolean express_mode)
{
	g_return_if_fail (E_IS_UI_MANAGER (ui_manager));

	ui_manager->priv->express_mode = express_mode;

	g_object_notify (G_OBJECT (ui_manager), "express-mode");
}

/**
 * e_ui_manager_add_ui_from_file:
 * @ui_manager: an #EUIManager
 * @basename: basename of the UI definition file
 *
 * Loads a UI definition into @ui_manager from Evolution's UI directory.
 * If the EUIManager:express-mode property is %TRUE, a simplified version
 * of the UI may be presented.
 *
 * Failure here is fatal, since the application can't function without
 * its core UI definitions.
 *
 * Returns: The merge ID for the merged UI.  The merge ID can be used to
 *          unmerge the UI with gtk_ui_manager_remove_ui().
 **/
guint
e_ui_manager_add_ui_from_file (EUIManager *ui_manager,
                               const gchar *basename)
{
	EUIManagerClass *class;
	gchar *filename;
	gchar *contents;
	guint merge_id = 0;
	GError *error = NULL;

	g_return_val_if_fail (E_IS_UI_MANAGER (ui_manager), 0);
	g_return_val_if_fail (basename != NULL, 0);

	class = E_UI_MANAGER_GET_CLASS (ui_manager);
	g_return_val_if_fail (class->filter_ui != NULL, 0);

	filename = g_build_filename (EVOLUTION_UIDIR, basename, NULL);

	if (g_file_get_contents (filename, &contents, NULL, &error)) {
		gchar *filtered;

		/* We could call e_ui_manager_add_ui_from_string() here,
		 * but if an error occurs we'd like to include the file
		 * name in the error message. */

		filtered = class->filter_ui (ui_manager, contents);

		merge_id = gtk_ui_manager_add_ui_from_string (
			GTK_UI_MANAGER (ui_manager), filtered, -1, &error);

		g_free (filtered);
		g_free (contents);
	}

	g_free (filename);

	if (error != NULL) {
		g_error ("%s: %s", basename, error->message);
		g_assert_not_reached ();
	}

	return merge_id;
}

/**
 * e_ui_manager_add_ui_from_string:
 * @ui_manager: an #EUIManager
 * @ui_definition: the UI XML in NULL terminated string form
 * @error: return location for a #GError, or %NULL
 *
 * Loads the given UI definition into @ui_manager.  If the
 * EUIManager:express-mode property is %TRUE, a simplified version of
 * the UI may be presented.
 *
 * Failure here is <i>not</i> fatal, since the function is primarily
 * used to load UI definitions for plugins, which we can get by without.
 *
 * Returns: The merge ID for the merged UI.  The merge ID can be used to
 *          unmerge the UI with gtk_ui_manager_remove_ui().
 **/
guint
e_ui_manager_add_ui_from_string (EUIManager *ui_manager,
                                 const gchar *ui_definition,
                                 GError **error)
{
	EUIManagerClass *class;
	gchar *filtered;
	guint merge_id;

	g_return_val_if_fail (E_IS_UI_MANAGER (ui_manager), 0);
	g_return_val_if_fail (ui_definition != NULL, 0);

	class = E_UI_MANAGER_GET_CLASS (ui_manager);
	g_return_val_if_fail (class->filter_ui != NULL, 0);

	filtered = class->filter_ui (ui_manager, ui_definition);

	merge_id = gtk_ui_manager_add_ui_from_string (
		GTK_UI_MANAGER (ui_manager), filtered, -1, error);

	g_free (filtered);

	return merge_id;
}
