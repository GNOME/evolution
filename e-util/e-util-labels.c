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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <stdio.h>
#include <string.h>

#include <gconf/gconf-client.h>

#include <camel/camel-utf8.h>

#include "e-util.h"
#include "e-util-labels.h"
#include "e-dialog-utils.h"
#include "filter/filter-option.h"

typedef struct {
	const gchar *tag;
	const gchar *name;
	const gchar *colour;
} DefaultLabel;

/* Note, the first element of each DefaultLabel must NOT be translated */
DefaultLabel label_defaults[] = {
	{ "$Labelimportant", N_("I_mportant"), "#EF2929" },  /* red */
	{ "$Labelwork",      N_("_Work"),      "#F57900" },  /* orange */
	{ "$Labelpersonal",  N_("_Personal"),  "#4E9A06" },  /* green */
	{ "$Labeltodo",      N_("_To Do"),     "#3465A4" },  /* blue */
	{ "$Labellater",     N_("_Later"),     "#75507B" }   /* purple */
};

/**
 * e_util_labels_parse
 * Reads the setup from client and parses it to list of EUtilLabel objects.
 *
 * @param client The config client to be used for reading setup.
 *        Can be NULL, in that case it will use the default client.
 * @return Newly allocated list of labels, should be freed with @ref e_util_labels_free.
 **/
GSList *
e_util_labels_parse (GConfClient *client)
{
	GSList *labels, *list, *head;
	EUtilLabel *label;
	gchar *buf;
	gint num = 0;
	gboolean unref_client = client == NULL;

	labels = NULL;

	if (!client)
		client = gconf_client_get_default ();

	head = gconf_client_get_list (client, E_UTIL_LABELS_GCONF_KEY, GCONF_VALUE_STRING, NULL);

	for (list = head; list; list = list->next) {
		gchar *color, *name, *tag;
		name = buf = list->data;
		color = strrchr (buf, ':');
		if (color == NULL) {
			g_free (buf);
			continue;
		}

		*color++ = '\0';
		tag = strchr (color, '|');
		if (tag)
			*tag++ = '\0';

		label = g_new (EUtilLabel, 1);

		/* Needed for Backward Compatibility */
		if (num < G_N_ELEMENTS (label_defaults)) {
			label->name = g_strdup ((buf && *buf) ? buf : _(label_defaults[num].name));
			label->tag = g_strdup (label_defaults[num].tag);
			num++;
		} else if (!tag) {
			g_free (buf);
			g_free (label);
			continue;
		} else {
			label->name = g_strdup (name);
			label->tag = g_strdup (tag);
		}

		label->colour = g_strdup (color);
		labels = g_slist_prepend (labels, label);

		g_free (buf);
	}

	if (head)
		g_slist_free (head);

	while (num < G_N_ELEMENTS (label_defaults)) {
		/* complete the list with defaults */
		label = g_new (EUtilLabel, 1);
		label->tag = g_strdup (label_defaults[num].tag);
		label->name = g_strdup (_(label_defaults[num].name));
		label->colour = g_strdup (label_defaults[num].colour);

		labels = g_slist_prepend (labels, label);

		num++;
	}

	if (unref_client)
		g_object_unref (client);

	return g_slist_reverse (labels);
}

static void
free_label_struct (EUtilLabel *label)
{
	if (!label)
		return;

	g_free (label->tag);
	g_free (label->name);
	g_free (label->colour);
	g_free (label);
}

/**
 * e_util_labels_free
 * Frees memory previously allocated by @ref e_util_labels_parse
 *
 * @param labels Labels list, previously allocated by @ref e_util_labels_parse
 *               It is safe to call with NULL.
 **/
void
e_util_labels_free (GSList *labels)
{
	if (!labels)
		return;

	g_slist_foreach (labels, (GFunc)free_label_struct, NULL);
	g_slist_free (labels);
}

/* stores the actual cache to gconf */
static gboolean
flush_labels_cache (GSList *labels, gboolean free_labels)
{
	GSList *l, *text_labels;
	GConfClient *client;

	if (!labels)
		return FALSE;

	text_labels = NULL;

	for (l = labels; l; l = l->next) {
		EUtilLabel *label = l->data;

		if (label && label->tag && label->name && label->colour)
			text_labels = g_slist_prepend (text_labels, g_strdup_printf ("%s:%s|%s", label->name, label->colour, label->tag));
	}

	if (!text_labels) {
		if (free_labels)
			e_util_labels_free (labels);

		return FALSE;
	}

	text_labels = g_slist_reverse (text_labels);

	client = gconf_client_get_default ();
	gconf_client_set_list (client, E_UTIL_LABELS_GCONF_KEY, GCONF_VALUE_STRING, text_labels, NULL);
	g_object_unref (client);

	g_slist_foreach (text_labels, (GFunc)g_free, NULL);
	g_slist_free (text_labels);

	if (free_labels)
		e_util_labels_free (labels);

	/* not true if gconf failed to write; who cares */
	return TRUE;
}

/**
 * find_label
 *
 * Looks for label in labels cache by tag and returns actual pointer to cache.
 * @param labels The cache of labels; comes from @ref e_util_labels_parse
 * @param tag Tag of label you are looking for.
 * @return Pointer to cache data if label with such tag exists or NULL. Do not free it!
 **/
static EUtilLabel *
find_label (GSList *labels, const gchar *tag)
{
	GSList *l;

	g_return_val_if_fail (tag != NULL, NULL);

	for (l = labels; l; l = l->next) {
		EUtilLabel *label = l->data;

		if (label && label->tag && !strcmp (tag, label->tag))
			return label;
	}

	return NULL;
}

static gchar *
tag_from_name (const gchar *name)
{
	/* this does thunderbird, just do not ask */
	gchar *s1, *s2, *p;
	const gchar *bads = " ()/{%*<>\\\"";

	if (!name || !*name)
		return NULL;

	s1 = g_strdup (name);
	for (p = s1; p && *p; p++) {
		if (strchr (bads, *p))
			*p = '_';
	}

	s2 = camel_utf8_utf7 (s1);
	g_free (s1);

	s1 = g_ascii_strdown (s2, -1);
	g_free (s2);

	return s1;
}

/**
 * e_util_labels_add
 * Creates new label at the end of actual list of labels.
 *
 * @param name User readable name of this label. Should not be NULL.
 * @param color Color assigned to this label. Should not be NULL.
 * @return If succeeded then new label tag, NULL otherwise.
 *         Returned pointer should be freed with g_free.
 *         It will return NULL when the tag will be same as already existed.
 *         Tag name is generated in similar way as in Thunderbird.
 **/
gchar *
e_util_labels_add (const gchar *name, const GdkColor *color)
{
	EUtilLabel *label;
	GSList *labels;
	gchar *tag;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (color != NULL, NULL);

	labels = e_util_labels_parse (NULL);
	tag = tag_from_name (name);

	if (!tag || find_label (labels, tag) != NULL) {
		g_free (tag);
		e_util_labels_free (labels);
		return NULL;
	}

	label = g_new0 (EUtilLabel, 1);
	label->tag = g_strdup (tag);
	label->name = g_strdup (name);
	label->colour = gdk_color_to_string (color);

	labels = g_slist_append (labels, label);

	flush_labels_cache (labels, TRUE);

	return tag;
}

/**
 * e_util_labels_add_with_dlg
 * This will open a dialog to add or edit label.
 *
 * @param parent Parent widget for the dialog.
 * @param tag A tag for existing label to edit or NULL to add new label.
 * @return Tag for newly added label or NULL, if something failed.
 *         Returned value should be freed with g_free.
 **/
gchar *
e_util_labels_add_with_dlg (GtkWindow *parent, const gchar *tag)
{
	GtkWidget *table, *dialog, *l, *e, *c;
	const gchar *name;
	GdkColor color;
	gboolean is_edit = FALSE;
	gchar *new_tag = NULL;
	GSList *labels;

	table = gtk_table_new (2, 2, FALSE);

	labels = e_util_labels_parse (NULL);
	name = tag ? e_util_labels_get_name (labels, tag) : NULL;

	l = gtk_label_new_with_mnemonic (_("Label _Name:"));
	e = gtk_entry_new ();
	c = gtk_color_button_new ();

	if (!tag || !e_util_labels_get_color (labels, tag, &color))
		memset (&color, 0xCD, sizeof (GdkColor));
	else
		is_edit = TRUE;

	if (name)
		gtk_entry_set_text (GTK_ENTRY (e), name);

	gtk_entry_set_activates_default (GTK_ENTRY (e), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (l), e);
	gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.0);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (c), &color);

	gtk_table_attach (GTK_TABLE (table), l, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), e, 0, 1, 1, 2, 0, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), c, 1, 2, 1, 2, 0, 0, 0, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table), 10);
	gtk_widget_show_all (table);

	dialog = gtk_dialog_new_with_buttons (is_edit ? _("Edit Label") : _("Add Label"),
					      parent,
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);

	while (!new_tag) {
		const gchar *error = NULL;

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
			name = gtk_entry_get_text (GTK_ENTRY (e));
			gtk_color_button_get_color (GTK_COLOR_BUTTON (c), &color);

			if (!name || !*name)
				error = _("Label name cannot be empty.");
			else if (is_edit) {
				e_util_labels_set_data (tag, name, &color);
				break;
			} else if (!(new_tag = e_util_labels_add (name, &color)))
				error = _("A label having the same tag already exists on the server. Please rename your label.");
			else
				break;
		} else
			break;

		if (error)
			e_notice (parent, GTK_MESSAGE_ERROR, error);
	}

	gtk_widget_destroy (dialog);
	e_util_labels_free (labels);

	return new_tag;
}

/**
 * e_util_labels_remove
 *
 * @param tag Tag of the label to remove.
 * @return Whether was removed.
 **/
gboolean
e_util_labels_remove (const gchar *tag)
{
	EUtilLabel *label;
	GSList *labels;

	g_return_val_if_fail (tag != NULL, FALSE);

	labels = e_util_labels_parse (NULL);
	label = find_label (labels, tag);

	if (!label) {
		e_util_labels_free (labels);
		return FALSE;
	}

	labels = g_slist_remove (labels, label);

	free_label_struct (label);

	return	flush_labels_cache (labels, TRUE);
}

/**
 * e_util_labels_set_data
 *
 * @param tag Tag of the label of our interest.
 * @param name New name for the label.
 * @param color New color for the label.
 * @return Whether successfully saved.
 **/
gboolean
e_util_labels_set_data (const gchar *tag, const gchar *name, const GdkColor *color)
{
	EUtilLabel *label;
	GSList *labels;

	g_return_val_if_fail (tag != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	labels = e_util_labels_parse (NULL);
	label = find_label (labels, tag);

	if (!label) {
		e_util_labels_free (labels);
		return FALSE;
	}

	g_free (label->name);
	label->name = g_strdup (name);

	g_free (label->colour);
	label->colour = gdk_color_to_string (color);

	return flush_labels_cache (labels, TRUE);
}

/**
 * e_util_labels_is_system
 *
 * @return Whether the tag is one of default/system labels or not.
 **/
gboolean
e_util_labels_is_system (const gchar *tag)
{
	gint i;

	if (!tag)
		return FALSE;

	for (i = 0; i < G_N_ELEMENTS (label_defaults); i++) {
		if (strcmp (tag, label_defaults[i].tag) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * e_util_labels_get_new_tag
 *
 * @param old_tag Tag of the label from old version of Evolution.
 * @return New tag name equivalent with the old tag, or NULL if no such name existed before.
 **/
const gchar *
e_util_labels_get_new_tag (const gchar *old_tag)
{
	gint i;

	if (!old_tag)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (label_defaults); i++) {
		/* default labels have same name as those old, only with prefix "$Label" */
		if (!strcmp (old_tag, label_defaults[i].tag + 6))
			return label_defaults[i].tag;
	}

	return NULL;
}

/**
 * e_util_labels_get_name
 *
 * @param labels Cache of labels from call of @ref e_util_labels_parse.
 *        The returned pointer will be taken from this list, so it's alive as long as the list.
 * @param tag Tag of the label of our interest.
 * @return Name of the label with that tag or NULL, if no such label exists.
 **/
const gchar *
e_util_labels_get_name (GSList *labels, const gchar *tag)
{
	EUtilLabel *label;

	g_return_val_if_fail (tag != NULL, NULL);

	label = find_label (labels, tag);
	if (!label)
		return NULL;

	return label->name;
}

/**
 * e_util_labels_get_color
 *
 * @param labels Cache of labels from call of @ref e_util_labels_parse.
 * @param tag Tag of the label of our interest.
 * @param color [out] Actual color of the label with that tag, or unchanged if failed.
 * @return Whether found such label and color has been set.
 **/
gboolean
e_util_labels_get_color (GSList *labels, const gchar *tag, GdkColor *color)
{
	EUtilLabel *label;

	g_return_val_if_fail (tag != NULL, FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	label = find_label (labels, tag);
	if (!label)
		return FALSE;

	return gdk_color_parse (label->colour, color);
}

/**
 * e_util_labels_get_color_str
 *
 * @param labels Cache of labels from call of @ref e_util_labels_parse.
 *        The returned pointer will be taken from this list, so it's alive as long as the list.
 * @param tag Tag of the label of our interest.
 * @return String representation of that label, or NULL, if no such label exists.
 **/
const gchar *
e_util_labels_get_color_str (GSList *labels, const gchar *tag)
{
	EUtilLabel *label;

	g_return_val_if_fail (tag != NULL, NULL);

	label = find_label (labels, tag);
	if (!label)
		return NULL;

	return label->colour;
}

/**
 * e_util_labels_get_filter_options:
 * Returns list of newly allocated struct _filter_option-s, to be used in filters.
 **/
GSList *
e_util_labels_get_filter_options (void)
{
	GSList *known = e_util_labels_parse (NULL), *l;
	GSList *res = NULL;

	for (l = known; l; l = l->next) {
		EUtilLabel *label = l->data;
		const gchar *tag;
		struct _filter_option *fo;

		if (!label)
			continue;

		tag = label->tag;

		if (tag && strncmp (tag, "$Label", 6) == 0)
			tag += 6;

		fo = g_new0 (struct _filter_option, 1);
		fo->title = e_str_without_underscores (label->name);
		fo->value = g_strdup (tag);

		res = g_slist_prepend (res, fo);
	}

	e_util_labels_free (known);

	return g_slist_reverse (res);
}
