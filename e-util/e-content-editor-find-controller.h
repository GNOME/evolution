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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONTENT_EDITOR_FIND_CONTROLLER_H
#define E_CONTENT_EDITOR_FIND_CONTROLLER_H

#include <glib-object.h>

#include <camel/camel.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_CONTENT_EDITOR_FIND_CONTROLLER e_content_editor_find_controller_get_type ()
G_DECLARE_INTERFACE (EContentEditorFindController, e_content_editor_find_controller, E, CONTENT_EDITOR_FIND_CONTROLLER, GObject)

typedef enum {
	E_CONTENT_EDITOR_FIND_BACKWARDS,
	E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE,
	E_CONTENT_EDITOR_FIND_WRAP_AROUND,
} EContentEditorFindControllerFlags;

struct _EContentEditorFindControllerInterface {
	GTypeInterface parent_interface;

	void		(*search)			(EContentEditorFindController *controller,
							 const gchar *text,
							 EContentEditorFindControllerFlags flags);

	void		(*search_next)			(EContentEditorFindController *controller);

	void		(*search_finish)		(EContentEditorFindController *controller);

	void		(*count_matches)		(EContentEditorFindController *controller,
							 const gchar *text,
							 EContentEditorFindControllerFlags flags);

	void		(*replace_all)			(EContentEditorFindController *controller,
							 const gchar *text,
							 const gchar *replacement,
							 EContentEditorFindControllerFlags flags);

	/* Signals */
	void		(*found_text)			(EContentEditorFindController *controller,
							 guint match_count);

	void		(*failed_to_find_text)		(EContentEditorFindController *controller);

	void		(*counted_matches)		(EContentEditorFindController *controller,
							 guint match_count);

	void		(*replace_all_finished)		(EContentEditorFindController *controller,
							 guint match_count);
};

void		e_content_editor_find_controller_search
						(EContentEditorFindController *controller,
						 const gchar *text,
						 EContentEditorFindControllerFlags flags);

void		e_content_editor_find_controller_search_next
						(EContentEditorFindController *controller);

void		e_content_editor_find_controller_search_finish
						(EContentEditorFindController *controller);

void		e_content_editor_find_controller_count_matches
						(EContentEditorFindController *controller,
						 const gchar *text,
						 EContentEditorFindControllerFlags flags);

void		e_content_editor_find_controller_replace_all
						(EContentEditorFindController *controller,
						 const gchar *text,
						 const gchar *replacement,
						 EContentEditorFindControllerFlags flags);

G_END_DECLS

#endif /* E_CONTENT_EDITOR_FIND_CONTROLLER_H */
