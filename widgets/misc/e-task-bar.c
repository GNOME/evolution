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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-task-bar.h"

struct _ETaskBarPrivate
{
	GtkWidget *message_label;
	GtkHBox  *hbox;
};

/* WARNING: Ugly hack starts here.  */
#define MAX_ACTIVITIES_PER_COMPONENT 2

G_DEFINE_TYPE (ETaskBar, e_task_bar, GTK_TYPE_HBOX)

#if 0
static void
reduce_displayed_activities_per_component (ETaskBar *task_bar)
{
	GHashTable *component_ids_hash;
	GtkBox *box;
	GList *p;

	component_ids_hash = g_hash_table_new (g_str_hash, g_str_equal);

	box = GTK_BOX (task_bar->priv->hbox);

	for (p = box->children; p != NULL; p = p->next) {
		GtkBoxChild *child;
		const gchar *component_id;
		gpointer hash_item;

		child = (GtkBoxChild *) p->data;
		component_id = e_task_widget_get_component_id (E_TASK_WIDGET (child->widget));

		hash_item = g_hash_table_lookup (component_ids_hash, component_id);

		if (hash_item == NULL) {
			gtk_widget_show (child->widget);
			g_hash_table_insert (component_ids_hash, (gpointer) component_id, GINT_TO_POINTER (1));
		} else {
			gint num_items;

			num_items = GPOINTER_TO_INT (hash_item);
			g_return_if_fail (num_items <= MAX_ACTIVITIES_PER_COMPONENT);

			if (num_items == MAX_ACTIVITIES_PER_COMPONENT) {
				gtk_widget_hide (child->widget);
			} else {
				num_items ++;
				gtk_widget_show (child->widget);
				g_hash_table_insert (component_ids_hash, (gpointer) component_id, GINT_TO_POINTER (num_items));
			}
		}
	}

	g_hash_table_destroy (component_ids_hash);
}
#endif


static void impl_finalize (GObject *object);

static void
e_task_bar_class_init (ETaskBarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = impl_finalize;
}

static void
e_task_bar_init (ETaskBar *task_bar)
{
	GtkWidget *label, *hbox;
	gint height;

	task_bar->priv = g_new (ETaskBarPrivate, 1);

	gtk_box_set_spacing (GTK_BOX (task_bar), 10);

	label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (task_bar), label, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	task_bar->priv->message_label = label;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (task_bar), hbox);
	task_bar->priv->hbox = GTK_HBOX (hbox);

	/* Make the task bar large enough to accomodate a small icon.
	 * XXX The "* 2" is a fudge factor to allow for some padding.
	 *     The true value is probably buried in a style property. */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &height);
	gtk_widget_set_size_request (GTK_WIDGET (task_bar), -1, height * 2);
}

static void
impl_finalize (GObject *object)
{
	ETaskBar *task_bar;
	ETaskBarPrivate *priv;

	task_bar = E_TASK_BAR (object);
	priv = task_bar->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (e_task_bar_parent_class)->finalize) (object);
}


void
e_task_bar_construct (ETaskBar *task_bar)
{
	g_return_if_fail (task_bar != NULL);
	g_return_if_fail (E_IS_TASK_BAR (task_bar));

	/* Nothing to do here.  */
}

GtkWidget *
e_task_bar_new (void)
{
	ETaskBar *task_bar;

	task_bar = g_object_new (e_task_bar_get_type (), NULL);
	e_task_bar_construct (task_bar);

	return GTK_WIDGET (task_bar);
}

void
e_task_bar_set_message (ETaskBar   *task_bar,
			const gchar *message)
{
	if (message) {
		gtk_label_set_text (
			GTK_LABEL (task_bar->priv->message_label), message);
		gtk_widget_show (task_bar->priv->message_label);
	} else {
		e_task_bar_unset_message (task_bar);
	}
}

void
e_task_bar_unset_message (ETaskBar   *task_bar)
{
	gtk_widget_hide (task_bar->priv->message_label);
}

void
e_task_bar_prepend_task (ETaskBar *task_bar,
			 ETaskWidget *task_widget)
{
	GtkBoxChild *child_info;
	GtkBox *box;

	g_return_if_fail (task_bar != NULL);
	g_return_if_fail (E_IS_TASK_BAR (task_bar));
	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));

	/* Hah hah.  GTK+ sucks.  This is adapted from `gtkhbox.c'.  */

	child_info = g_new (GtkBoxChild, 1);
	child_info->widget = GTK_WIDGET (task_widget);
	child_info->padding = 0;
	child_info->expand = TRUE;
	child_info->fill = TRUE;
	child_info->pack = GTK_PACK_START;

	box = GTK_BOX (task_bar->priv->hbox);

	box->children = g_list_prepend (box->children, child_info);

	gtk_widget_set_parent (GTK_WIDGET (task_widget), GTK_WIDGET (task_bar->priv->hbox));

	if (GTK_WIDGET_REALIZED (task_bar))
		gtk_widget_realize (GTK_WIDGET (task_widget));

	if (GTK_WIDGET_VISIBLE (task_bar) && GTK_WIDGET_VISIBLE (task_widget)) {
		if (GTK_WIDGET_MAPPED (task_bar))
			gtk_widget_map (GTK_WIDGET (task_widget));
		gtk_widget_queue_resize (GTK_WIDGET (task_widget));
	}

	/* We don't restrict */
	/* reduce_displayed_activities_per_component (task_bar);*/

	gtk_widget_show (GTK_WIDGET (task_bar->priv->hbox));
}

void
e_task_bar_remove_task_from_id (ETaskBar *task_bar,
			guint id)
{
	ETaskWidget *task_widget;

	g_return_if_fail (task_bar != NULL);
	g_return_if_fail (E_IS_TASK_BAR (task_bar));

	task_widget = e_task_bar_get_task_widget_from_id (task_bar, id);
	if (!task_widget) {
		printf("Failed...\n");
		return;
	}

	gtk_widget_destroy (GTK_WIDGET (task_widget));

	/* We don't restrict here on */
	/* reduce_displayed_activities_per_component (task_bar); */

	if (g_list_length (GTK_BOX (task_bar->priv->hbox)->children) == 0)
		gtk_widget_hide (GTK_WIDGET (task_bar->priv->hbox));
}

void
e_task_bar_remove_task (ETaskBar *task_bar,
			gint n)
{
	ETaskWidget *task_widget;

	g_return_if_fail (task_bar != NULL);
	g_return_if_fail (E_IS_TASK_BAR (task_bar));
	g_return_if_fail (n >= 0);

	task_widget = e_task_bar_get_task_widget (task_bar, n);
	gtk_widget_destroy (GTK_WIDGET (task_widget));

	/* We don't restrict here on */
	/* reduce_displayed_activities_per_component (task_bar); */

	if (g_list_length (GTK_BOX (task_bar->priv->hbox)->children) == 0)
		gtk_widget_hide (GTK_WIDGET (task_bar->priv->hbox));
}

ETaskWidget *
e_task_bar_get_task_widget_from_id (ETaskBar *task_bar,
			    guint id)
{
	GtkBoxChild *child_info;
	ETaskWidget *w = NULL;
	GList *list;

	g_return_val_if_fail (task_bar != NULL, NULL);
	g_return_val_if_fail (E_IS_TASK_BAR (task_bar), NULL);

	list = GTK_BOX (task_bar->priv->hbox)->children;
	while (list) {
		child_info = list->data;
		w = (ETaskWidget *) child_info->widget;
		if (w && w->id == id)
			break;

		w = NULL;
		list = list->next;
	}

	return w;
}

ETaskWidget *

e_task_bar_get_task_widget (ETaskBar *task_bar,
			    gint n)
{
	GtkBoxChild *child_info;

	g_return_val_if_fail (task_bar != NULL, NULL);
	g_return_val_if_fail (E_IS_TASK_BAR (task_bar), NULL);

	child_info = (GtkBoxChild *) g_list_nth (GTK_BOX (task_bar->priv->hbox)->children, n)->data;

	return E_TASK_WIDGET (child_info->widget);
}

gint
e_task_bar_get_num_children (ETaskBar *task_bar)
{
	return g_list_length (GTK_BOX (task_bar->priv->hbox)->children);
}
