/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-config-field.c: One field of either the sort or group dialog.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * (C) 2000 Ximian, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gnome.h>
#include "gal/util/e-util.h"
#include "e-table-config-field.h"

#define PARENT_TYPE (gtk_vbox_get_type())

static GtkVBoxClass *etcf_parent_class;

static void
etcf_destroy (GtkObject *object)
{
	ETableConfigField *etcf = E_TABLE_CONFIG_FIELD (object);

	gtk_object_unref(GTK_OBJECT(etcf->spec));
	gtk_object_unref(GTK_OBJECT(etcf->sort_info));

	GTK_OBJECT_CLASS (etcf_parent_class)->destroy (object);
}

static void
etcf_class_init (GtkObjectClass *klass)
{
	etcf_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etcf_destroy;
}

static void
etcf_init (ETableConfigField *etcf)
{
	etcf->spec             = NULL;
	etcf->sort_info        = NULL;

	etcf->combo            = NULL;
	etcf->radio_ascending  = NULL;
	etcf->radio_descending = NULL;
	etcf->child_fields     = NULL;
}

E_MAKE_TYPE(e_table_config_field, "ETableConfigField", ETableConfigField, etcf_class_init, etcf_init, PARENT_TYPE);

ETableConfigField *
e_table_config_field_new       (ETableSpecification *spec,
				ETableSortInfo      *sort_info,
				gboolean             grouping)
{
	ETableConfigField *etcf = gtk_type_new (E_TABLE_CONFIG_FIELD_TYPE);

	e_table_config_field_construct (etcf, spec, sort_info, grouping);

	return (ETableConfigField *) etcf;
}

inline static int
etcf_get_count (ETableConfigField *etcf)
{
	if (etcf->grouping)
		return e_table_sort_info_grouping_get_count(etcf->sort_info);
	else
		return e_table_sort_info_sorting_get_count(etcf->sort_info);
}

inline static ETableSortColumn
etcf_get_nth (ETableConfigField *etcf)
{
	if (etcf->grouping)
		return e_table_sort_info_grouping_get_nth(etcf->sort_info, etcf->n);
	else
		return e_table_sort_info_sorting_get_nth(etcf->sort_info, etcf->n);
}

inline static void
etcf_set_nth (ETableConfigField *etcf, ETableSortColumn column)
{
	if (etcf->grouping)
		e_table_sort_info_grouping_set_nth(etcf->sort_info, etcf->n, column);
	else
		e_table_sort_info_sorting_set_nth(etcf->sort_info, etcf->n, column);
}

inline static void
etcf_truncate (ETableConfigField *etcf)
{
	if (etcf->grouping)
		e_table_sort_info_grouping_truncate(etcf->sort_info, etcf->n);
	else
		e_table_sort_info_sorting_truncate(etcf->sort_info, etcf->n);
}

static void
etcf_set_sensitivity(ETableConfigField *etcf)
{
	int count = etcf_get_count(etcf);

	if (etcf->n >= count) {
		gtk_widget_set_sensitive(etcf->radio_ascending, FALSE);
		gtk_widget_set_sensitive(etcf->radio_descending, FALSE);
		if (etcf->child_fields)
			gtk_widget_set_sensitive(etcf->child_fields, FALSE);
	} else {
		gtk_widget_set_sensitive(etcf->radio_ascending, TRUE);
		gtk_widget_set_sensitive(etcf->radio_descending, TRUE);
		if (etcf->child_fields)
			gtk_widget_set_sensitive(etcf->child_fields, TRUE);
	}
}

static void
toggled(GtkWidget *widget, ETableConfigField *etcf)
{
	int count;

	count = etcf_get_count(etcf);
	if (count > etcf->n) {
		ETableSortColumn sort_column;

		sort_column = etcf_get_nth(etcf);
		sort_column.ascending = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(etcf->radio_ascending));
		etcf_set_nth(etcf, sort_column);
	}
}

static void
changed(GtkWidget *widget, ETableConfigField *etcf)
{
	ETableColumnSpecification **column;
	gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(etcf->combo)->entry));
	for (column = etcf->spec->columns; *column; column++) {
		if (!strcmp((*column)->title_, text)) {
			ETableSortColumn sort_column;

			sort_column.ascending = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(etcf->radio_ascending));
			sort_column.column = (*column)->model_col;

			etcf_set_nth(etcf, sort_column);
			etcf_set_sensitivity(etcf);
			return;
		}
	}
	etcf_truncate(etcf);
	etcf_set_sensitivity(etcf);
}

static void
etcf_setup(ETableConfigField *etcf)
{
	int count;
	GList *list = NULL;
	ETableColumnSpecification **column;
	ETableColumnSpecification *chosen_column = NULL;
	int model_col = -1;

	etcf_set_sensitivity(etcf);

	count = etcf_get_count(etcf);

	if (count > etcf->n) {
		ETableSortColumn sort_column;

		sort_column = etcf_get_nth(etcf);
		model_col = sort_column.column;
		if (sort_column.ascending)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(etcf->radio_ascending), TRUE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(etcf->radio_descending), TRUE);
	}

	for (column = etcf->spec->columns; *column; column++) {
		list = g_list_prepend(list, (*column)->title_);
		if (count > etcf->n && chosen_column == NULL && (*column)->model_col == model_col) {
			chosen_column = *column;
		}
	}
	list = g_list_reverse(list);
	list = g_list_prepend(list, "None");

	gtk_combo_set_popdown_strings(GTK_COMBO(etcf->combo), list);
	g_list_free(list);

	if (chosen_column) {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(etcf->combo)->entry), chosen_column->title_);
	} else {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(etcf->combo)->entry), "None");
	}

	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(etcf->combo)->entry), "changed",
			   GTK_SIGNAL_FUNC(changed), etcf);
	gtk_signal_connect(GTK_OBJECT(etcf->radio_ascending), "toggled",
			   GTK_SIGNAL_FUNC(toggled), etcf);
	gtk_signal_connect(GTK_OBJECT(etcf->radio_descending), "toggled",
			   GTK_SIGNAL_FUNC(toggled), etcf);
}

static ETableConfigField *
e_table_config_field_construct_nth (ETableConfigField   *etcf,
				    ETableSpecification *spec,
				    ETableSortInfo      *sort_info,
				    gboolean             grouping,
				    int                  n)
{
	GtkWidget *frame;
	GtkWidget *internal_hbox;
	GtkWidget *internal_vbox1;
	GtkWidget *internal_vbox2;

	etcf->spec = spec;
	gtk_object_ref(GTK_OBJECT(spec));

	etcf->sort_info = sort_info;
	gtk_object_ref(GTK_OBJECT(sort_info));

	etcf->grouping = grouping;
	etcf->n = n;

	gtk_box_set_spacing(GTK_BOX(etcf), 6);

	frame = gtk_frame_new(n > 0 ? _("Then By") : (grouping ? _("Group By") : _("Sort By")));
	gtk_box_pack_start(GTK_BOX(etcf), frame, FALSE, FALSE, 0);

	internal_hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(frame), internal_hbox);
	gtk_container_set_border_width(GTK_CONTAINER(internal_hbox), 6);

	internal_vbox1 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(internal_hbox), internal_vbox1, FALSE, FALSE, 0);

	etcf->combo = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(internal_vbox1), etcf->combo, FALSE, FALSE, 0);

	internal_vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(internal_hbox), internal_vbox2, FALSE, FALSE, 0);

	etcf->radio_ascending = gtk_radio_button_new_with_label (NULL, _("Ascending"));
	gtk_box_pack_start(GTK_BOX(internal_vbox2), etcf->radio_ascending, FALSE, FALSE, 0);

	etcf->radio_descending = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(etcf->radio_ascending), _("Descending"));
	gtk_box_pack_start(GTK_BOX(internal_vbox2), etcf->radio_descending, FALSE, FALSE, 0);

	if (n < 3) {
		etcf->child_fields = GTK_WIDGET(gtk_type_new (E_TABLE_CONFIG_FIELD_TYPE));
		e_table_config_field_construct_nth(E_TABLE_CONFIG_FIELD(etcf->child_fields), spec, sort_info, grouping, n + 1);
		gtk_box_pack_start(GTK_BOX(etcf), etcf->child_fields, FALSE, FALSE, 0);
		gtk_widget_show(etcf->child_fields);
	} else
		etcf->child_fields = NULL;

	etcf_setup(etcf);

	gtk_widget_show(etcf->radio_descending);
	gtk_widget_show(etcf->radio_ascending);
	gtk_widget_show(internal_vbox2);
	gtk_widget_show(etcf->combo);
	gtk_widget_show(internal_vbox1);
	gtk_widget_show(internal_hbox);
	gtk_widget_show(frame);
	return etcf;
}

ETableConfigField *
e_table_config_field_construct (ETableConfigField   *etcf,
				ETableSpecification *spec,
				ETableSortInfo      *sort_info,
				gboolean             grouping)
{
	return e_table_config_field_construct_nth(etcf, spec, sort_info, grouping, 0);
}
