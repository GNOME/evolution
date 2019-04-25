/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <errno.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>
#include <e-util/e-util.h>

#include "calendar-config.h"
#include "e-timezone-entry.h"

#include "e-comp-editor-property-part.h"
#include "e-comp-editor-property-parts.h"

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_SUMMARY \
	(e_comp_editor_property_part_summary_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_SUMMARY, ECompEditorPropertyPartSummary))
#define E_IS_COMP_EDITOR_PROPERTY_PART_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_SUMMARY))

typedef struct _ECompEditorPropertyPartSummary ECompEditorPropertyPartSummary;
typedef struct _ECompEditorPropertyPartSummaryClass ECompEditorPropertyPartSummaryClass;

struct _ECompEditorPropertyPartSummary {
	ECompEditorPropertyPartString parent;
};

struct _ECompEditorPropertyPartSummaryClass {
	ECompEditorPropertyPartStringClass parent_class;
};

GType e_comp_editor_property_part_summary_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartSummary, e_comp_editor_property_part_summary, E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING)

static void
ecepp_summary_create_widgets (ECompEditorPropertyPart *property_part,
			      GtkWidget **out_label_widget,
			      GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartClass *part_class;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SUMMARY (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_summary_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);
	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	*out_label_widget = gtk_label_new_with_mnemonic (C_("ECompEditor", "_Summary:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);
}

static void
e_comp_editor_property_part_summary_init (ECompEditorPropertyPartSummary *part_summary)
{
}

static void
e_comp_editor_property_part_summary_class_init (ECompEditorPropertyPartSummaryClass *klass)
{
	ECompEditorPropertyPartStringClass *part_string_class;
	ECompEditorPropertyPartClass *part_class;

	part_string_class = E_COMP_EDITOR_PROPERTY_PART_STRING_CLASS (klass);
	part_string_class->entry_type = E_TYPE_SPELL_ENTRY;
	part_string_class->prop_kind = I_CAL_SUMMARY_PROPERTY;
	part_string_class->i_cal_new_func = i_cal_property_new_summary;
	part_string_class->i_cal_set_func = i_cal_property_set_summary;
	part_string_class->i_cal_get_func = i_cal_property_get_summary;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_summary_create_widgets;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_summary_new (EFocusTracker *focus_tracker)
{
	ECompEditorPropertyPart *property_part;

	property_part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_SUMMARY, NULL);

	e_comp_editor_property_part_string_attach_focus_tracker (
		E_COMP_EDITOR_PROPERTY_PART_STRING (property_part), focus_tracker);

	return property_part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_LOCATION \
	(e_comp_editor_property_part_location_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_LOCATION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_LOCATION, ECompEditorPropertyPartLocation))
#define E_IS_COMP_EDITOR_PROPERTY_PART_LOCATION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_LOCATION))

typedef struct _ECompEditorPropertyPartLocation ECompEditorPropertyPartLocation;
typedef struct _ECompEditorPropertyPartLocationClass ECompEditorPropertyPartLocationClass;

struct _ECompEditorPropertyPartLocation {
	ECompEditorPropertyPartString parent;
};

struct _ECompEditorPropertyPartLocationClass {
	ECompEditorPropertyPartStringClass parent_class;
};

GType e_comp_editor_property_part_location_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartLocation, e_comp_editor_property_part_location, E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING)

static gchar *
ecepp_location_get_locations_filename (gboolean config_dir_only)
{
	return g_build_filename (e_get_user_config_dir (), "calendar", config_dir_only ? NULL : "locations", NULL);
}

static void
ecepp_location_load_list (GtkEntry *entry)
{
	GtkEntryCompletion *completion;
	GtkListStore *store;
	gchar *filename, *contents = NULL;
	gchar **locations;
	gint row;
	GError *error = NULL;

	g_return_if_fail (GTK_IS_ENTRY (entry));

	completion = gtk_entry_get_completion (entry);
	g_return_if_fail (completion != NULL);

	filename = ecepp_location_get_locations_filename (FALSE);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_free (filename);
		return;
	}

	if (!g_file_get_contents (filename, &contents, NULL, &error)) {
		if (error != NULL) {
			g_warning (
				"%s: Failed to load locations list '%s': %s",
				G_STRFUNC, filename, error->message);
			g_error_free (error);
		}

		g_free (filename);
		return;
	}

	locations = g_strsplit (contents, "\n", 0);
	if (!locations) {
		g_free (contents);
		g_free (filename);
		return;
	}

	row = 0;
	store = GTK_LIST_STORE (gtk_entry_completion_get_model (completion));
	while (locations[row] && *locations[row]) {
		GtkTreeIter iter;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, locations[row], -1);
		row++;
	}

	g_strfreev (locations);
	g_free (contents);
	g_free (filename);
}

static void
ecepp_location_save_list (GtkEntry *entry)
{
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *current_location;
	gchar *filename, *stored_content = NULL;
	gboolean needs_save = TRUE;
	GString *contents;
	GError *error = NULL;

	g_return_if_fail (GTK_IS_ENTRY (entry));

	completion = gtk_entry_get_completion (entry);
	g_return_if_fail (completion != NULL);

	filename = ecepp_location_get_locations_filename (TRUE);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		gint r = g_mkdir_with_parents (filename, 0700);
		if (r < 0) {
			g_warning ("%s: Failed to create %s: %s", G_STRFUNC, filename, g_strerror (errno));
			g_free (filename);
			return;
		}
	}

	g_free (filename);

	filename = ecepp_location_get_locations_filename (FALSE);
	current_location = gtk_entry_get_text (entry);

	/* Put current locatin on the very top of the list */
	contents = g_string_new (current_location);
	if (contents->len > 0)
		g_string_append_c (contents, '\n');

	model = gtk_entry_completion_get_model (completion);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		gint i = 0;
		do {
			gchar *str;

			gtk_tree_model_get (model, &iter, 0, &str, -1);

			/* Skip the current location */
			if (str && *str && g_ascii_strcasecmp (str, current_location) != 0)
				g_string_append_printf (contents, "%s\n", str);

			g_free (str);

			i++;

		} while (gtk_tree_model_iter_next (model, &iter) && (i < 20));
	}

	if (g_file_get_contents (filename, &stored_content, NULL, NULL)) {
		needs_save = g_strcmp0 (stored_content, contents->str) != 0;
		g_free (stored_content);
	}

	if (needs_save) {
		g_file_set_contents (filename, contents->str, -1, &error);
		if (error != NULL) {
			g_warning (
				"%s: Failed to save locations '%s': %s",
				G_STRFUNC, filename, error->message);
			g_error_free (error);
		}
	}

	g_string_free (contents, TRUE);
	g_free (filename);
}

static void
ecepp_location_create_widgets (ECompEditorPropertyPart *property_part,
			       GtkWidget **out_label_widget,
			       GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartClass *part_class;
	GtkEntryCompletion *completion;
	GtkListStore *list_store;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_LOCATION (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_location_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);

	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	completion = gtk_entry_completion_new ();

	list_store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (list_store));
	gtk_entry_completion_set_text_column (completion, 0);

	gtk_entry_set_completion (GTK_ENTRY (*out_edit_widget), completion);
	g_object_unref (completion);

	*out_label_widget = gtk_label_new_with_mnemonic (C_("ECompEditor", "_Location:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);

	ecepp_location_load_list (GTK_ENTRY (*out_edit_widget));
}

static void
ecepp_location_fill_component (ECompEditorPropertyPart *property_part,
			       ICalComponent *component)
{
	ECompEditorPropertyPartClass *part_class;
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_LOCATION (property_part));

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_location_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->fill_component != NULL);

	part_class->fill_component (property_part, component);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_ENTRY (edit_widget));

	ecepp_location_save_list (GTK_ENTRY (edit_widget));
}

static void
e_comp_editor_property_part_location_init (ECompEditorPropertyPartLocation *part_location)
{
}

static void
e_comp_editor_property_part_location_class_init (ECompEditorPropertyPartLocationClass *klass)
{
	ECompEditorPropertyPartStringClass *part_string_class;
	ECompEditorPropertyPartClass *part_class;

	part_string_class = E_COMP_EDITOR_PROPERTY_PART_STRING_CLASS (klass);
	part_string_class->prop_kind = I_CAL_LOCATION_PROPERTY;
	part_string_class->i_cal_new_func = i_cal_property_new_location;
	part_string_class->i_cal_set_func = i_cal_property_set_location;
	part_string_class->i_cal_get_func = i_cal_property_get_location;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_location_create_widgets;
	part_class->fill_component = ecepp_location_fill_component;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_location_new (EFocusTracker *focus_tracker)
{
	ECompEditorPropertyPart *property_part;

	property_part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_LOCATION, NULL);

	e_comp_editor_property_part_string_attach_focus_tracker (
		E_COMP_EDITOR_PROPERTY_PART_STRING (property_part), focus_tracker);

	return property_part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_CATEGORIES \
	(e_comp_editor_property_part_categories_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_CATEGORIES(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_CATEGORIES, ECompEditorPropertyPartCategories))
#define E_IS_COMP_EDITOR_PROPERTY_PART_CATEGORIES(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_CATEGORIES))

typedef struct _ECompEditorPropertyPartCategories ECompEditorPropertyPartCategories;
typedef struct _ECompEditorPropertyPartCategoriesClass ECompEditorPropertyPartCategoriesClass;

struct _ECompEditorPropertyPartCategories {
	ECompEditorPropertyPartString parent;
};

struct _ECompEditorPropertyPartCategoriesClass {
	ECompEditorPropertyPartStringClass parent_class;
};

GType e_comp_editor_property_part_categories_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartCategories, e_comp_editor_property_part_categories, E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING)

static void
ecepp_categories_button_clicked_cb (GtkButton *button,
				    GtkEntry *entry)
{
	g_return_if_fail (GTK_IS_ENTRY (entry));

	e_categories_config_open_dialog_for_entry (entry);
}

static void
ecepp_categories_create_widgets (ECompEditorPropertyPart *property_part,
				 GtkWidget **out_label_widget,
				 GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartClass *part_class;
	GtkEntryCompletion *completion;
	GtkWidget *button;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_CATEGORIES (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_categories_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);
	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (*out_edit_widget), completion);
	g_object_unref (completion);

	button = gtk_button_new_with_mnemonic (C_("ECompEditor", "_Categories..."));
	g_signal_connect (button, "clicked", G_CALLBACK (ecepp_categories_button_clicked_cb), *out_edit_widget);

	*out_label_widget = button;

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);
}

static void
e_comp_editor_property_part_categories_init (ECompEditorPropertyPartCategories *part_categories)
{
	e_comp_editor_property_part_string_set_is_multivalue (
		E_COMP_EDITOR_PROPERTY_PART_STRING (part_categories), TRUE);
}

static void
e_comp_editor_property_part_categories_class_init (ECompEditorPropertyPartCategoriesClass *klass)
{
	ECompEditorPropertyPartStringClass *part_string_class;
	ECompEditorPropertyPartClass *part_class;

	part_string_class = E_COMP_EDITOR_PROPERTY_PART_STRING_CLASS (klass);
	part_string_class->prop_kind = I_CAL_CATEGORIES_PROPERTY;
	part_string_class->i_cal_new_func = i_cal_property_new_categories;
	part_string_class->i_cal_set_func = i_cal_property_set_categories;
	part_string_class->i_cal_get_func = i_cal_property_get_categories;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_categories_create_widgets;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_categories_new (EFocusTracker *focus_tracker)
{
	ECompEditorPropertyPart *property_part;

	property_part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_CATEGORIES, NULL);

	e_comp_editor_property_part_string_attach_focus_tracker (
		E_COMP_EDITOR_PROPERTY_PART_STRING (property_part), focus_tracker);

	return property_part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_DESCRIPTION \
	(e_comp_editor_property_part_description_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_DESCRIPTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DESCRIPTION, ECompEditorPropertyPartDescription))
#define E_IS_COMP_EDITOR_PROPERTY_PART_DESCRIPTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DESCRIPTION))

typedef struct _ECompEditorPropertyPartDescription ECompEditorPropertyPartDescription;
typedef struct _ECompEditorPropertyPartDescriptionClass ECompEditorPropertyPartDescriptionClass;

struct _ECompEditorPropertyPartDescription {
	ECompEditorPropertyPartString parent;
};

struct _ECompEditorPropertyPartDescriptionClass {
	ECompEditorPropertyPartStringClass parent_class;
};

GType e_comp_editor_property_part_description_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartDescription, e_comp_editor_property_part_description, E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING)

static void
ecepp_description_create_widgets (ECompEditorPropertyPart *property_part,
				  GtkWidget **out_label_widget,
				  GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartClass *part_class;
	GtkTextView *text_view;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DESCRIPTION (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_description_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);
	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	*out_label_widget = gtk_label_new_with_mnemonic (C_("ECompEditor", "_Description:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	text_view = GTK_TEXT_VIEW (gtk_bin_get_child (GTK_BIN (*out_edit_widget)));
	gtk_text_view_set_wrap_mode (text_view, GTK_WRAP_WORD);
	gtk_text_view_set_monospace (text_view, TRUE);
	e_buffer_tagger_connect (text_view);
	e_spell_text_view_attach (text_view);

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	g_object_set (G_OBJECT (*out_edit_widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"height-request", 100,
		NULL);

	gtk_widget_show (*out_label_widget);
}

static void
ecepp_description_fill_widget (ECompEditorPropertyPart *property_part,
			       ICalComponent *component)
{
	ECompEditorPropertyPartClass *part_class;
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DESCRIPTION (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_description_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->fill_widget != NULL);

	part_class->fill_widget (property_part, component);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_SCROLLED_WINDOW (edit_widget));

	e_buffer_tagger_update_tags (GTK_TEXT_VIEW (gtk_bin_get_child (GTK_BIN (edit_widget))));
}

static void
e_comp_editor_property_part_description_init (ECompEditorPropertyPartDescription *part_description)
{
}

static void
e_comp_editor_property_part_description_class_init (ECompEditorPropertyPartDescriptionClass *klass)
{
	ECompEditorPropertyPartStringClass *part_string_class;
	ECompEditorPropertyPartClass *part_class;

	part_string_class = E_COMP_EDITOR_PROPERTY_PART_STRING_CLASS (klass);
	part_string_class->entry_type = GTK_TYPE_TEXT_VIEW;
	part_string_class->prop_kind = I_CAL_DESCRIPTION_PROPERTY;
	part_string_class->i_cal_new_func = i_cal_property_new_description;
	part_string_class->i_cal_set_func = i_cal_property_set_description;
	part_string_class->i_cal_get_func = i_cal_property_get_description;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_description_create_widgets;
	part_class->fill_widget = ecepp_description_fill_widget;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_description_new (EFocusTracker *focus_tracker)
{
	ECompEditorPropertyPart *property_part;

	property_part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_DESCRIPTION, NULL);

	e_comp_editor_property_part_string_attach_focus_tracker (
		E_COMP_EDITOR_PROPERTY_PART_STRING (property_part), focus_tracker);

	return property_part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_URL \
	(e_comp_editor_property_part_url_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_URL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_URL, ECompEditorPropertyPartUrl))
#define E_IS_COMP_EDITOR_PROPERTY_PART_URL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_URL))

typedef struct _ECompEditorPropertyPartUrl ECompEditorPropertyPartUrl;
typedef struct _ECompEditorPropertyPartUrlClass ECompEditorPropertyPartUrlClass;

struct _ECompEditorPropertyPartUrl {
	ECompEditorPropertyPartString parent;
};

struct _ECompEditorPropertyPartUrlClass {
	ECompEditorPropertyPartStringClass parent_class;
};

GType e_comp_editor_property_part_url_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartUrl, e_comp_editor_property_part_url, E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING)

static void
ecepp_url_create_widgets (ECompEditorPropertyPart *property_part,
			  GtkWidget **out_label_widget,
			  GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartClass *part_class;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_URL (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_url_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);
	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	*out_label_widget = gtk_label_new_with_mnemonic (C_("ECompEditor", "_Web page:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);
}

static void
e_comp_editor_property_part_url_init (ECompEditorPropertyPartUrl *part_url)
{
}

static void
e_comp_editor_property_part_url_class_init (ECompEditorPropertyPartUrlClass *klass)
{
	ECompEditorPropertyPartStringClass *part_string_class;
	ECompEditorPropertyPartClass *part_class;

	part_string_class = E_COMP_EDITOR_PROPERTY_PART_STRING_CLASS (klass);
	part_string_class->entry_type = E_TYPE_URL_ENTRY;
	part_string_class->prop_kind = I_CAL_URL_PROPERTY;
	part_string_class->i_cal_new_func = i_cal_property_new_url;
	part_string_class->i_cal_set_func = i_cal_property_set_url;
	part_string_class->i_cal_get_func = i_cal_property_get_url;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_url_create_widgets;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_url_new (EFocusTracker *focus_tracker)
{
	ECompEditorPropertyPart *property_part;

	property_part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_URL, NULL);

	e_comp_editor_property_part_string_attach_focus_tracker (
		E_COMP_EDITOR_PROPERTY_PART_STRING (property_part), focus_tracker);

	return property_part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED \
	(e_comp_editor_property_part_datetime_labeled_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED, ECompEditorPropertyPartDatetimeLabeled))
#define E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED))

typedef struct _ECompEditorPropertyPartDatetimeLabeled ECompEditorPropertyPartDatetimeLabeled;
typedef struct _ECompEditorPropertyPartDatetimeLabeledClass ECompEditorPropertyPartDatetimeLabeledClass;

struct _ECompEditorPropertyPartDatetimeLabeled {
	ECompEditorPropertyPartDatetime parent;

	gchar *label;
};

struct _ECompEditorPropertyPartDatetimeLabeledClass {
	ECompEditorPropertyPartDatetimeClass parent_class;
};

GType e_comp_editor_property_part_datetime_labeled_get_type (void) G_GNUC_CONST;

enum {
	DATETIME_LABELED_PROP_0,
	DATETIME_LABELED_PROP_LABEL
};

G_DEFINE_ABSTRACT_TYPE (ECompEditorPropertyPartDatetimeLabeled, e_comp_editor_property_part_datetime_labeled, E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME)

static void
ecepp_datetime_labeled_create_widgets (ECompEditorPropertyPart *property_part,
				       GtkWidget **out_label_widget,
				       GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartDatetimeLabeled *part_datetime_labeled;
	ECompEditorPropertyPartClass *part_class;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_datetime_labeled_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	part_datetime_labeled = E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (property_part);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);
	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	*out_label_widget = gtk_label_new_with_mnemonic (part_datetime_labeled->label);
	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);
}

static void
ecepp_datetime_labeled_set_property (GObject *object,
				     guint property_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	ECompEditorPropertyPartDatetimeLabeled *part_datetime_labeled;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (object));

	part_datetime_labeled = E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (object);

	switch (property_id) {
		case DATETIME_LABELED_PROP_LABEL:
			g_free (part_datetime_labeled->label);
			part_datetime_labeled->label = g_value_dup_string (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ecepp_datetime_labeled_finalize (GObject *object)
{
	ECompEditorPropertyPartDatetimeLabeled *part_datetime_labeled;

	part_datetime_labeled = E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (object);

	g_free (part_datetime_labeled->label);
	part_datetime_labeled->label = NULL;

	G_OBJECT_CLASS (e_comp_editor_property_part_datetime_labeled_parent_class)->finalize (object);
}

static void
e_comp_editor_property_part_datetime_labeled_init (ECompEditorPropertyPartDatetimeLabeled *part_datetime_labeled)
{
}

static void
e_comp_editor_property_part_datetime_labeled_class_init (ECompEditorPropertyPartDatetimeLabeledClass *klass)
{
	ECompEditorPropertyPartClass *part_class;
	GObjectClass *object_class;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_datetime_labeled_create_widgets;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = ecepp_datetime_labeled_set_property;
	object_class->finalize = ecepp_datetime_labeled_finalize;

	g_object_class_install_property (
		object_class,
		DATETIME_LABELED_PROP_LABEL,
		g_param_spec_string (
			"label",
			"Label",
			"Label of the datetime",
			NULL,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_comp_editor_property_part_datetime_labeled_setup (ECompEditorPropertyPartDatetimeLabeled *part_datetime_labeled,
						    gboolean date_only,
						    gboolean allow_no_date_set)
{
	ECompEditorPropertyPartDatetime *part_datetime;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (part_datetime_labeled));

	part_datetime = E_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime_labeled);

	e_comp_editor_property_part_datetime_set_date_only (part_datetime, date_only);
	e_comp_editor_property_part_datetime_set_allow_no_date_set (part_datetime, allow_no_date_set);
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_DTSTART \
	(e_comp_editor_property_part_dtstart_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_DTSTART(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DTSTART, ECompEditorPropertyPartDtstart))
#define E_IS_COMP_EDITOR_PROPERTY_PART_DTSTART(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DTSTART))

typedef struct _ECompEditorPropertyPartDtstart ECompEditorPropertyPartDtstart;
typedef struct _ECompEditorPropertyPartDtstartClass ECompEditorPropertyPartDtstartClass;

struct _ECompEditorPropertyPartDtstart {
	ECompEditorPropertyPartDatetimeLabeled parent;
};

struct _ECompEditorPropertyPartDtstartClass {
	ECompEditorPropertyPartDatetimeLabeledClass parent_class;
};

GType e_comp_editor_property_part_dtstart_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartDtstart, e_comp_editor_property_part_dtstart, E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED)

static void
e_comp_editor_property_part_dtstart_init (ECompEditorPropertyPartDtstart *part_dtstart)
{
}

static void
e_comp_editor_property_part_dtstart_class_init (ECompEditorPropertyPartDtstartClass *klass)
{
	ECompEditorPropertyPartDatetimeClass *part_datetime_class;

	part_datetime_class = E_COMP_EDITOR_PROPERTY_PART_DATETIME_CLASS (klass);
	part_datetime_class->prop_kind = I_CAL_DTSTART_PROPERTY;
	part_datetime_class->i_cal_new_func = i_cal_property_new_dtstart;
	part_datetime_class->i_cal_set_func = i_cal_property_set_dtstart;
	part_datetime_class->i_cal_get_func = i_cal_property_get_dtstart;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_dtstart_new (const gchar *label,
					 gboolean date_only,
					 gboolean allow_no_date_set)
{
	ECompEditorPropertyPart *part;

	part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_DTSTART,
		"label", label,
		NULL);

	e_comp_editor_property_part_datetime_labeled_setup (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (part),
		date_only, allow_no_date_set);

	return part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_DTEND \
	(e_comp_editor_property_part_dtend_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_DTEND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DTEND, ECompEditorPropertyPartDtend))
#define E_IS_COMP_EDITOR_PROPERTY_PART_DTEND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DTEND))

typedef struct _ECompEditorPropertyPartDtend ECompEditorPropertyPartDtend;
typedef struct _ECompEditorPropertyPartDtendClass ECompEditorPropertyPartDtendClass;

struct _ECompEditorPropertyPartDtend {
	ECompEditorPropertyPartDatetimeLabeled parent;
};

struct _ECompEditorPropertyPartDtendClass {
	ECompEditorPropertyPartDatetimeLabeledClass parent_class;
};

GType e_comp_editor_property_part_dtend_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartDtend, e_comp_editor_property_part_dtend, E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED)

static void
e_comp_editor_property_part_dtend_init (ECompEditorPropertyPartDtend *part_dtend)
{
}

static void
e_comp_editor_property_part_dtend_class_init (ECompEditorPropertyPartDtendClass *klass)
{
	ECompEditorPropertyPartDatetimeClass *part_datetime_class;

	part_datetime_class = E_COMP_EDITOR_PROPERTY_PART_DATETIME_CLASS (klass);
	part_datetime_class->prop_kind = I_CAL_DTEND_PROPERTY;
	part_datetime_class->i_cal_new_func = i_cal_property_new_dtend;
	part_datetime_class->i_cal_set_func = i_cal_property_set_dtend;
	part_datetime_class->i_cal_get_func = i_cal_property_get_dtend;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_dtend_new (const gchar *label,
				       gboolean date_only,
				       gboolean allow_no_date_set)
{
	ECompEditorPropertyPart *part;

	part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_DTEND,
		"label", label,
		NULL);

	e_comp_editor_property_part_datetime_labeled_setup (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (part),
		date_only, allow_no_date_set);

	return part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_DUE \
	(e_comp_editor_property_part_due_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_DUE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DUE, ECompEditorPropertyPartDue))
#define E_IS_COMP_EDITOR_PROPERTY_PART_DUE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DUE))

typedef struct _ECompEditorPropertyPartDue ECompEditorPropertyPartDue;
typedef struct _ECompEditorPropertyPartDueClass ECompEditorPropertyPartDueClass;

struct _ECompEditorPropertyPartDue {
	ECompEditorPropertyPartDatetimeLabeled parent;
};

struct _ECompEditorPropertyPartDueClass {
	ECompEditorPropertyPartDatetimeLabeledClass parent_class;
};

GType e_comp_editor_property_part_due_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartDue, e_comp_editor_property_part_due, E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED)

static void
e_comp_editor_property_part_due_init (ECompEditorPropertyPartDue *part_due)
{
}

static void
e_comp_editor_property_part_due_class_init (ECompEditorPropertyPartDueClass *klass)
{
	ECompEditorPropertyPartDatetimeClass *part_datetime_class;

	part_datetime_class = E_COMP_EDITOR_PROPERTY_PART_DATETIME_CLASS (klass);
	part_datetime_class->prop_kind = I_CAL_DUE_PROPERTY;
	part_datetime_class->i_cal_new_func = i_cal_property_new_due;
	part_datetime_class->i_cal_set_func = i_cal_property_set_due;
	part_datetime_class->i_cal_get_func = i_cal_property_get_due;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_due_new (gboolean date_only,
				     gboolean allow_no_date_set)
{
	ECompEditorPropertyPart *part;

	part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_DUE,
		"label", C_("ECompEditor", "D_ue date:"),
		NULL);

	e_comp_editor_property_part_datetime_labeled_setup (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (part),
		date_only, allow_no_date_set);

	return part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_COMPLETED \
	(e_comp_editor_property_part_completed_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_COMPLETED(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_COMPLETED, ECompEditorPropertyPartCompleted))
#define E_IS_COMP_EDITOR_PROPERTY_PART_COMPLETED(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_COMPLETED))

typedef struct _ECompEditorPropertyPartCompleted ECompEditorPropertyPartCompleted;
typedef struct _ECompEditorPropertyPartCompletedClass ECompEditorPropertyPartCompletedClass;

struct _ECompEditorPropertyPartCompleted {
	ECompEditorPropertyPartDatetimeLabeled parent;
};

struct _ECompEditorPropertyPartCompletedClass {
	ECompEditorPropertyPartDatetimeLabeledClass parent_class;
};

GType e_comp_editor_property_part_completed_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartCompleted, e_comp_editor_property_part_completed, E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED)

static void
e_comp_editor_property_part_completed_ensure_date_time (ICalTime *pvalue)
{
	if (!pvalue)
		return;

	if (i_cal_time_is_date (pvalue)) {
		i_cal_time_set_is_date (pvalue, FALSE);
		i_cal_time_set_time (pvalue, 0, 0, 0);
		i_cal_time_set_timezone (pvalue, i_cal_timezone_get_utc_timezone ());
	} else if (!i_cal_time_is_utc (pvalue)) {
		/* Make sure the time is in UTC */
		i_cal_timezone_convert_time (pvalue, i_cal_time_get_timezone (pvalue), i_cal_timezone_get_utc_timezone ());
		i_cal_time_set_timezone (pvalue, i_cal_timezone_get_utc_timezone ());
	}
}

static ICalProperty *
e_comp_editor_property_part_completed_new_func_wrapper (ICalTime *value)
{
	e_comp_editor_property_part_completed_ensure_date_time (value);

	return i_cal_property_new_completed (value);
}

static void
e_comp_editor_property_part_completed_set_func_wrapper (ICalProperty *prop,
							ICalTime *value)
{
	e_comp_editor_property_part_completed_ensure_date_time (value);

	i_cal_property_set_completed (prop, value);
}

static void
e_comp_editor_property_part_completed_init (ECompEditorPropertyPartCompleted *part_completed)
{
}

static void
e_comp_editor_property_part_completed_class_init (ECompEditorPropertyPartCompletedClass *klass)
{
	ECompEditorPropertyPartDatetimeClass *part_datetime_class;

	part_datetime_class = E_COMP_EDITOR_PROPERTY_PART_DATETIME_CLASS (klass);
	part_datetime_class->prop_kind = I_CAL_COMPLETED_PROPERTY;
	part_datetime_class->i_cal_new_func = e_comp_editor_property_part_completed_new_func_wrapper;
	part_datetime_class->i_cal_set_func = e_comp_editor_property_part_completed_set_func_wrapper;
	part_datetime_class->i_cal_get_func = i_cal_property_get_completed;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_completed_new (gboolean date_only,
					   gboolean allow_no_date_set)
{
	ECompEditorPropertyPart *part;

	part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_COMPLETED,
		"label", C_("ECompEditor", "Date _completed:"),
		NULL);

	e_comp_editor_property_part_datetime_labeled_setup (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME_LABELED (part),
		date_only, allow_no_date_set);

	return part;
}

/* ************************************************************************* */

ECompEditorPropertyPart *
e_comp_editor_property_part_classification_new (void)
{
	ECompEditorPropertyPartPickerMap map[] = {
		{ I_CAL_CLASS_PUBLIC,       NC_("ECompEditor", "Public"),       FALSE, NULL },
		{ I_CAL_CLASS_PRIVATE,      NC_("ECompEditor", "Private"),      FALSE, NULL },
		{ I_CAL_CLASS_CONFIDENTIAL, NC_("ECompEditor", "Confidential"), FALSE, NULL }
	};
	GSettings *settings;
	ECompEditorPropertyPart *part;
	gboolean classify_private;
	gint ii, n_elems = G_N_ELEMENTS (map);

	for (ii = 0; ii < n_elems; ii++) {
		map[ii].description = g_dpgettext2 (GETTEXT_PACKAGE, "ECompEditor", map[ii].description);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	classify_private = g_settings_get_boolean (settings, "classify-private");
	g_object_unref (settings);

	part = e_comp_editor_property_part_picker_with_map_new (map, n_elems,
		C_("ECompEditor", "C_lassification:"),
		I_CAL_CLASS_PROPERTY,
		(ECompEditorPropertyPartPickerMapICalNewFunc) i_cal_property_new_class,
		(ECompEditorPropertyPartPickerMapICalSetFunc) i_cal_property_set_class,
		(ECompEditorPropertyPartPickerMapICalGetFunc) i_cal_property_get_class);

	e_comp_editor_property_part_picker_with_map_set_selected (
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part),
		classify_private ? ICAL_CLASS_PRIVATE : ICAL_CLASS_PUBLIC);

	return part;
}

/* ************************************************************************* */

ECompEditorPropertyPart *
e_comp_editor_property_part_status_new (void)
{
	ECompEditorPropertyPartPickerMap map[] = {
		{ I_CAL_STATUS_NONE,      NC_("ECompEditor", "Not Started"), TRUE,  NULL },
		{ I_CAL_STATUS_INPROCESS, NC_("ECompEditor", "In Progress"), FALSE, NULL },
		{ I_CAL_STATUS_COMPLETED, NC_("ECompEditor", "Completed"),   FALSE, NULL },
		{ I_CAL_STATUS_CANCELLED, NC_("ECompEditor", "Cancelled"),    FALSE, NULL }
	};
	gint ii, n_elems = G_N_ELEMENTS (map);

	for (ii = 0; ii < n_elems; ii++) {
		map[ii].description = g_dpgettext2 (GETTEXT_PACKAGE, "ECompEditor", map[ii].description);
	}

	return e_comp_editor_property_part_picker_with_map_new (map, n_elems,
		C_("ECompEditor", "_Status:"),
		I_CAL_STATUS_PROPERTY,
		(ECompEditorPropertyPartPickerMapICalNewFunc) i_cal_property_new_status,
		(ECompEditorPropertyPartPickerMapICalSetFunc) i_cal_property_set_status,
		(ECompEditorPropertyPartPickerMapICalGetFunc) i_cal_property_get_status);
}

/* ************************************************************************* */

static gboolean
ecepp_priority_matches (gint map_value,
			gint component_value)
{
	if (map_value == component_value)
		return TRUE;

	if (component_value == 0)
		return map_value == 0;
	else if (component_value <= 4)
		return map_value == 3;
	else if (component_value == 5)
		return map_value == 5;
	else
		return map_value == 7;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_priority_new (void)
{
	ECompEditorPropertyPartPickerMap map[] = {
		{ 0, NC_("ECompEditor", "Undefined"), TRUE,  ecepp_priority_matches },
		{ 3, NC_("ECompEditor", "High"),      FALSE, ecepp_priority_matches },
		{ 5, NC_("ECompEditor", "Normal"),    FALSE, ecepp_priority_matches },
		{ 7, NC_("ECompEditor", "Low"),       FALSE, ecepp_priority_matches }
	};
	gint ii, n_elems = G_N_ELEMENTS (map);

	for (ii = 0; ii < n_elems; ii++) {
		map[ii].description = g_dpgettext2 (GETTEXT_PACKAGE, "ECompEditor", map[ii].description);
	}

	return e_comp_editor_property_part_picker_with_map_new (map, n_elems,
		C_("ECompEditor", "Priorit_y:"),
		I_CAL_PRIORITY_PROPERTY,
		i_cal_property_new_priority,
		i_cal_property_set_priority,
		i_cal_property_get_priority);
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_PERCENTCOMPLETE \
	(e_comp_editor_property_part_percentcomplete_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_PERCENTCOMPLETE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PERCENTCOMPLETE, ECompEditorPropertyPartPercentcomplete))
#define E_IS_COMP_EDITOR_PROPERTY_PART_PERCENTCOMPLETE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PERCENTCOMPLETE))

typedef struct _ECompEditorPropertyPartPercentcomplete ECompEditorPropertyPartPercentcomplete;
typedef struct _ECompEditorPropertyPartPercentcompleteClass ECompEditorPropertyPartPercentcompleteClass;

struct _ECompEditorPropertyPartPercentcomplete {
	ECompEditorPropertyPartSpin parent;
};

struct _ECompEditorPropertyPartPercentcompleteClass {
	ECompEditorPropertyPartSpinClass parent_class;
};

GType e_comp_editor_property_part_percentcomplete_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartPercentcomplete, e_comp_editor_property_part_percentcomplete, E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN)

static void
ecepp_percentcomplete_create_widgets (ECompEditorPropertyPart *property_part,
				      GtkWidget **out_label_widget,
				      GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartClass *part_class;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PERCENTCOMPLETE (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_percentcomplete_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);
	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	*out_label_widget = gtk_label_new_with_mnemonic (C_("ECompEditor", "Percent complete:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);
}

static void
e_comp_editor_property_part_percentcomplete_init (ECompEditorPropertyPartPercentcomplete *part_percentcomplete)
{
}

static void
e_comp_editor_property_part_percentcomplete_class_init (ECompEditorPropertyPartPercentcompleteClass *klass)
{
	ECompEditorPropertyPartSpinClass *part_spin_class;
	ECompEditorPropertyPartClass *part_class;

	part_spin_class = E_COMP_EDITOR_PROPERTY_PART_SPIN_CLASS (klass);
	part_spin_class->prop_kind = I_CAL_PERCENTCOMPLETE_PROPERTY;
	part_spin_class->i_cal_new_func = i_cal_property_new_percentcomplete;
	part_spin_class->i_cal_set_func = i_cal_property_set_percentcomplete;
	part_spin_class->i_cal_get_func = i_cal_property_get_percentcomplete;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_percentcomplete_create_widgets;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_percentcomplete_new (void)
{
	ECompEditorPropertyPart *part;

	part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_PERCENTCOMPLETE, NULL);

	e_comp_editor_property_part_spin_set_range (E_COMP_EDITOR_PROPERTY_PART_SPIN (part), 0, 100);

	return part;
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_TIMEZONE \
	(e_comp_editor_property_part_timezone_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_TIMEZONE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_TIMEZONE, ECompEditorPropertyPartTimezone))
#define E_IS_COMP_EDITOR_PROPERTY_PART_TIMEZONE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_TIMEZONE))

typedef struct _ECompEditorPropertyPartTimezone ECompEditorPropertyPartTimezone;
typedef struct _ECompEditorPropertyPartTimezoneClass ECompEditorPropertyPartTimezoneClass;

struct _ECompEditorPropertyPartTimezone {
	ECompEditorPropertyPart parent;
};

struct _ECompEditorPropertyPartTimezoneClass {
	ECompEditorPropertyPartClass parent_class;
};

GType e_comp_editor_property_part_timezone_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartTimezone, e_comp_editor_property_part_timezone, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static void
ecepp_timezone_create_widgets (ECompEditorPropertyPart *property_part,
			       GtkWidget **out_label_widget,
			       GtkWidget **out_edit_widget)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TIMEZONE (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	*out_label_widget = gtk_label_new_with_mnemonic (C_("ECompEditor", "Time _zone:"));

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);

	*out_edit_widget = e_timezone_entry_new ();
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (*out_edit_widget), calendar_config_get_icaltimezone ());

	gtk_widget_show (*out_edit_widget);

	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	g_signal_connect_swapped (*out_edit_widget, "changed",
		G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);
}

static void
ecepp_timezone_fill_widget (ECompEditorPropertyPart *property_part,
			    ICalComponent *component)
{
	ICalTime * (* get_func) (ICalProperty *prop);
	ICalProperty *prop;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TIMEZONE (property_part));

	get_func = i_cal_property_get_dtstart;
	prop = i_cal_component_get_first_property (component, I_CAL_DTSTART_PROPERTY);

	if (!prop) {
		get_func = i_cal_property_get_dtend;
		prop = i_cal_component_get_first_property (component, I_CAL_DTEND_PROPERTY);
	}

	if (!prop) {
		get_func = i_cal_property_get_due;
		prop = i_cal_component_get_first_property (component, I_CAL_DUE_PROPERTY);
	}

	if (prop) {
		ICalTime *itt;

		itt = get_func (prop);
		if (itt && i_cal_time_get_timezone (itt)) {
			GtkWidget *edit_widget;

			edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
			g_return_if_fail (E_IS_TIMEZONE_ENTRY (edit_widget));

			e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (edit_widget), i_cal_time_get_timezone (itt));
		}

		g_clear_object (&itt);
		g_object_unref (prop);
	}
}

static void
ecepp_timezone_fill_component (ECompEditorPropertyPart *property_part,
			       ICalComponent *component)
{
	/* Nothing to do here, this is sort-of virtual property part */
}

static void
e_comp_editor_property_part_timezone_init (ECompEditorPropertyPartTimezone *part_timezone)
{
}

static void
e_comp_editor_property_part_timezone_class_init (ECompEditorPropertyPartTimezoneClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_timezone_create_widgets;
	part_class->fill_widget = ecepp_timezone_fill_widget;
	part_class->fill_component = ecepp_timezone_fill_component;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_timezone_new (void)
{
	return g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_TIMEZONE, NULL);
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY \
	(e_comp_editor_property_part_transparency_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY, ECompEditorPropertyPartTransparency))
#define E_IS_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY))

typedef struct _ECompEditorPropertyPartTransparency ECompEditorPropertyPartTransparency;
typedef struct _ECompEditorPropertyPartTransparencyClass ECompEditorPropertyPartTransparencyClass;

struct _ECompEditorPropertyPartTransparency {
	ECompEditorPropertyPart parent;
};

struct _ECompEditorPropertyPartTransparencyClass {
	ECompEditorPropertyPartClass parent_class;
};

GType e_comp_editor_property_part_transparency_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartTransparency, e_comp_editor_property_part_transparency, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static void
ecepp_transparency_create_widgets (ECompEditorPropertyPart *property_part,
				   GtkWidget **out_label_widget,
				   GtkWidget **out_edit_widget)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	*out_label_widget = NULL;

	*out_edit_widget = gtk_check_button_new_with_mnemonic (C_("ECompEditor", "Show time as _busy"));

	g_object_set (G_OBJECT (*out_edit_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_edit_widget);

	g_signal_connect_swapped (*out_edit_widget, "toggled",
		G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);
}

static void
ecepp_transparency_fill_widget (ECompEditorPropertyPart *property_part,
				ICalComponent *component)
{
	GtkWidget *edit_widget;
	ICalProperty *prop;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY (property_part));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_CHECK_BUTTON (edit_widget));

	prop = i_cal_component_get_first_property (component, I_CAL_TRANSP_PROPERTY);
	if (prop) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edit_widget),
			i_cal_property_get_transp (prop) == I_CAL_TRANSP_OPAQUE);

		g_object_unref (prop);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edit_widget), TRUE);
	}
}

static void
ecepp_transparency_fill_component (ECompEditorPropertyPart *property_part,
				   ICalComponent *component)
{
	GtkWidget *edit_widget;
	ICalProperty *prop;
	ICalPropertyTransp value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY (property_part));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_CHECK_BUTTON (edit_widget));

	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (edit_widget)) ? I_CAL_TRANSP_OPAQUE : I_CAL_TRANSP_TRANSPARENT;

	prop = i_cal_component_get_first_property (component, I_CAL_TRANSP_PROPERTY);
	if (prop) {
		i_cal_property_set_transp (prop, value);
	} else {
		prop = i_cal_property_new_transp (value);
		i_cal_component_add_property (component, prop);
	}

	g_clear_object (&prop);
}

static void
e_comp_editor_property_part_transparency_init (ECompEditorPropertyPartTransparency *part_transparency)
{
}

static void
e_comp_editor_property_part_transparency_class_init (ECompEditorPropertyPartTransparencyClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_transparency_create_widgets;
	part_class->fill_widget = ecepp_transparency_fill_widget;
	part_class->fill_component = ecepp_transparency_fill_component;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_transparency_new (void)
{
	return g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY, NULL);
}

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_COLOR \
	(e_comp_editor_property_part_color_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_COLOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_COLOR, ECompEditorPropertyPartColor))
#define E_IS_COMP_EDITOR_PROPERTY_PART_COLOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_COLOR))

typedef struct _ECompEditorPropertyPartColor ECompEditorPropertyPartColor;
typedef struct _ECompEditorPropertyPartColorClass ECompEditorPropertyPartColorClass;

struct _ECompEditorPropertyPartColor {
	ECompEditorPropertyPart parent;

	gulong notify_current_color_id;
};

struct _ECompEditorPropertyPartColorClass {
	ECompEditorPropertyPartClass parent_class;
};

GType e_comp_editor_property_part_color_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ECompEditorPropertyPartColor, e_comp_editor_property_part_color, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static const gchar *
ecepp_color_rgba_to_string (const GdkRGBA *rgba)
{
	const struct _colors {
		const gchar *name;
		guchar rr, gg, bb;
	} colors[] = {
		{ "aliceblue",		  240, 248, 255 },
		{ "antiquewhite",	  250, 235, 215 },
		{ "aqua",		    0, 255, 255 },
		{ "aquamarine",		  127, 255, 212 },
		{ "azure",		  240, 255, 255 },
		{ "beige",		  245, 245, 220 },
		{ "bisque",		  255, 228, 196 },
		{ "black",		    0,   0,   0 },
		{ "blanchedalmond",	  255, 235, 205 },
		{ "blue",		    0,   0, 255 },
		{ "blueviolet",		  138,  43, 226 },
		{ "brown",		  165,  42,  42 },
		{ "burlywood",		  222, 184, 135 },
		{ "cadetblue",		   95, 158, 160 },
		{ "chartreuse",		  127, 255,   0 },
		{ "chocolate",		  210, 105,  30 },
		{ "coral",		  255, 127,  80 },
		{ "cornflowerblue",	  100, 149, 237 },
		{ "cornsilk",		  255, 248, 220 },
		{ "crimson",		  220,  20,  60 },
		{ "cyan",		    0, 255, 255 },
		{ "darkblue",		    0,   0, 139 },
		{ "darkcyan",		    0, 139, 139 },
		{ "darkgoldenrod",	  184, 134,  11 },
		{ "darkgray",		  169, 169, 169 },
		{ "darkgreen",		    0, 100,   0 },
		{ "darkgrey",		  169, 169, 169 },
		{ "darkkhaki",		  189, 183, 107 },
		{ "darkmagenta",	  139,   0, 139 },
		{ "darkolivegreen",	   85, 107,  47 },
		{ "darkorange",		  255, 140,   0 },
		{ "darkorchid",		  153,  50, 204 },
		{ "darkred",		  139,   0,   0 },
		{ "darksalmon",		  233, 150, 122 },
		{ "darkseagreen",	  143, 188, 143 },
		{ "darkslateblue",	   72 , 61, 139 },
		{ "darkslategray",	   47,  79,  79 },
		{ "darkslategrey",	   47,  79,  79 },
		{ "darkturquoise",	    0, 206, 209 },
		{ "darkviolet",		  148,   0, 211 },
		{ "deeppink",		  255,  20, 147 },
		{ "deepskyblue",	    0, 191, 255 },
		{ "dimgray",		  105, 105, 105 },
		{ "dimgrey",		  105, 105, 105 },
		{ "dodgerblue",		   30, 144, 255 },
		{ "firebrick",		  178,  34,  34 },
		{ "floralwhite",	  255, 250, 240 },
		{ "forestgreen",	   34, 139,  34 },
		{ "fuchsia",		  255,   0, 255 },
		{ "gainsboro",		  220, 220, 220 },
		{ "ghostwhite",		  248, 248, 255 },
		{ "gold",		  255, 215,   0 },
		{ "goldenrod",		  218, 165,  32 },
		{ "gray",		  128, 128, 128 },
		{ "green",		    0, 128,   0 },
		{ "greenyellow",	  173, 255,  47 },
		{ "grey",		  128, 128, 128 },
		{ "honeydew",		  240, 255, 240 },
		{ "hotpink",		  255, 105, 180 },
		{ "indianred",		  205,  92,  92 },
		{ "indigo",		   75,   0, 130 },
		{ "ivory",		  255, 255, 240 },
		{ "khaki",		  240, 230, 140 },
		{ "lavender",		  230, 230, 250 },
		{ "lavenderblush",	  255, 240, 245 },
		{ "lawngreen",		  124, 252,   0 },
		{ "lemonchiffon",	  255, 250, 205 },
		{ "lightblue",		  173, 216, 230 },
		{ "lightcoral",		  240, 128, 128 },
		{ "lightcyan",		  224, 255, 255 },
		{ "lightgoldenrodyellow", 250, 250, 210 },
		{ "lightgray",		  211, 211, 211 },
		{ "lightgreen",		  144, 238, 144 },
		{ "lightgrey",		  211, 211, 211 },
		{ "lightpink",		  255, 182, 193 },
		{ "lightsalmon",	  255, 160, 122 },
		{ "lightseagreen",	   32, 178, 170 },
		{ "lightskyblue",	  135, 206, 250 },
		{ "lightslategray",	  119, 136, 153 },
		{ "lightslategrey",	  119, 136, 153 },
		{ "lightsteelblue",	  176, 196, 222 },
		{ "lightyellow",	  255, 255, 224 },
		{ "lime",		    0, 255,   0 },
		{ "limegreen",		   50, 205,  50 },
		{ "linen",		  250, 240, 230 },
		{ "magenta",		  255,   0, 255 },
		{ "maroon",		  128,   0,   0 },
		{ "mediumaquamarine",	  102, 205, 170 },
		{ "mediumblue",		    0,   0, 205 },
		{ "mediumorchid",	  186,  85, 211 },
		{ "mediumpurple",	  147, 112, 219 },
		{ "mediumseagreen",	   60, 179, 113 },
		{ "mediumslateblue",	  123, 104, 238 },
		{ "mediumspringgreen",	    0, 250, 154 },
		{ "mediumturquoise",	   72, 209, 204 },
		{ "mediumvioletred",	  199,  21, 133 },
		{ "midnightblue",	   25,  25, 112 },
		{ "mintcream",		  245, 255, 250 },
		{ "mistyrose",		  255, 228, 225 },
		{ "moccasin",		  255, 228, 181 },
		{ "navajowhite",	  255, 222, 173 },
		{ "navy",		    0,   0, 128 },
		{ "oldlace",		  253, 245, 230 },
		{ "olive",		  128, 128,   0 },
		{ "olivedrab",		  107, 142,  35 },
		{ "orange",		  255, 165,   0 },
		{ "orangered",		  255,  69,   0 },
		{ "orchid",		  218, 112, 214 },
		{ "palegoldenrod",	  238, 232, 170 },
		{ "palegreen",		  152, 251, 152 },
		{ "paleturquoise",	  175, 238, 238 },
		{ "palevioletred",	  219, 112, 147 },
		{ "papayawhip",		  255, 239, 213 },
		{ "peachpuff",		  255, 218, 185 },
		{ "peru",		  205, 133, 63  },
		{ "pink",		  255, 192, 203 },
		{ "plum",		  221, 160, 221 },
		{ "powderblue",		  176, 224, 230 },
		{ "purple",		  128,   0, 128 },
		{ "red",		  255,   0,   0 },
		{ "rosybrown",		  188, 143, 143 },
		{ "royalblue",		   65, 105, 225 },
		{ "saddlebrown",	  139,  69,  19 },
		{ "salmon",		  250, 128, 114 },
		{ "sandybrown",		  244, 164,  96 },
		{ "seagreen",		   46, 139,  87 },
		{ "seashell",		  255, 245, 238 },
		{ "sienna",		  160,  82,  45 },
		{ "silver",		  192, 192, 192 },
		{ "skyblue",		  135, 206, 235 },
		{ "slateblue",		  106,  90, 205 },
		{ "slategray",		  112, 128, 144 },
		{ "slategrey",		  112, 128, 144 },
		{ "snow",		  255, 250, 250 },
		{ "springgreen",	    0, 255, 127 },
		{ "steelblue",		   70, 130, 180 },
		{ "tan",		  210, 180, 140 },
		{ "teal",		    0, 128, 128 },
		{ "thistle",		  216, 191, 216 },
		{ "tomato",		  255,  99,  71 },
		{ "turquoise",		   64, 224, 208 },
		{ "violet",		  238, 130, 238 },
		{ "wheat",		  245, 222, 179 },
		{ "white",		  255, 255, 255 },
		{ "whitesmoke",		  245, 245, 245 },
		{ "yellow",		  255, 255,   0 },
		{ "yellowgreen",	  154, 205,  50 }
	};
	guchar rr, gg, bb;
	gint best = G_MAXINT;
	const gchar *name = NULL;
	gint ii;

	g_return_val_if_fail (rgba != NULL, NULL);

	rr = 0xFF * rgba->red;
	gg = 0xFF * rgba->green;
	bb = 0xFF * rgba->blue;

	for (ii = 0; ii < G_N_ELEMENTS (colors); ii++) {
		gint delta_rr, delta_gg, delta_bb, rr_mid;
		gint dist_cc;

		delta_rr = colors[ii].rr - rr;
		delta_gg = colors[ii].gg - gg;
		delta_bb = colors[ii].bb - bb;

		/* Exact match */
		if (!delta_rr && !delta_gg && !delta_bb)
			return colors[ii].name;

		rr_mid = (colors[ii].rr + rr) / 2;

		/* Euclidean distance: https://en.wikipedia.org/wiki/Color_difference */
		dist_cc = ((2 + (rr_mid / 256.0)) * delta_rr * delta_rr) +
			  (4 * delta_gg * delta_gg) +
			  ((2 + ((255 - rr_mid) / 256.0)) * delta_bb * delta_bb);

		if (dist_cc < best) {
			best = dist_cc;
			name = colors[ii].name;
		}
	}

	return name;
}

static void
ecepp_color_notify_current_color_cb (EColorCombo *color_combo,
				     GParamSpec *param,
				     gpointer user_data)
{
	ECompEditorPropertyPartColor *color_part = user_data;
	const gchar *color_name;
	GdkRGBA rgba = { 0.0, 0.0, 0.0, 0.0 }, def_rgba = { 0.0, 0.0, 0.0, 0.0 }, parsed = {0.0, 0.0, 0.0, 0.0 };

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_COLOR (color_part));

	e_color_combo_get_current_color (color_combo, &rgba);
	e_color_combo_get_default_color (color_combo, &def_rgba);

	if (gdk_rgba_equal (&rgba, &def_rgba))
		return;

	color_name = ecepp_color_rgba_to_string (&rgba);
	if (color_name && gdk_rgba_parse (&parsed, color_name) && !gdk_rgba_equal (&rgba, &parsed)) {
		g_signal_handler_block (color_combo, color_part->notify_current_color_id);
		e_color_combo_set_current_color (color_combo, &parsed);
		g_signal_handler_unblock (color_combo, color_part->notify_current_color_id);
	}
}

static void
ecepp_color_set_palette (GtkWidget *color_combo)
{
	const gchar *colors[] = {
		"black",
		"saddlebrown",
		"rosybrown",
		"darkgreen",
		"midnightblue",
		"navy",
		"darkslateblue",
		"darkslategray",
		"maroon",

		"orangered",
		"olive",
		"green",
		"teal",
		"blue",
		"slategray",
		"gray",
		"red",

		"orange",
		"yellowgreen",
		"seagreen",
		"mediumturquoise",
		"royalblue",
		"purple",
		"lightslategray",
		"fuchsia",

		"gold",
		"yellow",
		"lime",
		"aqua",
		"deepskyblue",
		"brown",
		"silver",
		"lightpink",

		"navajowhite",
		"khaki",
		"beige",
		"lightcyan",
		"lightskyblue",
		"plum",
		"white"
	};
	GList *palette = NULL;
	gint ii;

	g_return_if_fail (E_IS_COLOR_COMBO (color_combo));

	for (ii = G_N_ELEMENTS (colors) - 1; ii >= 0 ; ii--) {
		GdkRGBA *rgba;

		rgba = g_new0 (GdkRGBA, 1);

		g_warn_if_fail (gdk_rgba_parse (rgba, colors[ii]));

		palette = g_list_prepend (palette, rgba);
	}

	e_color_combo_set_palette (E_COLOR_COMBO (color_combo), palette);

	g_list_free_full (palette, g_free);
}

static void
ecepp_color_create_widgets (ECompEditorPropertyPart *property_part,
			    GtkWidget **out_label_widget,
			    GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartColor *color_part;
	GdkRGBA rgba;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_COLOR (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	rgba.red = 0.0;
	rgba.green = 0.0;
	rgba.blue = 0.0;
	rgba.alpha = 0.001;

	*out_label_widget = NULL;

	/* Translators: This 'None' is meant for 'Color' in calendar component editor, like 'None color' */
	*out_edit_widget = e_color_combo_new_defaults (&rgba, C_("ECompEditor", "None"));

	g_object_set (G_OBJECT (*out_edit_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_edit_widget);

	g_signal_connect_swapped (*out_edit_widget, "activated",
		G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);

	ecepp_color_set_palette (*out_edit_widget);

	color_part = E_COMP_EDITOR_PROPERTY_PART_COLOR (property_part);
	color_part->notify_current_color_id =
		g_signal_connect (*out_edit_widget, "notify::current-color",
		G_CALLBACK (ecepp_color_notify_current_color_cb), property_part);
}

static void
ecepp_color_fill_widget (ECompEditorPropertyPart *property_part,
			 ICalComponent *component)
{
	GtkWidget *edit_widget;
	ICalProperty *prop;
	gboolean color_set = FALSE;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_COLOR (property_part));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (E_IS_COLOR_COMBO (edit_widget));

	prop = i_cal_component_get_first_property (component, I_CAL_COLOR_PROPERTY);
	if (prop) {
		const gchar *color = i_cal_property_get_color (prop);
		GdkRGBA rgba;

		if (color && gdk_rgba_parse (&rgba, color)) {
			e_color_combo_set_current_color (E_COLOR_COMBO (edit_widget), &rgba);
			color_set = TRUE;
		}

		g_clear_object (&prop);
	}

	if (!color_set) {
		GdkRGBA rgba;

		rgba.red = 0.0;
		rgba.green = 0.0;
		rgba.blue = 0.0;
		rgba.alpha = 0.001;

		e_color_combo_set_current_color (E_COLOR_COMBO (edit_widget), &rgba);
	}
}

static void
ecepp_color_fill_component (ECompEditorPropertyPart *property_part,
			    ICalComponent *component)
{
	GtkWidget *edit_widget;
	ICalProperty *prop;
	GdkRGBA rgba;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_COLOR (property_part));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (E_IS_COLOR_COMBO (edit_widget));

	rgba.red = 0.0;
	rgba.green = 0.0;
	rgba.blue = 0.0;
	rgba.alpha = 0.001;

	e_color_combo_get_current_color (E_COLOR_COMBO (edit_widget), &rgba);

	prop = i_cal_component_get_first_property (component, I_CAL_COLOR_PROPERTY);

	if (rgba.alpha <= 1.0 - 1e-9) {
		if (prop)
			i_cal_component_remove_property (component, prop);
	} else {
		const gchar *str;

		str = ecepp_color_rgba_to_string (&rgba);
		if (str) {
			if (prop) {
				i_cal_property_set_color (prop, str);
			} else {
				prop = i_cal_property_new_color (str);
				i_cal_component_add_property (component, prop);
			}
		} else {
			g_warning ("%s: Failed to convert RGBA (%f,%f,%f,%f) to string", G_STRFUNC, rgba.red, rgba.green, rgba.blue, rgba.alpha);
		}
	}

	g_clear_object (&prop);
}

static void
e_comp_editor_property_part_color_init (ECompEditorPropertyPartColor *part_color)
{
}

static void
e_comp_editor_property_part_color_class_init (ECompEditorPropertyPartColorClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_color_create_widgets;
	part_class->fill_widget = ecepp_color_fill_widget;
	part_class->fill_component = ecepp_color_fill_component;
}

ECompEditorPropertyPart *
e_comp_editor_property_part_color_new (void)
{
	return g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_COLOR, NULL);
}
