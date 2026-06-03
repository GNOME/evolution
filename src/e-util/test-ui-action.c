/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <locale.h>
#include <e-util/e-util.h>

static void
test_action_secondary_label (void)
{
	EUIAction *action;

	action = e_ui_action_new ("test", "test-action", NULL);

	g_assert_null (e_ui_action_get_secondary_label (action));

	e_ui_action_set_label (action, "_Short");
	e_ui_action_set_secondary_label (action, "Full _Label");

	g_assert_cmpstr (e_ui_action_get_label (action), ==, "_Short");
	g_assert_cmpstr (e_ui_action_get_secondary_label (action), ==, "Full _Label");

	e_ui_action_set_secondary_label (action, NULL);
	g_assert_null (e_ui_action_get_secondary_label (action));

	g_object_unref (action);
}

static void
test_action_secondary_label_property (void)
{
	EUIAction *action;
	gchar *value = NULL;

	action = e_ui_action_new ("test", "test-action", NULL);

	g_object_set (action, "secondary-label", "Property _Value", NULL);
	g_object_get (action, "secondary-label", &value, NULL);

	g_assert_cmpstr (value, ==, "Property _Value");
	g_assert_cmpstr (e_ui_action_get_secondary_label (action), ==, "Property _Value");

	g_free (value);
	g_object_unref (action);
}

static void
test_parser_use_secondary_label (void)
{
	EUIParser *parser;
	EUIElement *root, *menu, *item;
	GError *error = NULL;
	const gchar *data =
		"<eui>"
		"<menu id='test-menu'>"
		"<item action='action1'/>"
		"<item action='action2' use_secondary_label='true'/>"
		"<item action='action3' use_secondary_label='false'/>"
		"</menu>"
		"</eui>";

	parser = e_ui_parser_new ();
	g_assert_true (e_ui_parser_merge_data (parser, data, -1, &error));
	g_assert_no_error (error);

	root = e_ui_parser_get_root (parser);
	g_assert_nonnull (root);

	menu = e_ui_element_get_child (root, 0);
	g_assert_nonnull (menu);

	item = e_ui_element_get_child (menu, 0);
	g_assert_nonnull (item);
	g_assert_cmpstr (e_ui_element_item_get_action (item), ==, "action1");
	g_assert_false (e_ui_element_item_get_use_secondary_label (item));

	item = e_ui_element_get_child (menu, 1);
	g_assert_nonnull (item);
	g_assert_cmpstr (e_ui_element_item_get_action (item), ==, "action2");
	g_assert_true (e_ui_element_item_get_use_secondary_label (item));

	item = e_ui_element_get_child (menu, 2);
	g_assert_nonnull (item);
	g_assert_cmpstr (e_ui_element_item_get_action (item), ==, "action3");
	g_assert_false (e_ui_element_item_get_use_secondary_label (item));

	g_object_unref (parser);
}

static void
test_parser_use_secondary_label_export (void)
{
	EUIParser *parser;
	GError *error = NULL;
	gchar *exported;
	const gchar *data =
		"<eui>"
		"<menu id='test-menu'>"
		"<item action='action1'/>"
		"<item action='action2' use_secondary_label='true'/>"
		"</menu>"
		"</eui>";

	parser = e_ui_parser_new ();
	g_assert_true (e_ui_parser_merge_data (parser, data, -1, &error));
	g_assert_no_error (error);

	exported = e_ui_parser_export (parser, 0);
	g_assert_nonnull (exported);

	g_assert_nonnull (strstr (exported, "use_secondary_label='true'"));
	g_assert_null (strstr (exported, "action1' use_secondary_label"));

	g_free (exported);
	g_object_unref (parser);
}

static void
test_customizer_rename_actions (void)
{
	EUIParser *parser;
	EUIElement *root, *menu, *item;
	GError *error = NULL;
	GPtrArray *accels;
	const gchar *data =
		"<eui>"
		"<menu id='test-menu'>"
		"<item action='old-action-1'/>"
		"<item action='unchanged'/>"
		"<item action='old-action-2'/>"
		"</menu>"
		"<accels action='old-action-1'>"
		"<accel value='&lt;Control&gt;k'/>"
		"</accels>"
		"</eui>";
	static const EUIActionRename renames[] = {
		{ "old-action-1", "new-action-1" },
		{ "old-action-2", "new-action-2" },
	};
	EUICustomizer *customizer;
	EUIManager *manager;

	manager = e_ui_manager_new (NULL);
	customizer = e_ui_manager_get_customizer (manager);

	parser = e_ui_customizer_get_parser (customizer);
	g_assert_nonnull (parser);

	g_assert_true (e_ui_parser_merge_data (parser, data, -1, &error));
	g_assert_no_error (error);

	e_ui_customizer_rename_actions (customizer, renames, G_N_ELEMENTS (renames));

	root = e_ui_parser_get_root (parser);
	g_assert_nonnull (root);

	menu = e_ui_element_get_child (root, 0);
	g_assert_nonnull (menu);

	item = e_ui_element_get_child (menu, 0);
	g_assert_nonnull (item);
	g_assert_cmpstr (e_ui_element_item_get_action (item), ==, "new-action-1");

	item = e_ui_element_get_child (menu, 1);
	g_assert_nonnull (item);
	g_assert_cmpstr (e_ui_element_item_get_action (item), ==, "unchanged");

	item = e_ui_element_get_child (menu, 2);
	g_assert_nonnull (item);
	g_assert_cmpstr (e_ui_element_item_get_action (item), ==, "new-action-2");

	accels = e_ui_parser_get_accels (parser, "new-action-1");
	g_assert_nonnull (accels);
	g_assert_cmpuint (accels->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (accels, 0), ==, "<Control>k");

	g_assert_null (e_ui_parser_get_accels (parser, "old-action-1"));

	g_object_unref (manager);
}

static void
test_customizer_rename_submenu (void)
{
	EUIParser *parser;
	EUIElement *root, *submenu;
	GError *error = NULL;
	const gchar *data =
		"<eui>"
		"<menu id='test-menu'>"
		"<submenu action='old-submenu'>"
		"<item action='child'/>"
		"</submenu>"
		"</menu>"
		"</eui>";
	static const EUIActionRename renames[] = {
		{ "old-submenu", "new-submenu" },
	};
	EUICustomizer *customizer;
	EUIManager *manager;

	manager = e_ui_manager_new (NULL);
	customizer = e_ui_manager_get_customizer (manager);

	parser = e_ui_customizer_get_parser (customizer);
	g_assert_nonnull (parser);

	g_assert_true (e_ui_parser_merge_data (parser, data, -1, &error));
	g_assert_no_error (error);

	e_ui_customizer_rename_actions (customizer, renames, G_N_ELEMENTS (renames));

	root = e_ui_parser_get_root (parser);
	g_assert_nonnull (root);

	submenu = e_ui_element_get_child (e_ui_element_get_child (root, 0), 0);
	g_assert_nonnull (submenu);
	g_assert_cmpstr (e_ui_element_submenu_get_action (submenu), ==, "new-submenu");

	g_object_unref (manager);
}

gint
main (gint argc,
      gchar *argv[])
{
	gint res;

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("https://gitlab.gnome.org/GNOME/evolution/issues/");

	g_test_add_func ("/EUIAction/SecondaryLabel", test_action_secondary_label);
	g_test_add_func ("/EUIAction/SecondaryLabelProperty", test_action_secondary_label_property);
	g_test_add_func ("/EUIParser/UseSecondaryLabel", test_parser_use_secondary_label);
	g_test_add_func ("/EUIParser/UseSecondaryLabelExport", test_parser_use_secondary_label_export);
	g_test_add_func ("/EUICustomizer/RenameActions", test_customizer_rename_actions);
	g_test_add_func ("/EUICustomizer/RenameSubmenu", test_customizer_rename_submenu);

	res = g_test_run ();

	e_misc_util_free_global_memory ();

	return res;
}
