/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-config.c: The ETable config dialog.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <stdlib.h>
#include <gnome.h>
#include "e-table-config.h"
#include "e-table-config-field.h"
#include "gal/util/e-util.h"

#define PARENT_TYPE (gnome_dialog_get_type())

static GnomeDialogClass *etco_parent_class;

static void
etco_destroy (GtkObject *object)
{
	ETableConfig *etco = E_TABLE_CONFIG (object);

	if (etco->state) {
		if (etco->sorting_changed_id)
			gtk_signal_disconnect(GTK_OBJECT(etco->state->sort_info), etco->sorting_changed_id);
		if (etco->grouping_changed_id)
			gtk_signal_disconnect(GTK_OBJECT(etco->state->sort_info), etco->grouping_changed_id);
		gtk_object_unref(GTK_OBJECT(etco->state));
	}

	gtk_object_unref(GTK_OBJECT(etco->spec));

	GTK_OBJECT_CLASS (etco_parent_class)->destroy (object);
}

static void
etco_class_init (GtkObjectClass *klass)
{
	etco_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etco_destroy;
}

static void
etco_clear_sort(GtkWidget *widget, ETableConfig *etco)
{
	etco->sort_dialog = NULL;
	gtk_object_unref(GTK_OBJECT(etco));
}

static void
etco_clear_group(GtkWidget *widget, ETableConfig *etco)
{
	etco->group_dialog = NULL;
	gtk_object_unref(GTK_OBJECT(etco));
}

static void
etco_sort_config_show(GtkWidget *widget, ETableConfig *etco)
{
	if (etco->sort_dialog)
		gdk_window_raise(GTK_WIDGET(etco->sort_dialog)->window);
	else {
		GtkWidget *etcf;
		etco->sort_dialog = gnome_dialog_new(_("Sort"),
						     GNOME_STOCK_BUTTON_OK,
						     NULL);
		etcf = GTK_WIDGET(e_table_config_field_new(etco->spec,
							   etco->state->sort_info,
							   FALSE));
		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(etco->sort_dialog)->vbox), etcf, FALSE, FALSE, 0);
		gnome_dialog_set_parent(GNOME_DIALOG(etco->sort_dialog),
					GTK_WINDOW(etco));

		gtk_signal_connect(GTK_OBJECT(etco->sort_dialog), "destroy",
				   GTK_SIGNAL_FUNC(etco_clear_sort), etco);
		gtk_object_ref(GTK_OBJECT(etco));

		gtk_signal_connect(GTK_OBJECT(etco->sort_dialog), "clicked",
				   GTK_SIGNAL_FUNC(gnome_dialog_close), etco);

		gtk_widget_show(GTK_WIDGET(etcf));
		gtk_widget_show(GTK_WIDGET(etco->sort_dialog));
	}
}

static void
etco_group_config_show(GtkWidget *widget, ETableConfig *etco)
{
	if (etco->group_dialog)
		gdk_window_raise(GTK_WIDGET(etco->group_dialog)->window);
	else {
		GtkWidget *etcf;
		etco->group_dialog = gnome_dialog_new(_("Group"),
						      GNOME_STOCK_BUTTON_OK,
						      NULL);
		etcf = GTK_WIDGET(e_table_config_field_new(etco->spec,
							   etco->state->sort_info,
							   TRUE));
		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(etco->group_dialog)->vbox), etcf, FALSE, FALSE, 0);
		gnome_dialog_set_parent(GNOME_DIALOG(etco->group_dialog),
					GTK_WINDOW(etco));

		gtk_signal_connect(GTK_OBJECT(etco->group_dialog), "destroy",
				   GTK_SIGNAL_FUNC(etco_clear_group), etco);
		gtk_signal_connect(GTK_OBJECT(etco->group_dialog), "clicked",
				   GTK_SIGNAL_FUNC(gnome_dialog_close), etco);
		gtk_object_ref(GTK_OBJECT(etco));

		gtk_widget_show(GTK_WIDGET(etcf));
		gtk_widget_show(GTK_WIDGET(etco->group_dialog));
	}
}

static void
etco_sort_info_update(ETableSortInfo *info, ETableConfig *etco)
{
	int count;
	int i;
	gchar **strings;
	gchar *substrings[3];
	int stringcount = 0;
	gchar *string;

	count = e_table_sort_info_sorting_get_count(info);
	strings = g_new(gchar *, count + 1);

	for (i = 0; i < count; i++) {
		ETableSortColumn col = e_table_sort_info_sorting_get_nth(info, i);
		ETableColumnSpecification **column;

		substrings[0] = NULL;

		for (column = etco->spec->columns; *column; column++) {
			if (col.column == (*column)->model_col) {
				substrings[0] = (*column)->title_;
				break;
			}
		}

		if (substrings[0]) {
			substrings[1] = col.ascending ? _("(Ascending)") : _("(Descending)");
			substrings[2] = NULL;
			strings[stringcount++] = g_strjoinv(" ", substrings);
		}
	}
	strings[stringcount] = NULL;
	string = g_strjoinv(", ", strings);

	for (i = 0; strings[i]; i++) {
		g_free(strings[i]);
	}
	gtk_label_set_text(GTK_LABEL(etco->sort_label), string);
	g_free(string);

}

static void
etco_group_info_update(ETableSortInfo *info, ETableConfig *etco)
{
	int count;
	int i;
	gchar **strings;
	gchar *substrings[3];
	int stringcount = 0;
	gchar *string;

	count = e_table_sort_info_grouping_get_count(info);
	strings = g_new(gchar *, count + 1);

	for (i = 0; i < count; i++) {
		ETableSortColumn col = e_table_sort_info_grouping_get_nth(info, i);
		ETableColumnSpecification **column;

		substrings[0] = NULL;

		for (column = etco->spec->columns; *column; column++) {
			if (col.column == (*column)->model_col) {
				substrings[0] = (*column)->title_;
				break;
			}
		}

		if (substrings[0]) {
			substrings[1] = col.ascending ? _("(Ascending)") : _("(Descending)");
			substrings[2] = NULL;
			strings[stringcount++] = g_strjoinv(" ", substrings);
		}
	}
	strings[stringcount] = NULL;
	string = g_strjoinv(", ", strings);

	for (i = 0; strings[i]; i++) {
		g_free(strings[i]);
	}
	gtk_label_set_text(GTK_LABEL(etco->group_label), string);
	g_free(string);

}

static void
etco_init (ETableConfig *etco)
{
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *sort_button;
	GtkWidget *group_button;

	gtk_window_set_title(GTK_WINDOW(etco), _("View Summary"));
	gnome_dialog_append_buttons(GNOME_DIALOG(etco),
				    GNOME_STOCK_BUTTON_OK,
				    NULL);
	gnome_dialog_set_default(GNOME_DIALOG(etco), 0);

	frame = gtk_frame_new(_("Description"));
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(etco)->vbox), frame, FALSE, FALSE, 0);

	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 6);

	sort_button = gtk_button_new_with_label(_("Sort..."));
	gtk_table_attach(GTK_TABLE(table),
			 sort_button,
			 0, 1, 0, 1,
			 GTK_FILL,
			 GTK_FILL,
			 0, 0);

	group_button = gtk_button_new_with_label(_("Group By..."));
	gtk_table_attach(GTK_TABLE(table),
			 group_button,
			 0, 1, 1, 2,
			 GTK_FILL,
			 GTK_FILL,
			 0, 0);

	etco->sort_label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table),
			 etco->sort_label,
			 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND,
			 GTK_FILL,
			 0, 0);

	etco->group_label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table),
			 etco->group_label,
			 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND,
			 GTK_FILL,
			 0, 0);

	gtk_signal_connect(GTK_OBJECT(sort_button), "clicked",
			   GTK_SIGNAL_FUNC(etco_sort_config_show), etco);
	gtk_signal_connect(GTK_OBJECT(group_button), "clicked",
			   GTK_SIGNAL_FUNC(etco_group_config_show), etco);

	gtk_widget_show(etco->group_label);
	gtk_widget_show(etco->sort_label);
	gtk_widget_show(group_button);
	gtk_widget_show(sort_button);
	gtk_widget_show(table);
	gtk_widget_show(frame);

	etco->sorting_changed_id = 0;
	etco->grouping_changed_id = 0;
}

E_MAKE_TYPE(e_table_config, "ETableConfig", ETableConfig, etco_class_init, etco_init, PARENT_TYPE);

GtkWidget *
e_table_config_new       (ETableSpecification *spec,
			  ETableState         *state)
{
	ETableConfig *etco = gtk_type_new (E_TABLE_CONFIG_TYPE);

	e_table_config_construct(etco, spec, state);

	return GTK_WIDGET(etco);
}

GtkWidget *
e_table_config_construct (ETableConfig        *etco,
			  ETableSpecification *spec,
			  ETableState         *state)
{
	etco->spec = spec;
	etco->state = state;

	if (etco->spec)
		gtk_object_ref(GTK_OBJECT(etco->spec));
	if (etco->state)
		gtk_object_ref(GTK_OBJECT(etco->state));

	etco->sorting_changed_id = gtk_signal_connect(GTK_OBJECT(etco->state->sort_info), "sort_info_changed",
						      GTK_SIGNAL_FUNC(etco_sort_info_update), etco);
	etco->grouping_changed_id = gtk_signal_connect(GTK_OBJECT(etco->state->sort_info), "group_info_changed",
						       GTK_SIGNAL_FUNC(etco_group_info_update), etco);

	etco_sort_info_update(etco->state->sort_info, etco);
	etco_group_info_update(etco->state->sort_info, etco);

	return GTK_WIDGET(etco);
}

