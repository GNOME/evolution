/*
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
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>

#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>
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
	ECalSourceType source_type;
};

static gboolean
eccp_check_complete (EConfig *ec, const gchar *pageid, gpointer data)
{
	CalendarSourceDialog *sdialog = data;
	gboolean valid = TRUE;
	const gchar *tmp;
	ESource *source;

	tmp = e_source_peek_name (sdialog->source);
	valid = tmp && tmp[0] && ((source = e_source_group_peek_source_by_name (sdialog->source_group, tmp)) == NULL || source == sdialog->original_source);

	return valid;
}

static void
eccp_commit (EConfig *ec, GSList *items, gpointer data)
{
	CalendarSourceDialog *sdialog = data;
	xmlNodePtr xml;

	if (sdialog->original_source) {
		const gchar *color_spec;

		xml = xmlNewNode (NULL, (const guchar *)"dummy");
		e_source_dump_to_xml_node (sdialog->source, xml);
		e_source_update_from_xml_node (sdialog->original_source, xml->children, NULL);
		xmlFreeNode (xml);

		color_spec = e_source_peek_color_spec (sdialog->source);
		if (color_spec != NULL)
			e_source_set_color_spec (sdialog->original_source, color_spec);
	} else {
		e_source_group_add_source (sdialog->source_group, sdialog->source, -1);
		e_source_list_sync (sdialog->source_list, NULL);
	}
}

static void
eccp_free (EConfig *ec, GSList *items, gpointer data)
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
	gint id = gtk_combo_box_get_active (dropdown);
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
eccp_get_source_type (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	static GtkWidget *label, *type;
	guint row;
	CalendarSourceDialog *sdialog = data;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) ec->target;
	ESource *source = t->source;
	ESourceGroup *group = e_source_peek_group (source);
	gchar *markup;

	if (old)
		gtk_widget_destroy (label);

	g_object_get (parent, "n-rows", &row, NULL);

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
		gint active = 0, i = 0;

		label = gtk_label_new_with_mnemonic(_("_Type:"));

		type = gtk_combo_box_new ();
		cell = gtk_cell_renderer_text_new ();
		store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
		for (l = sdialog->menu_source_groups; l; l = g_slist_next (l)) {
			/* Reuse previously defined *group here? */
			ESourceGroup *group = l->data;
			gchar *create_source = e_source_group_get_property (group, "create_source");

			if ( !(create_source && !strcmp (create_source, "no"))) {
				gtk_list_store_append (store, &iter);
				gtk_list_store_set (store, &iter, 0, e_source_group_peek_name (group), 1, group, -1);
				if (!strcmp (e_source_group_peek_uid (sdialog->source_group), e_source_group_peek_uid (group)))
					active = i;
				i++;
			}
			g_free (create_source);
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
eccp_get_source_name (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	static GtkWidget *label, *entry;
	guint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) ec->target;
	ESource *source = t->source;

	if (old)
		gtk_widget_destroy (label);

	g_object_get (parent, "n-rows", &row, NULL);

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
offline_status_changed_cb (GtkWidget *widget, CalendarSourceDialog *sdialog)
{

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		e_source_set_property (sdialog->source, "offline_sync", "1");
	else
		e_source_set_property (sdialog->source, "offline_sync", "0");

}

static GtkWidget *
eccp_general_offline (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	CalendarSourceDialog *sdialog = data;
	GtkWidget *offline_setting = NULL;
	const gchar *offline_sync;
	guint row;
	const gchar *base_uri = e_source_group_peek_base_uri (sdialog->source_group);
	gboolean is_local = base_uri && (g_str_has_prefix (base_uri, "local:") || g_str_has_prefix (base_uri, "contacts://"));
	offline_sync =  e_source_get_property (sdialog->source, "offline_sync");
	if (old)
		return old;
	else {
		g_object_get (parent, "n-rows", &row, NULL);

		if (sdialog->source_type == E_CAL_SOURCE_TYPE_EVENT)
			offline_setting = gtk_check_button_new_with_mnemonic (_("Cop_y calendar contents locally for offline operation"));
		else if (sdialog->source_type == E_CAL_SOURCE_TYPE_TODO)
			offline_setting = gtk_check_button_new_with_mnemonic (_("Cop_y task list contents locally for offline operation"));
		else if (sdialog->source_type == E_CAL_SOURCE_TYPE_JOURNAL)
			offline_setting = gtk_check_button_new_with_mnemonic (_("Cop_y memo list contents locally for offline operation"));

		gtk_widget_show (offline_setting);
		g_signal_connect (offline_setting, "toggled", G_CALLBACK (offline_status_changed_cb), sdialog);
		gtk_table_attach (GTK_TABLE (parent), offline_setting, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (offline_setting), (offline_sync && g_str_equal (offline_sync, "1"))  ? TRUE : FALSE);
	if (is_local)
	  gtk_widget_hide (offline_setting);
	return offline_setting;
}

static void
color_changed (GtkColorButton *color_button, ECalConfigTargetSource *target)
{
	ESource *source = target->source;
	gchar color_spec[16];
	GdkColor color;

	gtk_color_button_get_color (color_button, &color);
	g_snprintf (color_spec, sizeof (color_spec), "#%04x%04x%04x",
		color.red, color.green, color.blue);
	e_source_set_color_spec (source, color_spec);
}

static const gchar *
choose_initial_color (void)
{
	static const gchar *colors[] = {
		"#BECEDD", /* 190 206 221     Blue */
		"#E2F0EF", /* 226 240 239     Light Blue */
		"#C6E2B7", /* 198 226 183     Green */
		"#E2F0D3", /* 226 240 211     Light Green */
		"#E2D4B7", /* 226 212 183     Khaki */
		"#EAEAC1", /* 234 234 193     Light Khaki */
		"#F0B8B7", /* 240 184 183     Pink */
		"#FED4D3", /* 254 212 211     Light Pink */
		"#E2C6E1", /* 226 198 225     Purple */
		"#F0E2EF"  /* 240 226 239     Light Purple */
	};

	return colors[g_random_int_range (0, G_N_ELEMENTS (colors))];
}

static GtkWidget *
eccp_get_source_color (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	CalendarSourceDialog *sdialog = data;
	static GtkWidget *label, *color_button;
	guint row;
	const gchar *color_spec = NULL;
	GdkColor color;

	g_object_get (parent, "n-rows", &row, NULL);

	if (old)
		gtk_widget_destroy (label);

	if (sdialog->original_source)
		color_spec = e_source_peek_color_spec (sdialog->original_source);

	if (color_spec == NULL) {
		color_spec = choose_initial_color ();
		e_source_set_color_spec (sdialog->source, color_spec);
	}

	if (!gdk_color_parse (color_spec, &color))
		g_warning ("Unknown color \"%s\" in calendar \"%s\"",
			color_spec, e_source_peek_name (sdialog->source));

	label = gtk_label_new_with_mnemonic (_("Colo_r:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (
		GTK_TABLE (parent), label,
		0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);

	color_button = gtk_color_button_new_with_color (&color);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), color_button);
	gtk_table_attach (
		GTK_TABLE (parent), color_button,
		1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show (color_button);

	g_signal_connect (
		G_OBJECT (color_button), "color-set",
		G_CALLBACK (color_changed), ec->target);

	return color_button;
}

static ECalConfigItem eccp_items[] = {
	{ E_CONFIG_BOOK,          (gchar *) "", NULL },
	{ E_CONFIG_PAGE,          (gchar *) "00.general", (gchar *) N_("General") },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.general/00.source", (gchar *) N_("Calendar") },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/00.type", NULL, eccp_get_source_type },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/10.name", NULL, eccp_get_source_name },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/20.color", NULL, eccp_get_source_color },
	{ E_CONFIG_ITEM_TABLE,	  (gchar *) "00.general/00.source/30.offline", NULL, eccp_general_offline },
	{ 0 },
};

static ECalConfigItem ectp_items[] = {
	{ E_CONFIG_BOOK,          (gchar *) "", NULL },
	{ E_CONFIG_PAGE,          (gchar *) "00.general", (gchar *) N_("General") },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.general/00.source", (gchar *) N_("Task List") },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/00.type", NULL, eccp_get_source_type },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/10.name", NULL, eccp_get_source_name },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/20.color", NULL, eccp_get_source_color },
	{ E_CONFIG_ITEM_TABLE,	  (gchar *) "00.general/00.source/30.offline", NULL, eccp_general_offline },
	{ 0 },
};

static ECalConfigItem ecmp_items[] = {
	{ E_CONFIG_BOOK,          (gchar *) "", NULL },
	{ E_CONFIG_PAGE,          (gchar *) "00.general", (gchar *) N_("General") },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.general/00.source", (gchar *) N_("Memo List") },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/00.type", NULL, eccp_get_source_type },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/10.name", NULL, eccp_get_source_name },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/20.color", NULL, eccp_get_source_color },
	{ E_CONFIG_ITEM_TABLE,    (gchar *) "00.general/00.source/30.offline", NULL, eccp_general_offline },
	{ 0 },
};

/**
 * cs_load_sources:
 * @sdialog: dialog where to load sources list
 * @conf_key: configuration key where to get sources' list
 * @group: can be NULL
 *
 * Loads list of sources from @conf_key.
 */

static void
cs_load_sources (CalendarSourceDialog *sdialog, const gchar *conf_key, ESourceGroup *group)
{
	GConfClient *gconf;

	g_return_if_fail (sdialog != NULL && conf_key != NULL);

	sdialog->source = e_source_new ("", "");
	gconf = gconf_client_get_default ();
	sdialog->source_list = e_source_list_new_for_gconf (gconf, conf_key);
	sdialog->menu_source_groups = g_slist_copy (e_source_list_peek_groups (sdialog->source_list));
	sdialog->source_group = (ESourceGroup *)sdialog->menu_source_groups->data;

	g_object_unref (gconf);

	if (group)
		sdialog->source_group = (ESourceGroup *)group;
}

/**
 * calendar_setup_edit_calendar:
 * @parent: parent window for dialog (current unused)
 * @source: the ESource corresponding to the calendar
 *
 * Show calendar properties for @source.
 **/
void
calendar_setup_edit_calendar (GtkWindow *parent, ESource *source, ESourceGroup *group)
{
	CalendarSourceDialog *sdialog = g_new0 (CalendarSourceDialog, 1);
	gchar *xml;
	ECalConfig *ec;
	gint i;
	GSList *items = NULL;
	ECalConfigTargetSource *target;

	if (source) {
		const gchar *color_spec;

		sdialog->original_source = source;
		g_object_ref (source);
		sdialog->source_group = e_source_peek_group (source);
		xml = e_source_to_standalone_xml (source);
		sdialog->source = e_source_new_from_standalone_xml (xml);
		g_free (xml);

		color_spec = e_source_peek_color_spec (source);
		if (color_spec != NULL)
			e_source_set_color_spec (sdialog->source, color_spec);
	} else {
		cs_load_sources (sdialog, "/apps/evolution/calendar/sources", group);
	}

	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri (sdialog->source, NULL);
	e_source_set_group (sdialog->source, sdialog->source_group);

	sdialog->source_type = E_CAL_SOURCE_TYPE_EVENT;
	sdialog->config = ec = e_cal_config_new (E_CONFIG_BOOK, "org.gnome.evolution.calendar.calendarProperties");
	for (i = 0; eccp_items[i].path; i++)
		items = g_slist_prepend (items, &eccp_items[i]);
	e_config_add_items ((EConfig *) ec, items, eccp_commit, NULL, eccp_free, sdialog);
	e_config_add_page_check ((EConfig *) ec, NULL, eccp_check_complete, sdialog);

	target = e_cal_config_target_new_source (ec, sdialog->source);
	target->source_type = E_CAL_SOURCE_TYPE_EVENT;
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);

	sdialog->window = e_config_create_window ((EConfig *)ec, NULL, source ? _("Calendar Properties") : _("New Calendar"));

	/* forces initial validation */
	if (!sdialog->original_source)
		e_config_target_changed ((EConfig *)ec, E_CONFIG_TARGET_CHANGED_STATE);

	return;
}

void
calendar_setup_new_calendar (GtkWindow *parent)
{
	calendar_setup_edit_calendar (parent, NULL, NULL);
}

void
calendar_setup_edit_task_list (GtkWindow *parent, ESource *source)
{
	CalendarSourceDialog *sdialog = g_new0 (CalendarSourceDialog, 1);
	gchar *xml;
	ECalConfig *ec;
	gint i;
	GSList *items = NULL;
	ECalConfigTargetSource *target;

	if (source) {
		const gchar *color_spec;

		sdialog->original_source = source;
		g_object_ref (source);
		sdialog->source_group = e_source_peek_group (source);
		xml = e_source_to_standalone_xml (source);
		sdialog->source = e_source_new_from_standalone_xml (xml);
		g_free (xml);

		color_spec = e_source_peek_color_spec (source);
		e_source_set_color_spec (sdialog->source, color_spec);
	} else {
		cs_load_sources (sdialog, "/apps/evolution/tasks/sources", NULL);
	}

	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri (sdialog->source, NULL);
	e_source_set_group (sdialog->source, sdialog->source_group);

	sdialog->source_type = E_CAL_SOURCE_TYPE_TODO;
	sdialog->config = ec = e_cal_config_new (E_CONFIG_BOOK, "org.gnome.evolution.calendar.calendarProperties");
	for (i = 0; ectp_items[i].path; i++)
		items = g_slist_prepend (items, &ectp_items[i]);
	e_config_add_items ((EConfig *) ec, items, eccp_commit, NULL, eccp_free, sdialog);
	e_config_add_page_check ((EConfig *) ec, NULL, eccp_check_complete, sdialog);

	target = e_cal_config_target_new_source (ec, sdialog->source);
	target->source_type = E_CAL_SOURCE_TYPE_TODO;
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);

	sdialog->window = e_config_create_window ((EConfig *)ec, NULL, source ? _("Task List Properties") : _("New Task List"));

	/* forces initial validation */
	if (!sdialog->original_source)
		e_config_target_changed ((EConfig *)ec, E_CONFIG_TARGET_CHANGED_STATE);

	return;
}

void
calendar_setup_new_task_list (GtkWindow *parent)
{
	calendar_setup_edit_task_list (parent, NULL);
}

void
calendar_setup_edit_memo_list (GtkWindow *parent, ESource *source)
{
	CalendarSourceDialog *sdialog = g_new0 (CalendarSourceDialog, 1);
	gchar *xml;
	ECalConfig *ec;
	gint i;
	GSList *items = NULL;
	ECalConfigTargetSource *target;

	if (source) {
		const gchar *color_spec;

		sdialog->original_source = source;
		g_object_ref (source);
		sdialog->source_group = e_source_peek_group (source);
		xml = e_source_to_standalone_xml (source);
		sdialog->source = e_source_new_from_standalone_xml (xml);
		g_free (xml);

		color_spec = e_source_peek_color_spec (source);
		e_source_set_color_spec (sdialog->source, color_spec);
	} else {
		cs_load_sources (sdialog, "/apps/evolution/memos/sources", NULL);
	}

	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri (sdialog->source, NULL);
	e_source_set_group (sdialog->source, sdialog->source_group);

	sdialog->source_type = E_CAL_SOURCE_TYPE_JOURNAL;
	sdialog->config = ec = e_cal_config_new (E_CONFIG_BOOK, "org.gnome.evolution.calendar.calendarProperties");
	for (i = 0; ecmp_items[i].path; i++)
		items = g_slist_prepend (items, &ecmp_items[i]);
	e_config_add_items ((EConfig *) ec, items, eccp_commit, NULL, eccp_free, sdialog);
	e_config_add_page_check ((EConfig *) ec, NULL, eccp_check_complete, sdialog);

	target = e_cal_config_target_new_source (ec, sdialog->source);
	target->source_type = E_CAL_SOURCE_TYPE_JOURNAL;
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);

	sdialog->window = e_config_create_window ((EConfig *)ec, NULL, source ? _("Memo List Properties") : _("New Memo List"));

	/* forces initial validation */
	if (!sdialog->original_source)
		e_config_target_changed ((EConfig *)ec, E_CONFIG_TARGET_CHANGED_STATE);

	return;
}

void
calendar_setup_new_memo_list (GtkWindow *parent)
{
	calendar_setup_edit_memo_list (parent, NULL);
}
