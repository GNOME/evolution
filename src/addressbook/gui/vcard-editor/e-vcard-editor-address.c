/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-vcard-editor-address.h"

struct _EVCardEditorAddress {
	GtkGrid parent_object;

	GtkComboBox *type_combo; /* not owned */
	GtkTextView *address_text_view; /* not owned */
	GtkEntry *pobox_entry; /* not owned */
	GtkEntry *city_entry; /* not owned */
	GtkEntry *zip_entry; /* not owned */
	GtkEntry *state_entry; /* not owned */
	GtkEntry *country_entry; /* not owned */

	gboolean updating;
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EVCardEditorAddress, e_vcard_editor_address, GTK_TYPE_GRID)

static void
eve_address_emit_changed (EVCardEditorAddress *self)
{
	if (!self->updating)
		g_signal_emit (self, signals[CHANGED], 0, NULL);
}

static void
e_vcard_editor_address_grab_focus (GtkWidget *widget)
{
	EVCardEditorAddress *self = E_VCARD_EDITOR_ADDRESS (widget);

	if (self->type_combo)
		gtk_widget_grab_focus (GTK_WIDGET (self->type_combo));
}

static gboolean
e_vcard_editor_address_focus (GtkWidget *widget,
			      GtkDirectionType direction)
{
	EVCardEditorAddress *self = E_VCARD_EDITOR_ADDRESS (widget);
	GtkWidget *focused;
	gpointer order[7];
	guint ii;

	if (!self->type_combo || (direction != GTK_DIR_TAB_FORWARD && direction != GTK_DIR_TAB_BACKWARD))
		return GTK_WIDGET_CLASS (e_vcard_editor_address_parent_class)->focus (widget, direction);

	focused = gtk_container_get_focus_child (GTK_CONTAINER (self));

	order[0] = self->type_combo;
	order[1] = self->address_text_view;
	order[2] = self->pobox_entry;
	order[3] = self->city_entry;
	order[4] = self->zip_entry;
	order[5] = self->state_entry;
	order[6] = self->country_entry;

	for (ii = 0; ii < G_N_ELEMENTS (order); ii++) {
		if (order[ii] && (focused == (GtkWidget *) order[ii] || gtk_widget_is_focus (order[ii])))
			break;
	}

	if (ii >= G_N_ELEMENTS (order))
		return GTK_WIDGET_CLASS (e_vcard_editor_address_parent_class)->focus (widget, direction);

	if ((direction == GTK_DIR_TAB_FORWARD && ii + 1 >= G_N_ELEMENTS (order)) ||
	    (direction == GTK_DIR_TAB_BACKWARD && ii == 0))
		return FALSE;

	gtk_widget_grab_focus (order[ii + (direction == GTK_DIR_TAB_FORWARD ? 1 : -1)]);

	return TRUE;
}

static void
e_vcard_editor_address_constructed (GObject *object)
{
	EVCardEditorAddress *self = E_VCARD_EDITOR_ADDRESS (object);
	GtkWidget *widget, *label, *scrolled_window;
	GtkComboBoxText *text_combo;
	GtkGrid *grid;

	G_OBJECT_CLASS (e_vcard_editor_address_parent_class)->constructed (object);

	g_object_set (self,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"column-spacing", 4,
		"row-spacing", 4,
		"can-focus", FALSE,
		NULL);

	grid = GTK_GRID (self);

	widget = gtk_combo_box_text_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 4, 1);
	text_combo = GTK_COMBO_BOX_TEXT (widget);
	gtk_combo_box_text_append (text_combo, "work", NC_("addressbook-label", "Work"));
	gtk_combo_box_text_append (text_combo, "home", NC_("addressbook-label", "Home"));
	gtk_combo_box_text_append (text_combo, "other", NC_("addressbook-label", "Other"));

	self->type_combo = GTK_COMBO_BOX (widget);

	gtk_combo_box_set_active_id (self->type_combo, "work");

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled_window,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"can-focus", FALSE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		"kinetic-scrolling", TRUE,
		"propagate-natural-height", TRUE,
		"propagate-natural-width", TRUE,
		"min-content-height", 100,
		NULL);

	label = gtk_label_new_with_mnemonic (_("St_reet:"));
	widget = gtk_text_view_new ();
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		"valign", GTK_ALIGN_START,
		"mnemonic-widget", widget,
		"margin-top", 2,
		NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"accepts-tab", FALSE,
		"left-margin", 6,
		"right-margin", 6,
		"top-margin", 6,
		"bottom-margin", 6,
		NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);
	gtk_grid_attach (grid, label, 0, 1, 1, 1);
	gtk_grid_attach (grid, scrolled_window, 1, 1, 1, 1);

	self->address_text_view = GTK_TEXT_VIEW (widget);

	label = gtk_label_new_with_mnemonic (_("_PO Box:"));
	widget = gtk_entry_new ();
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		"mnemonic-widget", widget,
		NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		NULL);
	gtk_grid_attach (grid, label, 0, 4, 1, 1);
	gtk_grid_attach (grid, widget, 1, 4, 1, 1);

	self->pobox_entry = GTK_ENTRY (widget);

	label = gtk_label_new_with_mnemonic (_("_City:"));
	widget = gtk_entry_new ();
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		"mnemonic-widget", widget,
		NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		NULL);
	gtk_grid_attach (grid, label, 0, 5, 1, 1);
	gtk_grid_attach (grid, widget, 1, 5, 1, 1);

	self->city_entry = GTK_ENTRY (widget);

	label = gtk_label_new_with_mnemonic (_("_Zip/Postal Code:"));
	widget = gtk_entry_new ();
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		"mnemonic-widget", widget,
		NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		NULL);
	gtk_grid_attach (grid, label, 0, 6, 1, 1);
	gtk_grid_attach (grid, widget, 1, 6, 1, 1);

	self->zip_entry = GTK_ENTRY (widget);

	label = gtk_label_new_with_mnemonic (_("Stat_e/Province:"));
	widget = gtk_entry_new ();
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		"mnemonic-widget", widget,
		NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		NULL);
	gtk_grid_attach (grid, label, 0, 7, 1, 1);
	gtk_grid_attach (grid, widget, 1, 7, 1, 1);

	self->state_entry = GTK_ENTRY (widget);

	label = gtk_label_new_with_mnemonic (_("Co_untry:"));
	widget = gtk_entry_new ();
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		"mnemonic-widget", widget,
		NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		NULL);
	gtk_grid_attach (grid, label, 0, 8, 1, 1);
	gtk_grid_attach (grid, widget, 1, 8, 1, 1);

	self->country_entry = GTK_ENTRY (widget);

	gtk_widget_show_all (GTK_WIDGET (self));

	g_signal_connect_swapped (self->type_combo, "changed",
		G_CALLBACK (eve_address_emit_changed), self);
	g_signal_connect_swapped (gtk_text_view_get_buffer (self->address_text_view), "changed",
		G_CALLBACK (eve_address_emit_changed), self);
	g_signal_connect_swapped (self->pobox_entry, "changed",
		G_CALLBACK (eve_address_emit_changed), self);
	g_signal_connect_swapped (self->city_entry, "changed",
		G_CALLBACK (eve_address_emit_changed), self);
	g_signal_connect_swapped (self->zip_entry, "changed",
		G_CALLBACK (eve_address_emit_changed), self);
	g_signal_connect_swapped (self->state_entry, "changed",
		G_CALLBACK (eve_address_emit_changed), self);
	g_signal_connect_swapped (self->country_entry, "changed",
		G_CALLBACK (eve_address_emit_changed), self);
}

static void
e_vcard_editor_address_dispose (GObject *object)
{
	EVCardEditorAddress *self = E_VCARD_EDITOR_ADDRESS (object);

	self->type_combo = NULL;
	self->address_text_view = NULL;
	self->pobox_entry = NULL;
	self->city_entry = NULL;
	self->zip_entry = NULL;
	self->state_entry = NULL;
	self->country_entry = NULL;

	G_OBJECT_CLASS (e_vcard_editor_address_parent_class)->dispose (object);
}

static void
e_vcard_editor_address_class_init (EVCardEditorAddressClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_vcard_editor_address_constructed;
	object_class->dispose = e_vcard_editor_address_dispose;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->grab_focus = e_vcard_editor_address_grab_focus;
	widget_class->focus = e_vcard_editor_address_focus;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_vcard_editor_address_init (EVCardEditorAddress *self)
{
}

GtkWidget *
e_vcard_editor_address_new (void)
{
	return g_object_new (E_TYPE_VCARD_EDITOR_ADDRESS, NULL);
}

const gchar *
e_vcard_editor_address_get_address_type (EVCardEditorAddress *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ADDRESS (self), NULL);

	if (!self->type_combo)
		return NULL;

	return gtk_combo_box_get_active_id (self->type_combo);
}

void
e_vcard_editor_address_fill_widgets (EVCardEditorAddress *self,
				     const gchar *type,
				     const EContactAddress *addr)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_ADDRESS (self));

	self->updating = TRUE;

	if (type && g_ascii_strcasecmp (type, "work") == 0)
		gtk_combo_box_set_active_id (self->type_combo, "work");
	else if (type && g_ascii_strcasecmp (type, "home") == 0)
		gtk_combo_box_set_active_id (self->type_combo, "home");
	else
		gtk_combo_box_set_active_id (self->type_combo, "other");

	if (addr) {
		GtkTextBuffer *text_buffer;
		GtkTextIter iter_end, iter_start;

		text_buffer = gtk_text_view_get_buffer (self->address_text_view);
		gtk_text_buffer_set_text (text_buffer, addr->street ? addr->street : "", -1);

		gtk_text_buffer_get_end_iter (text_buffer, &iter_end);
		if (addr->ext && *addr->ext) {
			gtk_text_buffer_insert (text_buffer, &iter_end, "\n", -1);
			gtk_text_buffer_insert (text_buffer, &iter_end, addr->ext, -1);
		} else {
			gtk_text_buffer_insert (text_buffer, &iter_end, "", -1);
		}
		gtk_text_buffer_get_iter_at_line (text_buffer, &iter_start, 0);
		gtk_text_buffer_place_cursor (text_buffer, &iter_start);
	}

	gtk_entry_set_text (self->pobox_entry, addr && addr->po ? addr->po : "");
	gtk_entry_set_text (self->city_entry, addr && addr->locality ? addr->locality : "");
	gtk_entry_set_text (self->zip_entry, addr && addr->code ? addr->code : "");
	gtk_entry_set_text (self->state_entry, addr && addr->region ? addr->region : "");
	gtk_entry_set_text (self->country_entry, addr && addr->country ? addr->country : "");

	self->updating = FALSE;
}

gboolean
e_vcard_editor_address_fill_addr (EVCardEditorAddress *self,
				  gchar **out_type,
				  EContactAddress **out_addr,
				  gchar **out_error_message,
				  GtkWidget **out_error_widget)
{
	gchar *pobox, *city, *zip, *state, *country, *street = NULL, *ext = NULL;
	GtkTextBuffer *text_buffer;
	GtkTextIter iter_1, iter_2;

	g_return_val_if_fail (E_IS_VCARD_EDITOR_ADDRESS (self), FALSE);
	g_return_val_if_fail (out_type != NULL, FALSE);
	g_return_val_if_fail (out_addr != NULL, FALSE);

	*out_type = NULL;
	*out_addr = NULL;

	text_buffer = gtk_text_view_get_buffer (self->address_text_view);
	gtk_text_buffer_get_start_iter (text_buffer, &iter_1);

	/* Skip blank lines */
	while (gtk_text_iter_get_chars_in_line (&iter_1) < 1 &&
	       !gtk_text_iter_is_end (&iter_1))
		gtk_text_iter_forward_line (&iter_1);

	if (!gtk_text_iter_is_end (&iter_1)) {
		iter_2 = iter_1;
		gtk_text_iter_forward_to_line_end (&iter_2);

		/* Extract street (first line of text) */
		street = g_strstrip (gtk_text_iter_get_text (&iter_1, &iter_2));

		iter_1 = iter_2;
		gtk_text_iter_forward_line (&iter_1);

		if (!gtk_text_iter_is_end (&iter_1)) {
			gtk_text_iter_forward_to_end (&iter_2);

			/* Extract extended address (remaining lines of text) */
			ext = g_strstrip (gtk_text_iter_get_text (&iter_1, &iter_2));
		}
	}

	pobox = g_strstrip (g_strdup (gtk_entry_get_text (self->pobox_entry)));
	city = g_strstrip (g_strdup (gtk_entry_get_text (self->city_entry)));
	zip = g_strstrip (g_strdup (gtk_entry_get_text (self->zip_entry)));
	state = g_strstrip (g_strdup (gtk_entry_get_text (self->state_entry)));
	country = g_strstrip (g_strdup (gtk_entry_get_text (self->country_entry)));

	if ((pobox && *pobox) || (city && *city) || (zip && *zip) || (state && *state) ||
	    (country && *country) || (street && *street) || (ext && *ext)) {
		*out_type = g_strdup (gtk_combo_box_get_active_id (self->type_combo));
		*out_addr = e_contact_address_new ();

		(*out_addr)->street = g_steal_pointer (&street);
		(*out_addr)->ext = g_steal_pointer (&ext);
		(*out_addr)->po = g_steal_pointer (&pobox);
		(*out_addr)->locality = g_steal_pointer (&city);
		(*out_addr)->code = g_steal_pointer (&zip);
		(*out_addr)->region = g_steal_pointer (&state);
		(*out_addr)->country = g_steal_pointer (&country);
	}

	g_free (pobox);
	g_free (city);
	g_free (zip);
	g_free (state);
	g_free (country);
	g_free (street);
	g_free (ext);

	return TRUE;
}
