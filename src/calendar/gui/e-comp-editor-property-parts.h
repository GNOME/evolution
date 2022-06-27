/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_COMP_EDITOR_PROPERTY_PARTS_H
#define E_COMP_EDITOR_PROPERTY_PARTS_H

#include <libecal/libecal.h>
#include <e-util/e-util.h>
#include <calendar/gui/e-comp-editor-property-part.h>

G_BEGIN_DECLS

ECompEditorPropertyPart *
		e_comp_editor_property_part_summary_new		(EFocusTracker *focus_tracker);
ECompEditorPropertyPart *
		e_comp_editor_property_part_location_new	(EFocusTracker *focus_tracker);
ECompEditorPropertyPart *
		e_comp_editor_property_part_categories_new	(EFocusTracker *focus_tracker);
ECompEditorPropertyPart *
		e_comp_editor_property_part_description_new	(EFocusTracker *focus_tracker);
ECompEditorPropertyPart *
		e_comp_editor_property_part_url_new		(EFocusTracker *focus_tracker);
ECompEditorPropertyPart *
		e_comp_editor_property_part_dtstart_new		(const gchar *label,
								 gboolean date_only,
								 gboolean allow_no_date_set,
								 gboolean allow_shorten_time);
ECompEditorPropertyPart *
		e_comp_editor_property_part_dtend_new		(const gchar *label,
								 gboolean date_only,
								 gboolean allow_no_date_set);
ECompEditorPropertyPart *
		e_comp_editor_property_part_due_new		(gboolean date_only,
								 gboolean allow_no_date_set);
ECompEditorPropertyPart *
		e_comp_editor_property_part_completed_new	(gboolean date_only,
								 gboolean allow_no_date_set);
ECompEditorPropertyPart *
		e_comp_editor_property_part_classification_new	(void);
ECompEditorPropertyPart *
		e_comp_editor_property_part_status_new		(ICalComponentKind kind);
ECompEditorPropertyPart *
		e_comp_editor_property_part_priority_new	(void);
ECompEditorPropertyPart *
		e_comp_editor_property_part_percentcomplete_new	(void);
ECompEditorPropertyPart *
		e_comp_editor_property_part_timezone_new	(void);
ECompEditorPropertyPart *
		e_comp_editor_property_part_transparency_new	(void);
ECompEditorPropertyPart *
		e_comp_editor_property_part_color_new		(void);
ECompEditorPropertyPart *
		e_comp_editor_property_part_estimated_duration_new
								(void);

G_END_DECLS

#endif /* E_COMP_EDITOR_PROPERTY_PARTS_H */
