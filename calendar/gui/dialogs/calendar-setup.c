/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2004 Novell, Inc (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>

#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/libgnomeui.h>

#include "calendar-setup.h"
#include "../e-cal-config.h"

typedef struct _CalendarSourceDialog CalendarSourceDialog;

struct _CalendarSourceDialog {
	ECalConfig *config;	/* the config manager */

	GtkWidget *window;

	/* Source selection (creation only) */
	ESourceList *source_list;
	GSList *menu_source_groups;
	GtkWidget *group_optionmenu;

	/* ESource we're currently editing */
	ESource *source;
	/* The original source in edit mode.  Also used to flag when we are in edit mode. */
	ESource *original_source;

	/* Source group we're creating/editing a source in */
	ESourceGroup *source_group;
};

static gboolean
eccp_check_complete (EConfig *ec, const char *pageid, void *data)
{
	CalendarSourceDialog *sdialog = data;
	gboolean valid = TRUE;
	const char *tmp;
	ESource *source;

	tmp = e_source_peek_name (sdialog->source);
	valid = tmp && tmp[0] && ((source = e_source_group_peek_source_by_name (sdialog->source_group, tmp)) == NULL || source == sdialog->original_source);

	return valid;
}

static void
colorpicker_set_color (GnomeColorPicker *color, guint32 rgb)
{
	gnome_color_picker_set_i8 (color, (rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff, 0xff);
}

static guint32
colorpicker_get_color (GnomeColorPicker *color)
{
	guint8 r, g, b, a;
	guint32 rgb = 0;

	gnome_color_picker_get_i8 (color, &r, &g, &b, &a);

	rgb = r;
	rgb <<= 8;
	rgb |= g;
	rgb <<= 8;
	rgb |= b;

	return rgb;
}

static void
eccp_commit (EConfig *ec, GSList *items, void *data)
{
	CalendarSourceDialog *sdialog = data;
	xmlNodePtr xml;

	if (sdialog->original_source) {
		guint32 color;

		xml = xmlNewNode (NULL, "dummy");
		e_source_dump_to_xml_node (sdialog->source, xml);
		e_source_update_from_xml_node (sdialog->original_source, xml->children, NULL);
		xmlFreeNode (xml);

		e_source_get_color (sdialog->source, &color);
		e_source_set_color (sdialog->original_source, color);
	} else {
		e_source_group_add_source (sdialog->source_group, sdialog->source, -1);
		e_source_list_sync (sdialog->source_list, NULL);
	}
}

static void
eccp_free (EConfig *ec, GSList *items, void *data)
{
	CalendarSourceDialog *sdialog = data;

	g_slist_free (items);

	g_object_unref (sdialog->source);
	if (sdialog->original_source)
		g_object_unref (sdialog->original_source);
	if (sdialog->source_list)
		g_object_unref (sdialog->source_list);
	g_slist_free (sdialog->menu_source_groups);
	g_free (sdialog);
}

static void
eccp_type_changed (GtkComboBox *dropdown, CalendarSourceDialog *sdialog)
{
	int id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (dropdown);
	if (id == -1 || !gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
		return;

	/* TODO: when we change the group type, we lose all of the pre-filled dialog info */

	gtk_tree_model_get (model, &iter, 1, &sdialog->source_group, -1);
	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri (sdialog->source, NULL);
	e_source_set_group (sdialog->source, sdialog->source_group);

	e_source_set_relative_uri (sdialog->source, "");

	e_config_target_changed ((EConfig *) sdialog->config, E_CONFIG_TARGET_CHANGED_REBUILD);
}

static GtkWidget *
eccp_get_source_type (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, void *data)
{
	static GtkWidget *label, *type;
	int row;
	CalendarSourceDialog *sdialog = data;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) ec->target;
	ESource *source = t->source;
	ESourceGroup *group = e_source_peek_group (source);
	char *markup;

	if (old)
		gtk_widget_destroy (label);

	row = ((GtkTable *)parent)->nrows;

	if (sdialog->original_source) {
		label = gtk_label_new (_("Type:"));

		type = gtk_label_new ("");
		gtk_widget_show (type);
		markup = g_strdup_printf ("<span weight=\"bold\">%s</span>", e_source_group_peek_name (group));
		gtk_label_set_markup (GTK_LABEL (type), markup);
		gtk_misc_set_alignment (GTK_MISC (type), 0.0, 0.5);
		g_free (markup);
		gtk_table_attach (GTK_TABLE (parent), type, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	} else {
		GtkCellRenderer *cell;
		GtkListStore *store;
		GtkTreeIter iter;
		GSList *l;
		int active = 0, i = 0;

		label = gtk_label_new_with_mnemonic(_("_Type:"));

		type = gtk_combo_box_new ();
		cell = gtk_cell_renderer_text_new ();
		store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
		for (l = sdialog->menu_source_groups; l; l = g_slist_next (l)) {
			ESourceGroup *group = l->data;

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 0, e_source_group_peek_name (group), 1, group, -1);
			if (!strcmp (e_source_group_peek_uid (sdialog->source_group), e_source_group_peek_uid (group)))
				active = i;
			i++;
		}

		gtk_cell_layout_pack_start ((GtkCellLayout *) type, cell, TRUE);
		gtk_cell_layout_set_attributes ((GtkCellLayout *) type, cell, "text", 0, NULL);
		gtk_combo_box_set_model ((GtkComboBox *) type, (GtkTreeModel *) store);
		gtk_combo_box_set_active ((GtkComboBox *) type, active);
		g_signal_connect (type, "changed", G_CALLBACK (eccp_type_changed), sdialog);
		gtk_widget_show (type);
		gtk_table_attach (GTK_TABLE (parent), type, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), type);
	}

	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	return type;
}

static void
name_changed (GtkEntry *entry, ECalConfigTargetSource *t)
{
	ESource *source = t->source;
	e_source_set_name (source, gtk_entry_get_text (GTK_ENTRY (entry)));
}

static GtkWidget *
eccp_get_source_name (EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	static GtkWidget *label, *entry;
	int row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) ec->target;
	ESource *source = t->source;

	if (old)
		gtk_widget_destroy (label);

	row = ((GtkTable*)parent)->nrows;

	label = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_table_attach (GTK_TABLE (parent), entry, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (name_changed), (gpointer) t);

	if (source)
		gtk_entry_set_text (GTK_ENTRY (entry), e_source_peek_name (source));

	return entry;
}

static void
color_changed (GnomeColorPicker *picker, guint r, guint g, guint b, guint a, ECalConfigTargetSource *t)
{
	ESource *source = t->source;
	e_source_set_color (source, colorpicker_get_color (picker));
}

static GtkWidget *
eccp_get_source_color (EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	CalendarSourceDialog *sdialog = data;
	static GtkWidget *label, *picker;
	int row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) ec->target;
	ESource *source = t->source;

	static guint32 assigned_colors[] = {
		0xBECEDD, /* 190 206 221     Blue */
		0xE2F0EF, /* 226 240 239     Light Blue */
		0xC6E2B7, /* 198 226 183     Green */
		0xE2F0D3, /* 226 240 211     Light Green */
		0xE2D4B7, /* 226 212 183     Khaki */
		0xEAEAC1, /* 234 234 193     Light Khaki */
		0xF0B8B7, /* 240 184 183     Pink */
		0xFED4D3, /* 254 212 211     Light Pink */
		0xE2C6E1, /* 226 198 225     Purple */
		0xF0E2EF  /* 240 226 239     Light Purple */
	};
	GRand *rand = g_rand_new ();
	guint32 color;

	if (old)
		gtk_widget_destroy (label);

	row = ((GtkTable*)parent)->nrows;

	color = assigned_colors[g_rand_int_range (rand, 0, 9)];
	g_rand_free (rand);

	label = gtk_label_new_with_mnemonic (_("C_olor:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	picker = gnome_color_picker_new ();
	gtk_widget_show (picker);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), picker);
	gtk_table_attach (GTK_TABLE (parent), picker, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	g_signal_connect (G_OBJECT (picker), "color-set", G_CALLBACK (color_changed), t);

	if (sdialog->original_source)
		e_source_get_color (sdialog->original_source, &color);
	else
		/* since we don't have an original source here, we want to set
		 * the initial color */
		e_source_set_color (sdialog->source, color);

	colorpicker_set_color (GNOME_COLOR_PICKER (picker), color);

	return picker;
}

static ECalConfigItem eccp_items[] = {
	{ E_CONFIG_BOOK, "", NULL },
	{ E_CONFIG_PAGE,          "00.general", N_("General") },
	{ E_CONFIG_SECTION_TABLE, "00.general/00.source", N_("Calendar") },
	{ E_CONFIG_ITEM_TABLE,    "00.general/00.source/00.type", NULL, eccp_get_source_type },
	{ E_CONFIG_ITEM_TABLE,    "00.general/00.source/10.name", NULL, eccp_get_source_name },
	{ E_CONFIG_ITEM_TABLE,    "00.general/00.source/20.color", NULL, eccp_get_source_color },
	{ 0 },
};

static ECalConfigItem ectp_items[] = {
	{ E_CONFIG_BOOK, "", NULL },
	{ E_CONFIG_PAGE,          "00.general", N_("General") },
	{ E_CONFIG_SECTION_TABLE, "00.general/00.source", N_("Tasks List") },
	{ E_CONFIG_ITEM_TABLE,    "00.general/00.source/00.type", NULL, eccp_get_source_type },
	{ E_CONFIG_ITEM_TABLE,    "00.general/00.source/10.name", NULL, eccp_get_source_name },
	{ E_CONFIG_ITEM_TABLE,    "00.general/00.source/20.color", NULL, eccp_get_source_color },
	{ 0 },
};

/**
 * calendar_setup_edit_calendar:
 * @parent: parent window for dialog (current unused)
 * @source: the ESource corresponding to the calendar
 *
 * Show calendar properties for @source.
 **/
void
calendar_setup_edit_calendar (struct _GtkWindow *parent, ESource *source, ESourceGroup *group)
{
	CalendarSourceDialog *sdialog = g_new0 (CalendarSourceDialog, 1);
	char *xml;
	ECalConfig *ec;
	int i;
	GSList *items = NULL;
	ECalConfigTargetSource *target;

	if (source) {
		guint32 color;

		sdialog->original_source = source;
		g_object_ref (source);
		sdialog->source_group = e_source_peek_group (source);
		xml = e_source_to_standalone_xml (source);
		sdialog->source = e_source_new_from_standalone_xml (xml);
		g_free (xml);

		e_source_get_color (source, &color);
		e_source_set_color (sdialog->source, color);
	} else {
		GConfClient *gconf;
		GSList *l;

		sdialog->source = e_source_new ("", "");
		gconf = gconf_client_get_default ();
		sdialog->source_list = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
		l = e_source_list_peek_groups (sdialog->source_list);
		sdialog->menu_source_groups = g_slist_copy(l);

		sdialog->source_group = (ESourceGroup *)sdialog->menu_source_groups->data;
		g_object_unref (gconf);
		if (group)
			sdialog->source_group = (ESourceGroup *)group;
	}

	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri (sdialog->source, NULL);
	e_source_set_group (sdialog->source, sdialog->source_group);

	sdialog->config = ec = e_cal_config_new (E_CONFIG_BOOK, "org.gnome.evolution.calendar.calendarProperties");
	for (i = 0; eccp_items[i].path; i++)
		items = g_slist_prepend (items, &eccp_items[i]);
	e_config_add_items ((EConfig *) ec, items, eccp_commit, NULL, eccp_free, sdialog);
	e_config_add_page_check ((EConfig *) ec, NULL, eccp_check_complete, sdialog);

	target = e_cal_config_target_new_source (ec, sdialog->source);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);

	if (source)
		sdialog->window = e_config_create_window ((EConfig *)ec, NULL, _("Calendar Properties"));
	else
		sdialog->window = e_config_create_window ((EConfig *)ec, NULL, _("New Calendar"));
		
	/* forces initial validation */
	if (!sdialog->original_source)
		e_config_target_changed ((EConfig *)ec, E_CONFIG_TARGET_CHANGED_STATE);

	return;
}

void
calendar_setup_new_calendar (struct _GtkWindow *parent)
{
	calendar_setup_edit_calendar (parent, NULL, NULL);
}

void
calendar_setup_edit_task_list (struct _GtkWindow *parent, ESource *source)
{
	CalendarSourceDialog *sdialog = g_new0 (CalendarSourceDialog, 1);
	char *xml;
	ECalConfig *ec;
	int i;
	GSList *items = NULL;
	ECalConfigTargetSource *target;

	if (source) {
		guint32 color;

		sdialog->original_source = source;
		g_object_ref (source);
		sdialog->source_group = e_source_peek_group (source);
		xml = e_source_to_standalone_xml (source);
		sdialog->source = e_source_new_from_standalone_xml (xml);
		g_free (xml);

		e_source_get_color (source, &color);
		e_source_set_color (sdialog->source, color);
	} else {
		GConfClient *gconf;
		GSList *l;

		sdialog->source = e_source_new ("", "");
		gconf = gconf_client_get_default ();
		sdialog->source_list = e_source_list_new_for_gconf (gconf, "/apps/evolution/tasks/sources");
		l = e_source_list_peek_groups (sdialog->source_list);
		sdialog->menu_source_groups = g_slist_copy(l);

		sdialog->source_group = (ESourceGroup *)sdialog->menu_source_groups->data;
		g_object_unref (gconf);
	}

	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri (sdialog->source, NULL);
	e_source_set_group (sdialog->source, sdialog->source_group);

	sdialog->config = ec = e_cal_config_new (E_CONFIG_BOOK, "org.gnome.evolution.calendar.calendarProperties");
	for (i = 0; ectp_items[i].path; i++)
		items = g_slist_prepend (items, &ectp_items[i]);
	e_config_add_items ((EConfig *) ec, items, eccp_commit, NULL, eccp_free, sdialog);
	e_config_add_page_check ((EConfig *) ec, NULL, eccp_check_complete, sdialog);

	target = e_cal_config_target_new_source (ec, sdialog->source);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);

	sdialog->window = e_config_create_window ((EConfig *)ec, NULL, _("Task List Properties"));

	/* forces initial validation */
	if (!sdialog->original_source)
		e_config_target_changed ((EConfig *)ec, E_CONFIG_TARGET_CHANGED_STATE);

	return;
}

void
calendar_setup_new_task_list (struct _GtkWindow *parent)
{
	calendar_setup_edit_task_list (parent, NULL);
}
