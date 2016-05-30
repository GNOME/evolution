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

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "e-content-editor.h"
#include "e-marshal.h"

G_DEFINE_INTERFACE (EContentEditorFindController, e_content_editor_find_controller, G_TYPE_OBJECT)

enum {
	FOUND_TEXT,
	FAILED_TO_FIND_TEXT,
	COUNTED_MATCHES,
	REPLACE_ALL_FINISHED,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
e_content_editor_find_controller_default_init (EContentEditorFindControllerInterface *iface)
{
	/**
	 * EContentEditorFindController:found-text
	 *
	 * Emitted when a text is found.
	 */
	signals[FOUND_TEXT] = g_signal_new (
		"found-text",
		E_TYPE_CONTENT_EDITOR_FIND_CONTROLLER,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (
			EContentEditorFindControllerInterface,
			found_text),
		NULL, NULL,
		g_cclosure_marshal_VOID__UINT,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);

	/**
	 * EContentEditorFindController:failed-to-find-text
	 *
	 * XXX.
	 */
	signals[FAILED_TO_FIND_TEXT] = g_signal_new (
		"load-finished",
		E_TYPE_CONTENT_EDITOR_FIND_CONTROLLER,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (
			EContentEditorFindControllerInterface,
			failed_to_find_text),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EContentEditorFindController:counted-matches
	 *
	 * XXX.
	 */
	signals[COUNTED_MATCHES] = g_signal_new (
		"counted-matches",
		E_TYPE_CONTENT_EDITOR_FIND_CONTROLLER,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (
			EContentEditorFindControllerInterface,
			counted_matches),
		NULL, NULL,
		g_cclosure_marshal_VOID__UINT,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);

	/**
	 * EContentEditorFindController:replace-all-finished
	 *
	 * XXX.
	 */
	signals[REPLACE_ALL_FINISHED] = g_signal_new (
		"replace-all-finished",
		E_TYPE_CONTENT_EDITOR_FIND_CONTROLLER,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (
			EContentEditorFindControllerInterface,
			replace_all_finished),
		NULL, NULL,
		g_cclosure_marshal_VOID__UINT,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);
}

void
e_content_editor_find_controller_search_next (EContentEditorFindController *controller)
{
	EContentEditorFindControllerInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR_FIND_CONTROLLER (controller));

	iface = E_CONTENT_EDITOR_FIND_CONTROLLER_GET_IFACE (controller);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->search_next != NULL);

	iface->search_next (controller);
}

void
e_content_editor_find_controller_search (EContentEditorFindController *controller,
                                         const gchar *text,
                                         EContentEditorFindControllerFlags flags)
{
	EContentEditorFindControllerInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR_FIND_CONTROLLER (controller));
	g_return_if_fail (text && *text);

	iface = E_CONTENT_EDITOR_FIND_CONTROLLER_GET_IFACE (controller);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->search != NULL);

	iface->search (controller, text, flags);
}

void
e_content_editor_find_controller_search_finish (EContentEditorFindController *controller)
{
	EContentEditorFindControllerInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR_FIND_CONTROLLER (controller));

	iface = E_CONTENT_EDITOR_FIND_CONTROLLER_GET_IFACE (controller);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->search_finish != NULL);

	iface->search_finish (controller);
}

void
e_content_editor_find_controller_count_matches (EContentEditorFindController *controller,
                                                const gchar *text,
                                                EContentEditorFindControllerFlags flags)
{
	EContentEditorFindControllerInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR_FIND_CONTROLLER (controller));
	g_return_if_fail (text && *text);

	iface = E_CONTENT_EDITOR_FIND_CONTROLLER_GET_IFACE (controller);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->count_matches != NULL);

	iface->count_matches (controller, text, flags);
}

void
e_content_editor_find_controller_replace_all (EContentEditorFindController *controller,
                                              const gchar *text,
                                              const gchar *replacement,
                                              EContentEditorFindControllerFlags flags)
{
	EContentEditorFindControllerInterface *iface;

	g_return_if_fail (E_IS_CONTENT_EDITOR_FIND_CONTROLLER (controller));
	g_return_if_fail (text && *text);

	iface = E_CONTENT_EDITOR_FIND_CONTROLLER_GET_IFACE (controller);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->replace_all != NULL);

	iface->replace_all (controller, text, replacement, flags);
}
