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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
	part_string_class->ical_prop_kind = ICAL_SUMMARY_PROPERTY;
	part_string_class->ical_new_func = icalproperty_new_summary;
	part_string_class->ical_set_func = icalproperty_set_summary;
	part_string_class->ical_get_func = icalproperty_get_summary;

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
			       icalcomponent *component)
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
	part_string_class->ical_prop_kind = ICAL_LOCATION_PROPERTY;
	part_string_class->ical_new_func = icalproperty_new_location;
	part_string_class->ical_set_func = icalproperty_set_location;
	part_string_class->ical_get_func = icalproperty_get_location;

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
}

static void
e_comp_editor_property_part_categories_class_init (ECompEditorPropertyPartCategoriesClass *klass)
{
	ECompEditorPropertyPartStringClass *part_string_class;
	ECompEditorPropertyPartClass *part_class;

	part_string_class = E_COMP_EDITOR_PROPERTY_PART_STRING_CLASS (klass);
	part_string_class->ical_prop_kind = ICAL_CATEGORIES_PROPERTY;
	part_string_class->ical_new_func = icalproperty_new_categories;
	part_string_class->ical_set_func = icalproperty_set_categories;
	part_string_class->ical_get_func = icalproperty_get_categories;

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
			       icalcomponent *component)
{
	ECompEditorPropertyPartClass *part_class;
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DESCRIPTION (property_part));
	g_return_if_fail (component != NULL);

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
	part_string_class->ical_prop_kind = ICAL_DESCRIPTION_PROPERTY;
	part_string_class->ical_new_func = icalproperty_new_description;
	part_string_class->ical_set_func = icalproperty_set_description;
	part_string_class->ical_get_func = icalproperty_get_description;

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
	part_string_class->ical_prop_kind = ICAL_URL_PROPERTY;
	part_string_class->ical_new_func = icalproperty_new_url;
	part_string_class->ical_set_func = icalproperty_set_url;
	part_string_class->ical_get_func = icalproperty_get_url;

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
	part_datetime_class->ical_prop_kind = ICAL_DTSTART_PROPERTY;
	part_datetime_class->ical_new_func = icalproperty_new_dtstart;
	part_datetime_class->ical_set_func = icalproperty_set_dtstart;
	part_datetime_class->ical_get_func = icalproperty_get_dtstart;
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
	part_datetime_class->ical_prop_kind = ICAL_DTEND_PROPERTY;
	part_datetime_class->ical_new_func = icalproperty_new_dtend;
	part_datetime_class->ical_set_func = icalproperty_set_dtend;
	part_datetime_class->ical_get_func = icalproperty_get_dtend;
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
	part_datetime_class->ical_prop_kind = ICAL_DUE_PROPERTY;
	part_datetime_class->ical_new_func = icalproperty_new_due;
	part_datetime_class->ical_set_func = icalproperty_set_due;
	part_datetime_class->ical_get_func = icalproperty_get_due;
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
e_comp_editor_property_part_completed_init (ECompEditorPropertyPartCompleted *part_completed)
{
}

static void
e_comp_editor_property_part_completed_class_init (ECompEditorPropertyPartCompletedClass *klass)
{
	ECompEditorPropertyPartDatetimeClass *part_datetime_class;

	part_datetime_class = E_COMP_EDITOR_PROPERTY_PART_DATETIME_CLASS (klass);
	part_datetime_class->ical_prop_kind = ICAL_COMPLETED_PROPERTY;
	part_datetime_class->ical_new_func = icalproperty_new_completed;
	part_datetime_class->ical_set_func = icalproperty_set_completed;
	part_datetime_class->ical_get_func = icalproperty_get_completed;
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
		{ ICAL_CLASS_PUBLIC,       NC_("ECompEditor", "Public"),       FALSE, NULL },
		{ ICAL_CLASS_PRIVATE,      NC_("ECompEditor", "Private"),      FALSE, NULL },
		{ ICAL_CLASS_CONFIDENTIAL, NC_("ECompEditor", "Confidential"), FALSE, NULL }
	};
	gint ii, n_elems = G_N_ELEMENTS (map);

	for (ii = 0; ii < n_elems; ii++) {
		map[ii].description = g_dpgettext2 (GETTEXT_PACKAGE, "ECompEditor", map[ii].description);
	}

	return e_comp_editor_property_part_picker_with_map_new (map, n_elems,
		C_("ECompEditor", "C_lassification:"),
		ICAL_CLASS_PROPERTY,
		(ECompEditorPropertyPartPickerMapICalNewFunc) icalproperty_new_class,
		(ECompEditorPropertyPartPickerMapICalSetFunc) icalproperty_set_class,
		(ECompEditorPropertyPartPickerMapICalGetFunc) icalproperty_get_class);
}

/* ************************************************************************* */

ECompEditorPropertyPart *
e_comp_editor_property_part_status_new (void)
{
	ECompEditorPropertyPartPickerMap map[] = {
		{ ICAL_STATUS_NONE,      NC_("ECompEditor", "Not Started"), TRUE,  NULL },
		{ ICAL_STATUS_INPROCESS, NC_("ECompEditor", "In Progress"), FALSE, NULL },
		{ ICAL_STATUS_COMPLETED, NC_("ECompEditor", "Completed"),   FALSE, NULL },
		{ ICAL_STATUS_CANCELLED, NC_("ECompEditor", "Canceled"),    FALSE, NULL }
	};
	gint ii, n_elems = G_N_ELEMENTS (map);

	for (ii = 0; ii < n_elems; ii++) {
		map[ii].description = g_dpgettext2 (GETTEXT_PACKAGE, "ECompEditor", map[ii].description);
	}

	return e_comp_editor_property_part_picker_with_map_new (map, n_elems,
		C_("ECompEditor", "_Status:"),
		ICAL_STATUS_PROPERTY,
		(ECompEditorPropertyPartPickerMapICalNewFunc) icalproperty_new_status,
		(ECompEditorPropertyPartPickerMapICalSetFunc) icalproperty_set_status,
		(ECompEditorPropertyPartPickerMapICalGetFunc) icalproperty_get_status);
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
		ICAL_PRIORITY_PROPERTY,
		icalproperty_new_priority,
		icalproperty_set_priority,
		icalproperty_get_priority);
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
	part_spin_class->ical_prop_kind = ICAL_PERCENTCOMPLETE_PROPERTY;
	part_spin_class->ical_new_func = icalproperty_new_percentcomplete;
	part_spin_class->ical_set_func = icalproperty_set_percentcomplete;
	part_spin_class->ical_get_func = icalproperty_get_percentcomplete;

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
			    icalcomponent *component)
{
	struct icaltimetype (* get_func) (const icalproperty *prop);
	icalproperty *prop;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TIMEZONE (property_part));

	get_func = icalproperty_get_dtstart;
	prop = icalcomponent_get_first_property (component, ICAL_DTSTART_PROPERTY);

	if (!prop) {
		get_func = icalproperty_get_dtend;
		prop = icalcomponent_get_first_property (component, ICAL_DTEND_PROPERTY);
	}

	if (!prop) {
		get_func = icalproperty_get_due;
		prop = icalcomponent_get_first_property (component, ICAL_DUE_PROPERTY);
	}

	if (prop) {
		struct icaltimetype itt;

		itt = get_func (prop);
		if (itt.zone) {
			GtkWidget *edit_widget;

			edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
			g_return_if_fail (E_IS_TIMEZONE_ENTRY (edit_widget));

			e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (edit_widget), (icaltimezone *) itt.zone);
		}
	}
}

static void
ecepp_timezone_fill_component (ECompEditorPropertyPart *property_part,
			       icalcomponent *component)
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
				icalcomponent *component)
{
	GtkWidget *edit_widget;
	icalproperty *prop;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY (property_part));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_CHECK_BUTTON (edit_widget));

	prop = icalcomponent_get_first_property (component, ICAL_TRANSP_PROPERTY);
	if (prop) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edit_widget),
			icalproperty_get_transp (prop) == ICAL_TRANSP_OPAQUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edit_widget), TRUE);
	}
}

static void
ecepp_transparency_fill_component (ECompEditorPropertyPart *property_part,
				   icalcomponent *component)
{
	GtkWidget *edit_widget;
	icalproperty *prop;
	icalproperty_transp value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_TRANSPARENCY (property_part));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_CHECK_BUTTON (edit_widget));

	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (edit_widget)) ? ICAL_TRANSP_OPAQUE : ICAL_TRANSP_TRANSPARENT;

	prop = icalcomponent_get_first_property (component, ICAL_TRANSP_PROPERTY);
	if (prop) {
		icalproperty_set_transp (prop, value);
	} else {
		prop = icalproperty_new_transp (value);
		icalcomponent_add_property (component, prop);
	}
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
