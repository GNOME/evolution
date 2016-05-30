/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-webkit-content-editor-find-controller.h"
#include "e-webkit-content-editor.h"

#include <e-util/e-util.h>
#include <string.h>

#define E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER, EWebKitContentEditorFindControllerPrivate))

enum {
	PROP_0,
	PROP_WEBKIT_CONTENT_EDITOR
};

struct _EWebKitContentEditorFindControllerPrivate {
	GWeakRef wk_editor;

	WebKitFindController *find_controller;

	gboolean performing_replace_all;
	guint replace_all_match_count;
	gchar *spell_check_replacement;

	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;
	gulong counted_matches_handler_id;
};

static void content_editor_content_editor_find_controller_init (EContentEditorFindControllerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EWebKitContentEditorFindController,
	e_webkit_content_editor_find_controller,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_CONTENT_EDITOR_FIND_CONTROLLER,
		content_editor_content_editor_find_controller_init));

static EWebKitContentEditor *
content_editor_find_controller_ref_editor (EWebKitContentEditorFindController *wk_controller)
{
	return g_weak_ref_get (&wk_controller->priv->wk_editor);
}

static void
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      EWebKitContentEditorFindController *wk_controller)
{
	if (wk_controller->priv->performing_replace_all) {
		EWebKitContentEditor *wk_editor;

		if (wk_controller->priv->replace_all_match_count == 0)
			wk_controller->priv->replace_all_match_count = match_count;

		/* Repeatedly search for 'word', then replace selection by
		 * 'replacement'. Repeat until there's at least one occurrence of
		 * 'word' in the document */
		wk_editor = content_editor_find_controller_ref_editor (wk_controller);

		e_content_editor_insert_content (
			E_CONTENT_EDITOR (wk_editor),
			wk_controller->priv->spell_check_replacement,
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN);

		g_object_unref (wk_editor);

		webkit_find_controller_search_next (find_controller);
	} else
		g_signal_emit_by_name (
			E_CONTENT_EDITOR_FIND_CONTROLLER (wk_controller),
			"found-text",
			0,
			match_count);
}

static void
webkit_content_editor_find_controller_search_finish (EContentEditorFindController *controller)
{
	EWebKitContentEditorFindController *wk_controller;

	wk_controller = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER (controller);

	if (wk_controller->priv->performing_replace_all) {
		g_signal_emit_by_name (
			E_CONTENT_EDITOR_FIND_CONTROLLER (controller),
			"replace-all-finished",
			0,
			wk_controller->priv->replace_all_match_count);
		wk_controller->priv->performing_replace_all = FALSE;
		wk_controller->priv->replace_all_match_count = 0;
	}

	webkit_find_controller_search_finish (wk_controller->priv->find_controller);
}

static void
webkit_find_controller_failed_to_find_text_cb (WebKitFindController *find_controller,
                                               EWebKitContentEditorFindController *wk_controller)
{
	if (wk_controller->priv->performing_replace_all) {
		webkit_content_editor_find_controller_search_finish (
			E_CONTENT_EDITOR_FIND_CONTROLLER (wk_controller));
	} else
		g_signal_emit_by_name (
			E_CONTENT_EDITOR_FIND_CONTROLLER (wk_controller),
			"failed-to-find-text",
			0);
}

static void
webkit_find_controller_counted_matches_cb (WebKitFindController *find_controller,
                                           guint match_count,
                                           EWebKitContentEditorFindController *wk_controller)
{
	g_signal_emit_by_name (
		E_CONTENT_EDITOR_FIND_CONTROLLER (wk_controller),
		"counted-matches",
		0,
		match_count);
}

static WebKitFindOptions
process_find_flags (EContentEditorFindControllerFlags flags)
{
	WebKitFindOptions options = 0;

	if (flags & E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE)
		options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

	if (flags & E_CONTENT_EDITOR_FIND_WRAP_AROUND)
		options |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;

	if (flags & E_CONTENT_EDITOR_FIND_BACKWARDS)
		options |= WEBKIT_FIND_OPTIONS_BACKWARDS;

	return options;
}

static void
webkit_content_editor_find_controller_search (EContentEditorFindController *controller,
                                              const gchar *text,
                                              EContentEditorFindControllerFlags flags)
{
	EWebKitContentEditorFindController *wk_controller;

	wk_controller = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER (controller);

	webkit_find_controller_search (
		wk_controller->priv->find_controller,
		text,
		process_find_flags (flags),
		G_MAXUINT);
}

static void
webkit_content_editor_find_controller_search_next (EContentEditorFindController *controller)
{
	EWebKitContentEditorFindController *wk_controller;

	wk_controller = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER (controller);

	webkit_find_controller_search_next (wk_controller->priv->find_controller);
}

static void
webkit_content_editor_find_controller_count_matches (EContentEditorFindController *controller,
                                                     const gchar *text,
                                                     EContentEditorFindControllerFlags flags)
{
	EWebKitContentEditorFindController *wk_controller;

	wk_controller = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER (controller);

	webkit_find_controller_count_matches (
		wk_controller->priv->find_controller,
		text,
		process_find_flags (flags),
		G_MAXUINT);
}

static void
webkit_content_editor_find_controller_replace_all (EContentEditorFindController *controller,
                                                   const gchar *text,
                                                   const gchar *replacement,
                                                   EContentEditorFindControllerFlags flags)
{
	EWebKitContentEditorFindController *wk_controller;

	wk_controller = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER (controller);

	if (wk_controller->priv->spell_check_replacement)
		g_free (wk_controller->priv->spell_check_replacement);
	wk_controller->priv->spell_check_replacement = g_strdup (replacement);

	wk_controller->priv->performing_replace_all = TRUE;

	webkit_find_controller_search (
		wk_controller->priv->find_controller, text, process_find_flags (flags), G_MAXUINT);
}

static void
webkit_content_editor_find_controller_dispose (GObject *object)
{
	EWebKitContentEditorFindControllerPrivate *priv;

	priv = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_GET_PRIVATE (object);

	if (priv->found_text_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->found_text_handler_id);
		priv->found_text_handler_id = 0;
	}

	if (priv->failed_to_find_text_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->failed_to_find_text_handler_id);
		priv->failed_to_find_text_handler_id = 0;
	}

	if (priv->counted_matches_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->find_controller,
			priv->counted_matches_handler_id);
		priv->counted_matches_handler_id = 0;
	}

	g_weak_ref_set (&priv->wk_editor, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_webkit_content_editor_find_controller_parent_class)->dispose (object);
}

static void
webkit_content_editor_find_controller_finalize (GObject *object)
{
	EWebKitContentEditorFindControllerPrivate *priv;

	priv = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_GET_PRIVATE (object);

	g_free (priv->spell_check_replacement);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_webkit_content_editor_find_controller_parent_class)->finalize (object);
}

static void
find_controller_set_content_editor (EWebKitContentEditorFindController *wk_controller,
                                    EWebKitContentEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_CONTENT_EDITOR (wk_editor));

	g_weak_ref_set (&wk_controller->priv->wk_editor, wk_editor);

	wk_controller->priv->find_controller =
		webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (wk_editor));

	wk_controller->priv->found_text_handler_id = g_signal_connect (
		wk_controller->priv->find_controller, "found-text",
		(GCallback) webkit_find_controller_found_text_cb, wk_controller);

	wk_controller->priv->failed_to_find_text_handler_id = g_signal_connect (
		wk_controller->priv->find_controller, "failed-to-find-text",
		(GCallback) webkit_find_controller_failed_to_find_text_cb, wk_controller);

	wk_controller->priv->counted_matches_handler_id = g_signal_connect (
		wk_controller->priv->find_controller, "counted-matches",
		(GCallback) webkit_find_controller_counted_matches_cb, wk_controller);
}

static void
webkit_content_editor_find_controller_set_property (GObject *object,
                                                    guint property_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEBKIT_CONTENT_EDITOR:
			find_controller_set_content_editor (
				E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_webkit_content_editor_find_controller_class_init (EWebKitContentEditorFindControllerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EWebKitContentEditorFindControllerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = webkit_content_editor_find_controller_set_property;
	object_class->dispose = webkit_content_editor_find_controller_dispose;
	object_class->finalize = webkit_content_editor_find_controller_finalize;

	g_object_class_install_property (
		object_class,
		PROP_WEBKIT_CONTENT_EDITOR,
		g_param_spec_object (
			"webkit-content-editor",
			NULL,
			NULL,
			E_TYPE_WEBKIT_CONTENT_EDITOR,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_webkit_content_editor_find_controller_init (EWebKitContentEditorFindController *wk_controller)
{
	wk_controller->priv = E_WEBKIT_CONTENT_EDITOR_FIND_CONTROLLER_GET_PRIVATE (wk_controller);

	wk_controller->priv->spell_check_replacement = NULL;
	wk_controller->priv->performing_replace_all = FALSE;
	wk_controller->priv->replace_all_match_count = 0;
}

static void
content_editor_content_editor_find_controller_init (EContentEditorFindControllerInterface *iface)
{
	iface->search = webkit_content_editor_find_controller_search;
	iface->search_next = webkit_content_editor_find_controller_search_next;
	iface->search_finish = webkit_content_editor_find_controller_search_finish;
	iface->count_matches = webkit_content_editor_find_controller_count_matches;
	iface->replace_all = webkit_content_editor_find_controller_replace_all;
}
