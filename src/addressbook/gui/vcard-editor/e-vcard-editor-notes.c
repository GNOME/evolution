/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-vcard-editor-notes.h"

struct _EVCardEditorNotes {
	GtkScrolledWindow parent_object;

	GtkTextView *text_view; /* not owned */

	gboolean updating;
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EVCardEditorNotes, e_vcard_editor_notes, GTK_TYPE_SCROLLED_WINDOW)

static void
eve_notes_emit_changed (EVCardEditorNotes *self)
{
	if (!self->updating)
		g_signal_emit (self, signals[CHANGED], 0, NULL);
}

static void
e_vcard_editor_notes_grab_focus (GtkWidget *widget)
{
	EVCardEditorNotes *self = E_VCARD_EDITOR_NOTES (widget);

	if (self->text_view)
		gtk_widget_grab_focus (GTK_WIDGET (self->text_view));
}

static void
e_vcard_editor_notes_constructed (GObject *object)
{
	EVCardEditorNotes *self = E_VCARD_EDITOR_NOTES (object);
	GtkWidget *widget;

	G_OBJECT_CLASS (e_vcard_editor_notes_parent_class)->constructed (object);

	g_object_set (self,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		"kinetic-scrolling", TRUE,
		"min-content-width", 200,
		"min-content-height", 250,
		"propagate-natural-height", TRUE,
		"propagate-natural-width", TRUE,
		NULL);

	widget = gtk_text_view_new ();
	g_object_set (widget,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"accepts-tab", FALSE,
		"wrap-mode", GTK_WRAP_WORD_CHAR,
		"left-margin", 6,
		"right-margin", 6,
		"top-margin", 6,
		"bottom-margin", 6,
		NULL);

	gtk_container_add (GTK_CONTAINER (self), widget);

	self->text_view = GTK_TEXT_VIEW (widget);

	gtk_widget_show_all (GTK_WIDGET (self));

	g_signal_connect_swapped (gtk_text_view_get_buffer (self->text_view), "changed",
		G_CALLBACK (eve_notes_emit_changed), self);
}

static void
e_vcard_editor_notes_dispose (GObject *object)
{
	EVCardEditorNotes *self = E_VCARD_EDITOR_NOTES (object);

	self->text_view = NULL;

	G_OBJECT_CLASS (e_vcard_editor_notes_parent_class)->dispose (object);
}

static void
e_vcard_editor_notes_class_init (EVCardEditorNotesClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_vcard_editor_notes_constructed;
	object_class->dispose = e_vcard_editor_notes_dispose;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->grab_focus = e_vcard_editor_notes_grab_focus;

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
e_vcard_editor_notes_init (EVCardEditorNotes *self)
{
}

GtkWidget *
e_vcard_editor_notes_new (void)
{
	return g_object_new (E_TYPE_VCARD_EDITOR_NOTES, NULL);
}

void
e_vcard_editor_notes_set_text (EVCardEditorNotes *self,
			       const gchar *text)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (E_IS_VCARD_EDITOR_NOTES (self));

	if (!self->text_view)
		return;

	self->updating = TRUE;

	buffer = gtk_text_view_get_buffer (self->text_view);
	gtk_text_buffer_set_text (buffer, text ? text : "", -1);

	self->updating = FALSE;
}

gchar *
e_vcard_editor_notes_get_text (EVCardEditorNotes *self)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	g_return_val_if_fail (E_IS_VCARD_EDITOR_NOTES (self), NULL);

	buffer = gtk_text_view_get_buffer (self->text_view);
	gtk_text_buffer_get_bounds (buffer, &start, &end);

	return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}
