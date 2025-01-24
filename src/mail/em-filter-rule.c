/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Not Zed <notzed@ximian.com>
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "e-mail-ui-session.h"
#include "em-filter-rule.h"
#include "em-filter-context.h"

#define d(x)

struct _EMFilterRulePrivate {
	GList *actions;
	gchar *account_uid;
};

static gint validate (EFilterRule *fr, EAlert **alert);
static gint filter_eq (EFilterRule *fr, EFilterRule *cm);
static xmlNodePtr xml_encode (EFilterRule *fr);
static gint xml_decode (EFilterRule *fr, xmlNodePtr, ERuleContext *rc);
static void rule_copy (EFilterRule *dest, EFilterRule *src);
static GtkWidget *get_widget (EFilterRule *fr, ERuleContext *rc);

G_DEFINE_TYPE_WITH_PRIVATE (EMFilterRule, em_filter_rule, E_TYPE_FILTER_RULE)

static void
em_filter_rule_build_code (EFilterRule *rule,
			   GString *out)
{
	EMFilterRule *ff;

	g_return_if_fail (EM_IS_FILTER_RULE (rule));
	g_return_if_fail (out != NULL);

	ff = EM_FILTER_RULE (rule);

	E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->build_code (rule, out);

	if (ff->priv->account_uid && *ff->priv->account_uid) {
		if (out->len) {
			gchar *prefix;

			prefix = g_strdup_printf ("(and (header-source \"%s\")\n", ff->priv->account_uid);
			g_string_prepend (out, prefix);
			g_string_append (out, ")\n");
			g_free (prefix);
		} else {
			g_string_append_printf (out, "(header-source \"%s\")\n", ff->priv->account_uid);
		}
	}
}

static void
em_filter_rule_finalize (GObject *object)
{
	EMFilterRule *ff = EM_FILTER_RULE (object);

	g_list_free_full (ff->priv->actions, g_object_unref);
	g_free (ff->priv->account_uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_filter_rule_parent_class)->finalize (object);
}

static void
em_filter_rule_class_init (EMFilterRuleClass *class)
{
	GObjectClass *object_class;
	EFilterRuleClass *filter_rule_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = em_filter_rule_finalize;

	filter_rule_class = E_FILTER_RULE_CLASS (class);
	filter_rule_class->validate = validate;
	filter_rule_class->eq = filter_eq;
	filter_rule_class->xml_encode = xml_encode;
	filter_rule_class->xml_decode = xml_decode;
	filter_rule_class->build_code = em_filter_rule_build_code;
	filter_rule_class->copy = rule_copy;
	filter_rule_class->get_widget = get_widget;
}

static void
em_filter_rule_init (EMFilterRule *ff)
{
	ff->priv = em_filter_rule_get_instance_private (ff);
}

/**
 * em_filter_rule_new:
 *
 * Create a new EMFilterRule object.
 *
 * Return value: A new #EMFilterRule object.
 **/
EFilterRule *
em_filter_rule_new (void)
{
	return g_object_new (em_filter_rule_get_type (), NULL, NULL);
}

void
em_filter_rule_add_action (EMFilterRule *fr,
                           EFilterPart *fp)
{
	fr->priv->actions = g_list_append (fr->priv->actions, fp);

	e_filter_rule_emit_changed ((EFilterRule *) fr);
}

void
em_filter_rule_remove_action (EMFilterRule *fr,
                              EFilterPart *fp)
{
	fr->priv->actions = g_list_remove (fr->priv->actions, fp);

	e_filter_rule_emit_changed ((EFilterRule *) fr);
}

void
em_filter_rule_replace_action (EMFilterRule *fr,
                               EFilterPart *fp,
                               EFilterPart *new)
{
	GList *l;

	l = g_list_find (fr->priv->actions, fp);
	if (l) {
		l->data = new;
	} else {
		fr->priv->actions = g_list_append (fr->priv->actions, new);
	}

	e_filter_rule_emit_changed ((EFilterRule *) fr);
}

void
em_filter_rule_build_action (EMFilterRule *fr,
                             GString *out)
{
	g_string_append (out, "(begin\n");
	e_filter_part_build_code_list (fr->priv->actions, out);
	g_string_append (out, ")\n");
}

static gint
validate (EFilterRule *fr,
          EAlert **alert)
{
	EMFilterRule *ff =(EMFilterRule *) fr;
	GList *parts;
	gint valid;

	valid = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		validate (fr, alert);

	/* validate rule actions */
	parts = ff->priv->actions;
	while (parts && valid) {
		valid = e_filter_part_validate ((EFilterPart *) parts->data, alert);
		parts = parts->next;
	}

	return valid;
}

static gint
list_eq (GList *al,
         GList *bl)
{
	gint truth = TRUE;

	while (truth && al && bl) {
		EFilterPart *a = al->data, *b = bl->data;

		truth = e_filter_part_eq (a, b);
		al = al->next;
		bl = bl->next;
	}

	return truth && al == NULL && bl == NULL;
}

static gint
filter_eq (EFilterRule *fr,
           EFilterRule *cm)
{
	return E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->eq (fr, cm)
		&& g_strcmp0 (em_filter_rule_get_account_uid (EM_FILTER_RULE (fr)), em_filter_rule_get_account_uid (EM_FILTER_RULE (cm))) == 0
		&& list_eq (
			((EMFilterRule *) fr)->priv->actions,
			((EMFilterRule *) cm)->priv->actions);
}

static xmlNodePtr
xml_encode (EFilterRule *fr)
{
	EMFilterRule *ff =(EMFilterRule *) fr;
	xmlNodePtr node, set, work;
	GList *l;

	node = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		xml_encode (fr);
	g_return_val_if_fail (node != NULL, NULL);

	if (ff->priv->account_uid && *ff->priv->account_uid)
		xmlSetProp (node, (const xmlChar *) "account-uid", (const xmlChar *) ff->priv->account_uid);

	set = xmlNewNode (NULL, (const guchar *)"actionset");
	xmlAddChild (node, set);
	l = ff->priv->actions;
	while (l) {
		work = e_filter_part_xml_encode ((EFilterPart *) l->data);
		xmlAddChild (set, work);
		l = l->next;
	}

	return node;

}

static void
load_set (xmlNodePtr node,
          EMFilterRule *ff,
          ERuleContext *rc)
{
	xmlNodePtr work;
	gchar *rulename;
	EFilterPart *part;

	work = node->children;
	while (work) {
		if (!strcmp ((gchar *) work->name, "part")) {
			rulename = (gchar *) xmlGetProp (work, (const guchar *)"name");
			part = em_filter_context_find_action ((EMFilterContext *) rc, rulename);
			if (part) {
				part = e_filter_part_clone (part);
				e_filter_part_xml_decode (part, work);
				em_filter_rule_add_action (ff, part);
			} else {
				g_warning ("cannot find rule part '%s'\n", rulename);
			}
			xmlFree (rulename);
		} else if (work->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node in part: %s", work->name);
		}
		work = work->next;
	}
}

static gint
xml_decode (EFilterRule *fr,
            xmlNodePtr node,
            ERuleContext *rc)
{
	EMFilterRule *ff =(EMFilterRule *) fr;
	xmlNodePtr work;
	xmlChar *account_uid;
	gint result;

	result = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		xml_decode (fr, node, rc);
	if (result != 0)
		return result;

	g_clear_pointer (&ff->priv->account_uid, g_free);

	account_uid = xmlGetProp (node, (const xmlChar *) "account-uid");
	if (account_uid) {
		if (*account_uid)
			ff->priv->account_uid = g_strdup ((const gchar *) account_uid);

		xmlFree (account_uid);
	}

	work = node->children;
	while (work) {
		if (!strcmp ((gchar *) work->name, "actionset")) {
			load_set (work, ff, rc);
		}
		work = work->next;
	}

	return 0;
}

static void
rule_copy (EFilterRule *dest,
           EFilterRule *src)
{
	EMFilterRule *fdest, *fsrc;
	GList *node;

	fdest =(EMFilterRule *) dest;
	fsrc =(EMFilterRule *) src;

	if (fdest->priv->actions) {
		g_list_free_full (fdest->priv->actions, g_object_unref);
		fdest->priv->actions = NULL;
	}

	node = fsrc->priv->actions;
	while (node) {
		EFilterPart *part = node->data;

		g_object_ref (part);
		fdest->priv->actions = g_list_append (fdest->priv->actions, part);
		node = node->next;
	}

	em_filter_rule_set_account_uid (fdest, em_filter_rule_get_account_uid (fsrc));

	E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->copy (dest, src);
}

struct _part_data {
	EFilterRule *fr;
	EMFilterContext *f;
	EFilterPart *part;
	GtkWidget *partwidget, *container;
};

static void
part_combobox_changed (GtkComboBox *combobox,
                       struct _part_data *data)
{
	EFilterPart *part = NULL;
	EFilterPart *newpart;
	gint index, i;

	index = gtk_combo_box_get_active (combobox);
	for (i = 0, part = em_filter_context_next_action (data->f, part);
		part && i < index;
		i++, part = em_filter_context_next_action (data->f, part)) {
		/* traverse until reached index */
	}

	if (!part) {
		g_warn_if_reached ();
		return;
	}
	g_return_if_fail (i == index);

	/* don't update if we haven't changed */
	if (!strcmp (part->title, data->part->title))
		return;

	/* here we do a widget shuffle, throw away the old widget/rulepart,
	 * and create another */
	if (data->partwidget)
		gtk_container_remove (GTK_CONTAINER (data->container), data->partwidget);

	newpart = e_filter_part_clone (part);
	e_filter_part_copy_values (newpart, data->part);
	em_filter_rule_replace_action ((EMFilterRule *) data->fr, data->part, newpart);
	g_object_unref (data->part);
	data->part = newpart;
	data->partwidget = e_filter_part_get_widget (newpart);
	if (data->partwidget)
		gtk_box_pack_start (
			GTK_BOX (data->container),
			data->partwidget, TRUE, TRUE, 0);
}

static GtkWidget *
get_rule_part_widget (EMFilterContext *f,
                      EFilterPart *newpart,
                      EFilterRule *fr)
{
	EFilterPart *part = NULL;
	GtkWidget *combobox;
	GtkWidget *hbox;
	GtkWidget *p;
	gint index = 0, current = 0;
	struct _part_data *data;

	data = g_malloc0 (sizeof (*data));
	data->fr = fr;
	data->f = f;
	data->part = newpart;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	/* only set to automatically clean up the memory and for less_parts */
	g_object_set_data_full ((GObject *) hbox, "data", data, g_free);

	p = e_filter_part_get_widget (newpart);

	data->partwidget = p;
	data->container = hbox;

	combobox = gtk_combo_box_text_new ();
	while ((part = em_filter_context_next_action (f, part))) {
		gtk_combo_box_text_append_text (
			GTK_COMBO_BOX_TEXT (combobox), _(part->title));

		if (!strcmp (newpart->title, part->title))
			current = index;

		index++;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), current);
	g_signal_connect (
		combobox, "changed",
		G_CALLBACK (part_combobox_changed), data);
	gtk_widget_show (combobox);

	gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, FALSE, 0);
	if (p)
		gtk_box_pack_start (GTK_BOX (hbox), p, TRUE, TRUE, 0);

	gtk_widget_show_all (hbox);

	return hbox;
}

struct _rule_data {
	EFilterRule *fr;
	EMFilterContext *f;
	GtkGrid *parts_grid;
	GtkWidget *drag_widget;

	gint n_rows;
};

enum {
	DND_TYPE_FILTER_ACTION,
	N_DND_TYPES
};

static GtkTargetEntry dnd_types[] = {
	{ (gchar *) "x-evolution-filter-action", GTK_TARGET_SAME_APP, DND_TYPE_FILTER_ACTION }
};

static GdkAtom dnd_atoms[N_DND_TYPES] = { 0 };

static void
event_box_drag_begin (GtkWidget *widget,
		      GdkDragContext *context,
		      gpointer user_data)
{
	struct _rule_data *data = user_data;
	cairo_surface_t *surface;
	cairo_t *cr;
	GtkStyleContext *style_context;

	data->drag_widget = widget;

	/* Just 1x1 dot as an image. No need to have there real icon,
	   because the move is done immediately */
	surface = gdk_window_create_similar_surface ( gtk_widget_get_window (widget), CAIRO_CONTENT_COLOR, 1, 1);
	style_context = gtk_widget_get_style_context (widget);
	cr = cairo_create (surface);
	gtk_render_background (style_context, cr, 0, 0, 1, 1);
	cairo_destroy (cr);

	cairo_surface_set_device_offset (surface, 0, 0);

	gtk_drag_set_icon_surface (context, surface);
	cairo_surface_destroy (surface);
}

static gboolean
event_box_drag_drop (GtkWidget *widget,
		     GdkDragContext *context,
		     gint x,
		     gint y,
		     guint time,
		     gpointer user_data)
{
	struct _rule_data *data = user_data;

	data->drag_widget = NULL;

	return FALSE;
}

static void
event_box_drag_end (GtkWidget *widget,
		    GdkDragContext *context,
		    gpointer user_data)
{
	struct _rule_data *data = user_data;

	data->drag_widget = NULL;
}

static gboolean
event_box_drag_motion_cb (GtkWidget *widget,
			  GdkDragContext *context,
			  gint x,
			  gint y,
			  guint time,
			  gpointer user_data)
{
	struct _rule_data *data = user_data;

	gdk_drag_status (context, widget == data->drag_widget ? 0 : GDK_ACTION_MOVE, time);

	if (widget != data->drag_widget) {
		gint index, index_src = -1, index_des = -1;

		for (index = 0; index < data->n_rows && (index_src == -1 || index_des == -1); index++) {
			GtkWidget *event_box;

			event_box = gtk_grid_get_child_at (data->parts_grid, 0, index);
			if (event_box == data->drag_widget) {
				index_src = index;
			} else if (event_box == widget) {
				index_des = index;
			}
		}

		g_warn_if_fail (index_src != -1);
		g_warn_if_fail (index_des != -1);
		g_warn_if_fail (index_src != index_des);

		if (index_src != -1 && index_des != -1 && index_src != index_des) {
			EMFilterRule *fr = (EMFilterRule *) data->fr;
			GtkWidget *event_box, *content, *remove_button;
			gpointer rule;

			/* Move internal data first */
			rule = g_list_nth_data (fr->priv->actions, index_src);
			fr->priv->actions = g_list_remove (fr->priv->actions, rule);
			fr->priv->actions = g_list_insert (fr->priv->actions, rule, index_des);

			/* Then the UI part */
			event_box = gtk_grid_get_child_at (data->parts_grid, 0, index_src);
			content = gtk_grid_get_child_at (data->parts_grid, 1, index_src);
			remove_button = gtk_grid_get_child_at (data->parts_grid, 2, index_src);

			g_warn_if_fail (event_box != NULL);
			g_warn_if_fail (content != NULL);
			g_warn_if_fail (remove_button != NULL);

			g_object_ref (event_box);
			g_object_ref (content);
			g_object_ref (remove_button);

			gtk_grid_remove_row (data->parts_grid, index_src);
			gtk_grid_insert_row (data->parts_grid, index_des);
			gtk_grid_attach (data->parts_grid, event_box, 0, index_des, 1, 1);
			gtk_grid_attach (data->parts_grid, content, 1, index_des, 1, 1);
			gtk_grid_attach (data->parts_grid, remove_button, 2, index_des, 1, 1);

			g_object_unref (event_box);
			g_object_unref (content);
			g_object_unref (remove_button);
		}
	}

	return TRUE;
}

static gboolean
rule_widget_drag_motion_cb (GtkWidget *widget,
			    GdkDragContext *context,
			    gint x,
			    gint y,
			    guint time,
			    gpointer user_data)
{
	struct _rule_data *data = user_data;
	gint ii;

	for (ii = 0; ii < data->n_rows; ii++) {
		if (gtk_grid_get_child_at (data->parts_grid, 1, ii) == widget) {
			return event_box_drag_motion_cb (gtk_grid_get_child_at (data->parts_grid, 0, ii),
				context, x, y, time, user_data);
		}
	}

	gdk_drag_status (context, 0, time);

	return FALSE;
}

static void
less_parts (GtkWidget *button,
            struct _rule_data *data)
{
	EFilterPart *part;
	GtkWidget *content = NULL;
	struct _part_data *part_data;
	gint index;

	if (g_list_length (((EMFilterRule *) data->fr)->priv->actions) < 2)
		return;

	for (index = 0; index < data->n_rows; index++) {
		if (button == gtk_grid_get_child_at (data->parts_grid, 2, index)) {
			content = gtk_grid_get_child_at (data->parts_grid, 1, index);
			break;
		}
	}

	g_return_if_fail (content != NULL);

	part_data = g_object_get_data ((GObject *) content, "data");

	g_return_if_fail (part_data != NULL);

	part = part_data->part;

	index = g_list_index (((EMFilterRule *) data->fr)->priv->actions, part);
	g_warn_if_fail (index >= 0);

	/* remove the part from the list */
	em_filter_rule_remove_action ((EMFilterRule *) data->fr, part);
	g_object_unref (part);

	/* and from the display */
	if (index >= 0) {
		gtk_grid_remove_row (data->parts_grid, index);
		data->n_rows--;
	}
}

static void
attach_rule (GtkWidget *rule,
             struct _rule_data *data,
             EFilterPart *part,
             gint row)
{
	GtkWidget *remove;
	GtkWidget *event_box, *label;

	event_box = gtk_event_box_new ();
	label = gtk_label_new ("⇕");
	gtk_container_add (GTK_CONTAINER (event_box), label);
	gtk_widget_set_sensitive (label, FALSE);
	gtk_widget_show (label);

	g_object_set (G_OBJECT (event_box),
		"halign", GTK_ALIGN_FILL,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	g_object_set (G_OBJECT (rule),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_CENTER,
		"vexpand", FALSE,
		NULL);

	remove = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	g_object_set (G_OBJECT (remove),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"vexpand", FALSE,
		"visible", TRUE,
		NULL);

	g_signal_connect (
		remove, "clicked",
		G_CALLBACK (less_parts), data);

	gtk_grid_insert_row (data->parts_grid, row);
	gtk_grid_attach (data->parts_grid, event_box, 0, row, 1, 1);
	gtk_grid_attach (data->parts_grid, rule, 1, row, 1, 1);
	gtk_grid_attach (data->parts_grid, remove, 2, row, 1, 1);

	if (!dnd_atoms[0]) {
		gint ii;

		for (ii = 0; ii < N_DND_TYPES; ii++)
			dnd_atoms[ii] = gdk_atom_intern (dnd_types[ii].target, FALSE);
	}

	gtk_drag_source_set (event_box, GDK_BUTTON1_MASK, dnd_types, N_DND_TYPES, GDK_ACTION_MOVE);
	gtk_drag_dest_set (event_box, GTK_DEST_DEFAULT_MOTION, dnd_types, N_DND_TYPES, GDK_ACTION_MOVE);

	g_signal_connect (event_box, "drag-begin",
		G_CALLBACK (event_box_drag_begin), data);
	g_signal_connect (event_box, "drag-motion",
		G_CALLBACK (event_box_drag_motion_cb), data);
	g_signal_connect (event_box, "drag-drop",
		G_CALLBACK (event_box_drag_drop), data);
	g_signal_connect (event_box, "drag-end",
		G_CALLBACK (event_box_drag_end), data);

	gtk_drag_dest_set (rule, GTK_DEST_DEFAULT_MOTION, dnd_types, N_DND_TYPES, GDK_ACTION_MOVE);

	g_signal_connect (rule, "drag-motion",
		G_CALLBACK (rule_widget_drag_motion_cb), data);
}

static void
do_grab_focus_cb (GtkWidget *widget,
                  gpointer data)
{
	gboolean *done = (gboolean *) data;

	if (*done || !widget)
		return;

	if (gtk_widget_get_can_focus (widget) || GTK_IS_COMBO_BOX (widget)) {
		*done = TRUE;
		gtk_widget_grab_focus (widget);
	} else if (GTK_IS_CONTAINER (widget)) {
		gtk_container_foreach (GTK_CONTAINER (widget), do_grab_focus_cb, done);
	}
}

static gboolean
scroll_to_new_part_idle_cb (gpointer user_data)
{
	GtkScrolledWindow *scrolled_window = user_data;
	GtkAdjustment *adjustment;

	adjustment = gtk_scrolled_window_get_vadjustment (scrolled_window);
	if (adjustment) {
		gdouble upper;

		upper = gtk_adjustment_get_upper (adjustment);
		gtk_adjustment_set_value (adjustment, upper);
	}

	g_object_unref (scrolled_window);

	return G_SOURCE_REMOVE;
}

static void
more_parts (GtkWidget *button,
            struct _rule_data *data)
{
	EFilterPart *new;

	/* create a new rule entry, use the first type of rule */
	new = em_filter_context_next_action ((EMFilterContext *) data->f, NULL);
	if (new) {
		GtkWidget *w;

		new = e_filter_part_clone (new);
		em_filter_rule_add_action ((EMFilterRule *) data->fr, new);
		w = get_rule_part_widget (data->f, new, data->fr);

		attach_rule (w, data, new, data->n_rows);
		data->n_rows++;

		if (GTK_IS_CONTAINER (w)) {
			gboolean done = FALSE;

			gtk_container_foreach (GTK_CONTAINER (w), do_grab_focus_cb, &done);
		} else
			gtk_widget_grab_focus (w);

		/* also scroll down to see new part */
		w = (GtkWidget *) g_object_get_data (G_OBJECT (button), "scrolled-window");
		if (w) {
			e_util_ensure_scrolled_window_height (GTK_SCROLLED_WINDOW (w));

			/* let the scrolled window some time to recalculate size of its
			   content and propagate it into the vertical adjustment */
			g_idle_add (scroll_to_new_part_idle_cb, g_object_ref (w));
		}
	}
}

static void
filter_type_changed_cb (GtkComboBox *combobox,
			EFilterRule *fr)
{
	const gchar *id;

	g_return_if_fail (GTK_IS_COMBO_BOX (combobox));
	g_return_if_fail (E_IS_FILTER_RULE (fr));

	id = gtk_combo_box_get_active_id (combobox);
	if (id && *id)
		e_filter_rule_set_source (fr, id);
}

static void
filter_rule_accounts_changed_cb (GtkComboBox *combobox,
				 EMFilterRule *fr)
{
	const gchar *id;

	g_return_if_fail (GTK_IS_COMBO_BOX (combobox));
	g_return_if_fail (EM_IS_FILTER_RULE (fr));

	id = gtk_combo_box_get_active_id (combobox);
	em_filter_rule_set_account_uid (fr, id);
}

/* This should have blocked the filter_rule_accounts_changed_cb() handler */
static void
filter_rule_select_account (GtkComboBox *accounts,
			    const gchar *account_uid)
{
	g_return_if_fail (GTK_IS_COMBO_BOX (accounts));

	if (!account_uid || !*account_uid) {
		gtk_combo_box_set_active (accounts, 0);
		return;
	}

	if (!gtk_combo_box_set_active_id (accounts, account_uid)) {
		CamelSession *session = g_object_get_data (G_OBJECT (accounts), "e-mail-session");
		CamelService *service = camel_session_ref_service (session, account_uid);

		if (service) {
			/* It is disabled account */
			gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (accounts),
				camel_service_get_uid (service),
				camel_service_get_display_name (service));
		} else {
			/* It is removed/non-existent account */
			gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (accounts),
				account_uid, account_uid);
		}

		g_warn_if_fail (gtk_combo_box_set_active_id (accounts, account_uid));

		g_clear_object (&service);
	}
}

static gint
filter_rule_compare_services (gconstpointer s1,
			      gconstpointer s2)
{
	CamelService *service1 = (CamelService *) s1;
	CamelService *service2 = (CamelService *) s2;

	return g_utf8_collate (camel_service_get_display_name (service1), camel_service_get_display_name (service2));
}

static void
filter_rule_fill_account_combo (GtkComboBox *source_combo,
				GtkComboBoxText *accounts_combo)
{
	ESourceRegistry *registry;
	CamelSession *session;
	GList *services, *link;
	GSList *add_services = NULL, *slink;
	gboolean is_incoming;
	gchar *account_id;

	g_return_if_fail (GTK_IS_COMBO_BOX (source_combo));
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (accounts_combo));

	session = g_object_get_data (G_OBJECT (accounts_combo), "e-mail-session");
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));
	is_incoming = g_strcmp0 (gtk_combo_box_get_active_id (source_combo), E_FILTER_SOURCE_INCOMING) == 0;
	account_id = g_strdup (gtk_combo_box_get_active_id (GTK_COMBO_BOX (accounts_combo)));

	g_signal_handlers_block_matched (accounts_combo, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, filter_rule_accounts_changed_cb, NULL);

	gtk_combo_box_text_remove_all (accounts_combo);

	/* Translators: This is in a Mail filter rule editor, part of "For account: [ Any   ]" section */
	gtk_combo_box_text_append (accounts_combo, NULL, C_("mail-filter-rule", "Any"));

	services = camel_session_list_services (session);

	for (link = services; link; link = g_list_next (link)) {
		CamelService *service = link->data;
		const gchar *uid = camel_service_get_uid (service);

		if (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0 ||
		    g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0)
			continue;

		if (is_incoming && CAMEL_IS_STORE (service) &&
		    (camel_store_get_flags (CAMEL_STORE (service)) & CAMEL_STORE_IS_BUILTIN) != 0)
			continue;

		if ((is_incoming && CAMEL_IS_STORE (service)) ||
		    (!is_incoming && CAMEL_IS_TRANSPORT (service))) {
			ESource *source = e_source_registry_ref_source (registry, uid);

			if (!source || !e_source_registry_check_enabled (registry, source)) {
				g_clear_object (&source);
				continue;
			}

			g_clear_object (&source);

			add_services = g_slist_prepend (add_services, service);
		}
	}

	add_services = g_slist_sort (add_services, filter_rule_compare_services);

	for (slink = add_services; slink; slink = g_slist_next (slink)) {
		CamelService *service = slink->data;

		gtk_combo_box_text_append (accounts_combo,
			camel_service_get_uid (service),
			camel_service_get_display_name (service));
	}

	g_list_free_full (services, g_object_unref);
	g_slist_free (add_services);

	filter_rule_select_account (GTK_COMBO_BOX (accounts_combo), account_id);

	g_signal_handlers_unblock_matched (accounts_combo, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, filter_rule_accounts_changed_cb, NULL);

	g_free (account_id);
}

static GtkWidget *
filter_rule_create_account_combo (EMFilterRule *fr,
				  EMailSession *session,
				  GtkWidget *source_combo)
{
	GtkComboBox *accounts;
	gulong handler_id;

	g_return_val_if_fail (EM_IS_FILTER_RULE (fr), NULL);
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);
	g_return_val_if_fail (GTK_IS_COMBO_BOX (source_combo), NULL);

	accounts = GTK_COMBO_BOX (gtk_combo_box_text_new ());
	g_object_set_data_full (G_OBJECT (accounts), "e-mail-session", g_object_ref (session), g_object_unref);

	g_signal_connect (source_combo, "changed",
		G_CALLBACK (filter_rule_fill_account_combo), accounts);

	handler_id = g_signal_connect (accounts, "changed",
		G_CALLBACK (filter_rule_accounts_changed_cb), fr);

	filter_rule_fill_account_combo (GTK_COMBO_BOX (source_combo), GTK_COMBO_BOX_TEXT (accounts));

	g_signal_handler_block (accounts, handler_id);
	filter_rule_select_account (accounts, fr->priv->account_uid);
	g_signal_handler_unblock (accounts, handler_id);

	return GTK_WIDGET (accounts);
}

static GtkWidget *
get_widget (EFilterRule *fr,
            ERuleContext *rc)
{
	GtkWidget *widget, *add, *label;
	GtkWidget *inframe, *w;
	GtkWidget *scrolledwindow;
	GtkGrid *hgrid, *parts_grid;
	GtkAdjustment *hadj, *vadj;
	GList *link;
	EFilterPart *part;
	struct _rule_data *data;
	EMFilterRule *ff = (EMFilterRule *) fr;
	gchar *msg;

	widget = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		get_widget (fr, rc);

	g_warn_if_fail (GTK_IS_GRID (widget));

	label = gtk_label_new_with_mnemonic (_("Rul_e type:"));
	w = gtk_combo_box_text_new ();

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), w);

	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (w), E_FILTER_SOURCE_INCOMING, _("Incoming"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (w), E_FILTER_SOURCE_OUTGOING, _("Outgoing"));

	gtk_combo_box_set_active_id (GTK_COMBO_BOX (w), fr->source);

	g_signal_connect (w, "changed", G_CALLBACK (filter_type_changed_cb), fr);

	hgrid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (hgrid, 12);

	gtk_grid_attach (hgrid, label, 0, 0, 1, 1);
	gtk_grid_attach_next_to (hgrid, w, label, GTK_POS_RIGHT, 1, 1);

	label = gtk_label_new_with_mnemonic (_("_For Account:"));
	w = filter_rule_create_account_combo (ff, em_filter_context_get_session (EM_FILTER_CONTEXT (rc)), w);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), w);

	gtk_grid_attach (hgrid, label, 2, 0, 1, 1);
	gtk_grid_attach_next_to (hgrid, w, label, GTK_POS_RIGHT, 1, 1);

	gtk_grid_insert_row (GTK_GRID (widget), 1);
	gtk_grid_attach (GTK_GRID (widget), GTK_WIDGET (hgrid), 0, 1, 1, 1);

	/* and now for the action area */
	msg = g_strdup_printf ("<b>%s</b>", _("Then"));
	label = gtk_label_new (msg);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_container_add (GTK_CONTAINER (widget), label);
	g_free (msg);

	hgrid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (hgrid, 3);
	gtk_widget_set_hexpand (GTK_WIDGET (hgrid), TRUE);
	gtk_widget_set_halign (GTK_WIDGET (hgrid), GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (hgrid));

	label = gtk_label_new ("");
	gtk_grid_attach (hgrid, label, 0, 0, 1, 1);

	inframe = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (inframe), 6);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (inframe), GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_hexpand (inframe, TRUE);
	gtk_widget_set_halign (inframe, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (inframe, TRUE);
	gtk_widget_set_valign (inframe, GTK_ALIGN_FILL);

	gtk_grid_attach_next_to (hgrid, inframe, label, GTK_POS_RIGHT, 1, 1);

	parts_grid = GTK_GRID (gtk_grid_new ());
	g_object_set (G_OBJECT (parts_grid),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	data = g_malloc0 (sizeof (struct _rule_data));
	data->f = (EMFilterContext *) rc;
	data->fr = fr;
	data->parts_grid = parts_grid;
	data->drag_widget = NULL;
	data->n_rows = 0;

	/* only set to automatically clean up the memory */
	g_object_set_data_full ((GObject *) hgrid, "data", data, g_free);

	for (link = ff->priv->actions; link; link = g_list_next (link)) {
		part = link->data;
		d (printf ("adding action %s\n", part->title));
		w = get_rule_part_widget ((EMFilterContext *) rc, part, fr);

		attach_rule (w, data, part, data->n_rows);
		data->n_rows++;
	}

	hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 1.0, 1.0 ,1.0, 1.0));
	vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 1.0, 1.0 ,1.0, 1.0));
	scrolledwindow = gtk_scrolled_window_new (hadj, vadj);

	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwindow),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (
		GTK_CONTAINER (scrolledwindow), GTK_WIDGET (parts_grid));

	gtk_widget_set_hexpand (scrolledwindow, TRUE);
	gtk_widget_set_halign (scrolledwindow, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (scrolledwindow, TRUE);
	gtk_widget_set_valign (scrolledwindow, GTK_ALIGN_FILL);

	gtk_container_add (GTK_CONTAINER (inframe), scrolledwindow);

	hgrid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (hgrid, 3);

	add = e_dialog_button_new_with_icon ("list-add", _("Add Ac_tion"));
	g_signal_connect (
		add, "clicked",
		G_CALLBACK (more_parts), data);
	gtk_grid_attach (hgrid, add, 0, 0, 1, 1);

	gtk_container_add (GTK_CONTAINER (inframe), GTK_WIDGET (hgrid));

	g_object_set_data (G_OBJECT (add), "scrolled-window", scrolledwindow);

	e_signal_connect_notify_swapped (
		vadj, "notify::upper",
		G_CALLBACK (e_util_ensure_scrolled_window_height), scrolledwindow);

	g_signal_connect (scrolledwindow, "map", G_CALLBACK (e_util_ensure_scrolled_window_height), NULL);

	gtk_widget_show_all (widget);

	return widget;
}

GList *
em_filter_rule_get_actions (EMFilterRule *rule)
{
	g_return_val_if_fail (EM_IS_FILTER_RULE (rule), NULL);

	return rule->priv->actions;
}

const gchar *
em_filter_rule_get_account_uid (EMFilterRule *rule)
{
	g_return_val_if_fail (EM_IS_FILTER_RULE (rule), NULL);

	return rule->priv->account_uid;
}

void
em_filter_rule_set_account_uid (EMFilterRule *rule,
				const gchar *account_uid)
{
	g_return_if_fail (EM_IS_FILTER_RULE (rule));

	if (g_strcmp0 (account_uid, rule->priv->account_uid) == 0)
		return;

	g_clear_pointer (&rule->priv->account_uid, g_free);
	rule->priv->account_uid = (account_uid && *account_uid) ? g_strdup (account_uid) : NULL;

	e_filter_rule_emit_changed (E_FILTER_RULE (rule));
}
