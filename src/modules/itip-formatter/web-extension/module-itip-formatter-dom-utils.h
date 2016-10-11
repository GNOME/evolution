/*
 * module-itip-formatter-dom-utils.h
 *
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
 */

#ifndef MODULE_ITIP_FORMATTER_DOM_UTILS_H
#define MODULE_ITIP_FORMATTER_DOM_UTILS_H

#include <webkitdom/webkitdom.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

void		module_itip_formatter_dom_utils_create_dom_bindings
						(WebKitDOMDocument *document,
						 guint64 page_id,
						 const gchar *part_id,
						 GDBusConnection *connection);
void		module_itip_formatter_dom_utils_show_button
						(WebKitDOMDocument *document,
						 const gchar *button_id);
void		module_itip_formatter_dom_utils_enable_button
						(WebKitDOMDocument *document,
						 const gchar *button_id,
						 gboolean enable);
void		module_itip_formatter_dom_utils_input_set_checked
						(WebKitDOMDocument *document,
						 const gchar *input_id,
						 gboolean checked);
gboolean	module_itip_formatter_dom_utils_input_is_checked
						(WebKitDOMDocument *document,
						 const gchar *input_id);
void		module_itip_formatter_dom_utils_show_checkbox
						(WebKitDOMDocument *document,
						 const gchar *id,
						 gboolean show,
						 gboolean update_second);
void		module_itip_formatter_dom_utils_set_buttons_sensitive
						(WebKitDOMDocument *document,
						 gboolean sensitive);
void		module_itip_formatter_dom_utils_set_area_text
						(WebKitDOMDocument *document,
						 const gchar *area_id,
						 const gchar *text);
void		module_itip_formatter_dom_utils_element_set_access_key
						(WebKitDOMDocument *document,
						 const gchar *element_id,
						 const gchar *access_key);
void		module_itip_formatter_dom_utils_element_hide_child_nodes
						(WebKitDOMDocument *document,
						 const gchar *element_id);
void		module_itip_formatter_dom_utils_enable_select
						(WebKitDOMDocument *document,
						 const gchar *select_id,
						 gboolean enabled);
gboolean	module_itip_formatter_dom_utils_select_is_enabled
						(WebKitDOMDocument *document,
						 const gchar *select_id);
gchar *		module_itip_formatter_dom_utils_select_get_value
						(WebKitDOMDocument *document,
						 const gchar *select_id);
void		module_itip_formatter_dom_utils_select_set_selected
						(WebKitDOMDocument *document,
						 const gchar *select_id,
						 const gchar *option);
void		module_itip_formatter_dom_utils_update_times
						(WebKitDOMDocument *document,
						 const gchar *element_id,
						 const gchar *header,
						 const gchar *label);
void		module_itip_formatter_dom_utils_append_info_item_row
						(WebKitDOMDocument *document,
						 const gchar *table_id,
						 const gchar *row_id,
						 const gchar *icon_name,
						 const gchar *message);
void		module_itip_formatter_dom_utils_enable_text_area
						(WebKitDOMDocument *document,
						 const gchar *area_id,
						 gboolean enable);
void		module_itip_formatter_dom_utils_text_area_set_value
						(WebKitDOMDocument *document,
						 const gchar *area_id,
						 const gchar *value);
gchar *		module_itip_formatter_dom_utils_text_area_get_value
						(WebKitDOMDocument *document,
						 const gchar *area_id);
void		module_itip_formatter_dom_utils_rebuild_source_list
						(WebKitDOMDocument *document,
						 const gchar *optgroup_id,
						 const gchar *optgroup_label,
						 const gchar *option_id,
						 const gchar *option_label,
						 gboolean writable);
G_END_DECLS

#endif /* MODULE_ITIP_FORMATTER_DOM_UTILS_H */
