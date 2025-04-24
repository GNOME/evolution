/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"
#include "e-ui-action.h"
#include "e-util-enums.h"

#include "e-ui-parser.h"

/**
 * SECTION: e-ui-parser
 * @include: e-util/e-util.h
 * @short_description: a UI definition parser
 *
 * #EUIParser parses UI definitions for menus, headerbar and toolbar from an XML data.
 * It's associated with an #EUIManager.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.56
 **/

/*
 Example:

 <eui>
   <headerbar id='headerbar-1'>
     <start>
       <item action='open' label_priority='1' icon_only='false' css_classes='suggested-action flat'/>
       <item action='close' label_priority='2'/>
     </start>
     <end>
       <item action='help-about'/>
       <placeholder id='hb1-placeholder'/>
       <item action='quit'/>
       <item action='menu-button' order='999999'/>
     </end>
   </headerbar>
   <menu id='main-menu'>
    "<submenu id='file-menu-id' action='file-menu'>
      "<item action='open'/>
      "<separator/>
      "<placeholder id='custom-file-actions-placeholder'/>
      "<item action='radio1' group='a' text_only='true'/>
      "<item action='radio2' group='a' text_only='true'/>
      "<item action='radio3' group='a' text_only='true'/>
      "<separator/>
      "<item action='close'/>
     </submenu>
     <placeholder id='edit-placeholder'/>
     <submenu action='help-menu'>
       <item action='help-content'/>
       <item action='help-about'/>
     </submenu>
   </menu>
   <menu id='popup-menu' is-popup='true'>
     <item action='item1'/>
     <item action='item2'/>
   </menu>
   <toolbar id='toolbar-1' primary='true'>
     <item action='item1'/>
     <item action='item2' important='true'/>
     <separator/>
     <placeholder id='tb1-placeholder'/>
     <separator/>
     <item action='item3'/>
   </toolbar>
   <accels action='item1'>
     <accel value="&lt;Control&gt;1"/>
     <accel value="&lt;Control&gt;n"/>
   </accels>
 </eui>
*/

static const gchar *
e_ui_element_kind_to_string (EUIElementKind kind)
{
	switch (kind) {
	case E_UI_ELEMENT_KIND_UNKNOWN:
		return "unknown";
	case E_UI_ELEMENT_KIND_ROOT:
		return "eui";
	case E_UI_ELEMENT_KIND_HEADERBAR:
		return "headerbar";
	case E_UI_ELEMENT_KIND_TOOLBAR:
		return "toolbar";
	case E_UI_ELEMENT_KIND_MENU:
		return "menu";
	case E_UI_ELEMENT_KIND_SUBMENU:
		return "submenu";
	case E_UI_ELEMENT_KIND_PLACEHOLDER:
		return "placeholder";
	case E_UI_ELEMENT_KIND_SEPARATOR:
		return "separator";
	case E_UI_ELEMENT_KIND_START:
		return "start";
	case E_UI_ELEMENT_KIND_END:
		return "end";
	case E_UI_ELEMENT_KIND_ITEM:
		return "item";
	}

	return "???";
}

struct _EUIElement {
	EUIElementKind kind;
	gchar *id;
	GPtrArray *children;
	union {
		/* menu properties */
		struct _menu {
			gboolean is_popup;
		} menu;

		/* submenu properties */
		struct _submenu {
			gchar *action;
		} submenu;

		/* headerbar properties */
		struct _headerbar {
			gboolean use_gtk_type;
		} headerbar;

		/* toolbar properties */
		struct _toolbar {
			gboolean primary;
		} toolbar;

		/* item properties */
		struct _item {
			guint label_priority;
			gint order;
			gint icon_only;
			gint text_only;
			gboolean important;
			gchar *css_classes;
			gchar *action;
			gchar *group;
		} item;
	} data;
};

G_DEFINE_BOXED_TYPE (EUIElement, e_ui_element, e_ui_element_copy, e_ui_element_free)

static EUIElement *
e_ui_element_new (EUIElementKind kind,
		  const gchar *id)
{
	EUIElement *self;

	self = g_new0 (EUIElement, 1);
	self->kind = kind;
	self->id = g_strdup (id);

	if (kind == E_UI_ELEMENT_KIND_ITEM) {
		self->data.item.label_priority = G_MAXUINT32;
		self->data.item.order = 0;
		self->data.item.icon_only = G_MAXINT32;
		self->data.item.text_only = G_MAXINT32;
		self->data.item.important = FALSE;
	}

	return self;
}

/**
 * e_ui_element_copy:
 * @src: an #EUIElement to create a copy of
 *
 * Creates a copy of the @src. Free the returned pointer
 * with e_ui_element_free(), when no longer needed.
 * The function does nothing when the @src is NULL.
 *
 * This does not copy the @src children, it only copies the @src itself.
 *
 * Returns: (transfer full) (nullable): copy of the @src, or %NULL, when the @src is %NULL
 *
 * Since: 3.56
 **/
EUIElement *
e_ui_element_copy (const EUIElement *src)
{
	EUIElement *des;

	if (!src)
		return NULL;

	des = e_ui_element_new (src->kind, src->id);

	switch (src->kind) {
	case E_UI_ELEMENT_KIND_HEADERBAR:
		des->data.headerbar.use_gtk_type = src->data.headerbar.use_gtk_type;
		break;
	case E_UI_ELEMENT_KIND_TOOLBAR:
		des->data.toolbar.primary = src->data.toolbar.primary;
		break;
	case E_UI_ELEMENT_KIND_MENU:
		des->data.menu.is_popup = src->data.menu.is_popup;
		break;
	case E_UI_ELEMENT_KIND_SUBMENU:
		des->data.submenu.action = g_strdup (src->data.submenu.action);
		break;
	case E_UI_ELEMENT_KIND_ITEM:
		des->data.item.label_priority = src->data.item.label_priority;
		des->data.item.order = src->data.item.order;
		des->data.item.icon_only = src->data.item.icon_only;
		des->data.item.text_only = src->data.item.text_only;
		des->data.item.important = src->data.item.important;
		des->data.item.css_classes = g_strdup (src->data.item.css_classes);
		des->data.item.action = g_strdup (src->data.item.action);
		des->data.item.group = g_strdup (src->data.item.group);
		break;
	default:
		break;
	}

	return des;
}

/**
 * e_ui_element_free:
 * @self: (nullable) (transfer full): an #EUIElement
 *
 * Frees the @self. The function does nothing when the @self is %NULL.
 *
 * Since: 3.56
 **/
void
e_ui_element_free (EUIElement *self)
{
	if (!self)
		return;

	g_clear_pointer (&self->id, g_free);
	g_clear_pointer (&self->children, g_ptr_array_unref);

	if (self->kind == E_UI_ELEMENT_KIND_SUBMENU) {
		g_clear_pointer (&self->data.submenu.action, g_free);
	} else if (self->kind == E_UI_ELEMENT_KIND_ITEM) {
		g_clear_pointer (&self->data.item.css_classes, g_free);
		g_clear_pointer (&self->data.item.action, g_free);
		g_clear_pointer (&self->data.item.group, g_free);
	}

	g_free (self);
}

/**
 * e_ui_element_new_separator:
 *
 * Creates a new #EUIElement of kind %E_UI_ELEMENT_KIND_SEPARATOR.
 * Free it with e_ui_element_free(), when no longer needed.
 *
 * Returns: (transfer full): a new separator #EUIElement
 *
 * Since: 3.56
 **/
EUIElement *
e_ui_element_new_separator (void)
{
	return e_ui_element_new (E_UI_ELEMENT_KIND_SEPARATOR, NULL);
}

/**
 * e_ui_element_new_for_action:
 * @action: an #EUIAction
 *
 * Creates a new #EUIElement referencing the @action.
 *
 * Returns: (transfer full): a new #EUIElement referencing the @action
 *
 * Since: 3.58
 **/
EUIElement *
e_ui_element_new_for_action (EUIAction *action)
{
	EUIElement *elem;

	g_return_val_if_fail (E_IS_UI_ACTION (action), NULL);

	elem = e_ui_element_new (E_UI_ELEMENT_KIND_ITEM, NULL);
	elem->data.item.action = e_util_strdup_strip (g_action_get_name (G_ACTION (action)));

	return elem;
}

/**
 * e_ui_element_add_child:
 * @self: an #EUIElement to add the child to
 * @child: (transfer full): the child to add
 *
 * Adds a new @child into the @self. The function assumes
 * ownership of the @child.
 *
 * Since: 3.56
 **/
void
e_ui_element_add_child (EUIElement *self,
			EUIElement *child)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (child != NULL);

	if (!self->children)
		self->children = g_ptr_array_new_with_free_func ((GDestroyNotify) e_ui_element_free);

	g_ptr_array_add (self->children, child);
}

/**
 * e_ui_element_remove_child:
 * @self: an #EUIElement
 * @child: a direct child to remove
 *
 * Removes a direct @child from the children of the @self.
 *
 * See e_ui_element_remove_child_by_id().
 *
 * Returns: whether the @child had been removed; if it is, it is also freed
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_remove_child (EUIElement *self,
			   EUIElement *child)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (child != NULL, FALSE);

	if (!self->children)
		return FALSE;

	return g_ptr_array_remove (self->children, child);
}

/**
 * e_ui_element_remove_child_by_id:
 * @self: an #EUIElement
 * @id: a direct child ID to be removed
 *
 * Removes a direct child of the @self with ID @id.
 *
 * Returns: whether the child with the ID @id had been removed; if it is, it is also freed
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_remove_child_by_id (EUIElement *self,
				 const gchar *id)
{
	guint ii;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	if (!self->children)
		return FALSE;

	for (ii = 0; ii < self->children->len; ii++) {
		EUIElement *elem = g_ptr_array_index (self->children, ii);

		if (elem && g_strcmp0 (elem->id, id) == 0) {
			g_ptr_array_remove_index (self->children, ii);
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * e_ui_element_get_kind:
 * @self: an #EUIElement
 *
 * Gets the kind of the @self.
 *
 * Returns: an #EUIElementKind of the @self
 *
 * Since: 3.56
 **/
EUIElementKind
e_ui_element_get_kind (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, E_UI_ELEMENT_KIND_UNKNOWN);
	return self->kind;
}

/**
 * e_ui_element_get_id:
 * @self: an #EUIElement
 *
 * Gets the identifier of the @self.
 *
 * Returns: (nullable): an identifier of the @self
 *
 * Since: 3.56
 **/
const gchar *
e_ui_element_get_id (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->id;
}

/**
 * e_ui_element_get_n_children:
 * @self: an #EUIElement
 *
 * Gets how many children the element has.
 *
 * Returns: count of children of the @self
 *
 * Since: 3.56
 **/
guint
e_ui_element_get_n_children (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, 0);
	return self->children ? self->children->len : 0;
}

/**
 * e_ui_element_get_child:
 * @self: an #EUIElement
 * @index: an index of the child to get
 *
 * Gets a child at @index, which should be less
 * than e_ui_element_get_n_children().
 *
 * Returns: (transfer none) (nullable): a child element at index @index,
 *   or %NULL, when @index is out of bounds
 *
 * Since: 3.56
 **/
EUIElement *
e_ui_element_get_child (EUIElement *self,
			guint index)
{
	g_return_val_if_fail (self != NULL, NULL);

	if (!self->children || index >= self->children->len)
		return NULL;

	return g_ptr_array_index (self->children, index);
}

/**
 * e_ui_element_get_child_by_id:
 * @self: an #EUIElement
 * @id: an identified of the child to get
 *
 * Gets a child by its identifier. It searches the direct
 * children of the @self only.
 *
 * Returns: (transfer none) (nullable): a child element with identifier @id,
 *   or %NULL, when not found
 *
 * Since: 3.56
 **/
EUIElement *
e_ui_element_get_child_by_id (EUIElement *self,
			      const gchar *id)
{
	guint ii;

	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (id != NULL, NULL);

	for (ii = 0; self->children && ii < self->children->len; ii++) {
		EUIElement *adept = g_ptr_array_index (self->children, ii);

		if (g_strcmp0 (adept->id, id) == 0)
			return adept;
	}

	return NULL;
}

static void
e_ui_element_export (const EUIElement *self,
		     GString *str,
		     gint indent,
		     GHashTable *accels)
{
	if (!self)
		return;

	if (indent >= 0)
		g_string_append_printf (str, "%*s", indent * 2, "");

	g_string_append_printf (str, "<%s", e_ui_element_kind_to_string (self->kind));

	if (self->id && self->kind != E_UI_ELEMENT_KIND_START && self->kind != E_UI_ELEMENT_KIND_END)
		e_util_markup_append_escaped (str, " id='%s'", self->id);

	if (self->kind == E_UI_ELEMENT_KIND_MENU) {
		if (self->data.menu.is_popup)
			g_string_append (str, " is-popup='true'");
	} else if (self->kind == E_UI_ELEMENT_KIND_SUBMENU) {
		if (self->data.submenu.action)
			e_util_markup_append_escaped (str, " action='%s'", self->data.submenu.action);
	} else if (self->kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		if (self->data.headerbar.use_gtk_type)
			g_string_append (str, " type='gtk'");
	} else if (self->kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		if (self->data.toolbar.primary)
			g_string_append (str, " primary='true'");
	} else if (self->kind == E_UI_ELEMENT_KIND_ITEM) {
		if (self->data.item.action)
			e_util_markup_append_escaped (str, " action='%s'", self->data.item.action);
		if (self->data.item.group)
			e_util_markup_append_escaped (str, " group='%s'", self->data.item.group);
		if (self->data.item.label_priority != G_MAXUINT32)
			g_string_append_printf (str, " label_priority='%u'", self->data.item.label_priority);
		if (self->data.item.icon_only != G_MAXINT32)
			g_string_append_printf (str, " icon_only='%s'", self->data.item.icon_only ? "true" : "false");
		if (self->data.item.text_only != G_MAXINT32)
			g_string_append_printf (str, " text_only='%s'", self->data.item.text_only ? "true" : "false");
		if (self->data.item.important)
			g_string_append (str, " important='true'");
		if (self->data.item.css_classes)
			g_string_append_printf (str, " css_classes='%s'", self->data.item.css_classes);
		if (self->data.item.order)
			g_string_append_printf (str, " order='%d'", self->data.item.order);
	}

	if (self->children || (accels && g_hash_table_size (accels) > 0)) {
		guint ii;

		g_string_append_c (str, '>');

		if (indent >= 0)
			g_string_append_c (str, '\n');

		for (ii = 0; self->children && ii < self->children->len; ii++) {
			const EUIElement *subelem = g_ptr_array_index (self->children, ii);
			e_ui_element_export (subelem, str, indent >= 0 ? indent + 1 : indent, NULL);
		}

		if (indent >= 0)
			indent++;

		if (indent > 0)
			g_string_append_printf (str, "%*s", indent * 2, "");

		if (accels) {
			GHashTableIter iter;
			gpointer key = NULL, value = NULL;
			guint n_left = g_hash_table_size (accels);

			g_hash_table_iter_init (&iter, accels);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				const gchar *action = key;
				GPtrArray *accels_array = value;

				/* the 'accels_array' can have 0 elements, to unset all accels for the action */
				if (action && *action && accels_array) {
					e_util_markup_append_escaped (str, "<accels action='%s'", action);

					if (accels_array->len > 0) {
						g_string_append_c (str, '>');
						if (indent >= 0)
							g_string_append_c (str, '\n');
						for (ii = 0; ii < accels_array->len; ii++) {
							const gchar *accel = g_ptr_array_index (accels_array, ii);
							if (accel && *accel) {
								if (indent >= 0)
									g_string_append_printf (str, "%*s", (indent + 1) * 2, "");

								e_util_markup_append_escaped (str, "<accel value='%s'/>", accel);

								if (indent >= 0)
									g_string_append_c (str, '\n');
							}
						}
						if (indent > 0)
							g_string_append_printf (str, "%*s", indent * 2, "");
						g_string_append (str, "</accels>");
					} else {
						g_string_append (str, "/>");
					}

					if (indent >= 0) {
						n_left--;
						if (!n_left && indent > 0)
							indent--;
						g_string_append_printf (str, "\n%*s", indent * 2, "");
					}
				}
			}
		}

		if (indent > 0)
			indent--;

		g_string_append_printf (str, "</%s>", e_ui_element_kind_to_string (self->kind));
	} else {
		g_string_append (str, "/>");
	}

	if (indent >= 0)
		g_string_append_c (str, '\n');
}

/* menu element functions */

/**
 * e_ui_element_menu_get_is_popup:
 * @self: an #EUIElement
 *
 * Gets a whether a menu is a popup (context) menu. Such menu hides actions
 * when they are insensitive. The default value is %FALSE.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_MENU.
 *
 * Returns: whether the menu is a popup menu
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_menu_get_is_popup (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_MENU, FALSE);

	return self->data.menu.is_popup;
}

/* submenu element functions */

/**
 * e_ui_element_submenu_get_action:
 * @self: an #EUIElement
 *
 * Gets a submenu action name.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_SUBMENU.
 *
 * Returns: an action name for the submenu element
 *
 * Since: 3.56
 **/
const gchar *
e_ui_element_submenu_get_action (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_SUBMENU, NULL);

	return self->data.submenu.action;
}

/* headerbar element functions */

/**
 * e_ui_element_headerbar_get_use_gtk_type:
 * @self: an #EUIElement
 *
 * Gets whether a #GtkHeaderBar should be created, instead of an #EHeaderBar.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_HEADERBAR.
 *
 * Returns: whether create #GtkHeaderBar
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_headerbar_get_use_gtk_type (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_HEADERBAR, FALSE);

	return self->data.headerbar.use_gtk_type;
}

/* toolbar element functions */

/**
 * e_ui_element_toolbar_get_primary:
 * @self: an #EUIElement
 *
 * Gets whether the toolbar is a primary toolbar.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_TOOLBAR.
 *
 * Returns: whether the toolbar is a primary toolbar
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_toolbar_get_primary (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_TOOLBAR, FALSE);

	return self->data.toolbar.primary;
}

/* item element functions */

/**
 * e_ui_element_item_get_label_priority:
 * @self: an #EUIElement
 *
 * Gets a label priority value for an item element. It is set only
 * for headerbar items.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: a label priority
 *
 * Since: 3.56
 **/
guint
e_ui_element_item_get_label_priority (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, 0);

	return self->data.item.label_priority;
}

/**
 * e_ui_element_item_get_order:
 * @self: an #EUIElement
 *
 * Gets order of the button in the header bar. Items with lower number
 * are added before items with higher number. The default value is zero,
 * meaning use the order as added through the .eui file.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: order index of the header bar item
 *
 * Since: 3.56
 **/
gint
e_ui_element_item_get_order (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, 0);

	return self->data.item.order;
}

/**
 * e_ui_element_item_set_order:
 * @self: an #EUIElement
 * @order: the value to set
 *
 * Sets an order of the button in the header bar. See e_ui_element_item_get_order()
 * for more information.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Since: 3.56
 **/
void
e_ui_element_item_set_order (EUIElement *self,
			     gint order)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM);

	self->data.item.order = order;
}

/**
 * e_ui_element_item_get_icon_only_is_set:
 * @self: an #EUIElement
 *
 * Gets whether an item has set icon-only property. The default value is %FALSE.
 * It is set only for headerbar items. Use e_ui_element_item_get_icon_only()
 * to get the actual value.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: whether the icon-only property is set
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_item_get_icon_only_is_set (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, FALSE);

	return self->data.item.icon_only != G_MAXINT32;
}

/**
 * e_ui_element_item_get_icon_only:
 * @self: an #EUIElement
 *
 * Gets whether an item may show only icon. The default value is %FALSE.
 * It is set only for headerbar items. Use e_ui_element_item_get_icon_only_is_set()
 * to check whether the value had been set.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: whether to show only icon
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_item_get_icon_only (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, FALSE);

	if (!e_ui_element_item_get_icon_only_is_set (self))
		return FALSE;

	return self->data.item.icon_only != 0;
}

/**
 * e_ui_element_item_get_text_only_is_set:
 * @self: an #EUIElement
 *
 * Gets whether an item has set text-only property. The default value is %FALSE.
 * Use e_ui_element_item_get_text_only() to get the actual value.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: whether the text-only property is set
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_item_get_text_only_is_set (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, FALSE);

	return self->data.item.text_only != G_MAXINT32;
}

/**
 * e_ui_element_item_get_text_only:
 * @self: an #EUIElement
 *
 * Gets whether an item may show only text. The default value is %FALSE.
 * Use e_ui_element_item_get_text_only_is_set()
 * to check whether the value had been set.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: whether to show only text
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_item_get_text_only (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, FALSE);

	if (!e_ui_element_item_get_text_only_is_set (self))
		return FALSE;

	return self->data.item.text_only != 0;
}

/**
 * e_ui_element_item_get_important:
 * @self: an #EUIElement
 *
 * Gets whether an item is important. The default value is %FALSE.
 * It's used for items inside toolbars.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: whether the item is important
 *
 * Since: 3.56
 **/
gboolean
e_ui_element_item_get_important (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, FALSE);

	return self->data.item.important;
}

/**
 * e_ui_element_item_get_css_classes:
 * @self: an #EUIElement
 *
 * Gets a space-separated list of CSS classes to add on the item.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: (nullable): a space-separated list of CSS classes to add on a GtkWidget,
 *    or %NULL, when not set
 *
 * Since: 3.56
 **/
const gchar *
e_ui_element_item_get_css_classes (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, NULL);

	return self->data.item.css_classes;
}

/**
 * e_ui_element_item_get_action:
 * @self: an #EUIElement
 *
 * Gets a item action name.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: an action name
 *
 * Since: 3.56
 **/
const gchar *
e_ui_element_item_get_action (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, NULL);

	return self->data.item.action;
}

/**
 * e_ui_element_item_get_group:
 * @self: an #EUIElement
 *
 * Gets a radio group name.
 *
 * This can be called only on elements of kind %E_UI_ELEMENT_KIND_ITEM.
 *
 * Returns: (nullable): a radio group name of an item, or %NULL,
 *   when the item is not part of any radio group
 *
 * Since: 3.56
 **/
const gchar *
e_ui_element_item_get_group (const EUIElement *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->kind == E_UI_ELEMENT_KIND_ITEM, NULL);

	return self->data.item.group;
}

/* ***************************************************************** */

struct _EUIParser {
	GObject parent;

	EUIElement *root;
	GHashTable *accels; /* GPtrArray { gchar * } */
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_ACCELS_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (EUIParser, e_ui_parser, G_TYPE_OBJECT)

static void
e_ui_parser_finalize (GObject *object)
{
	EUIParser *self = E_UI_PARSER (object);

	g_clear_pointer (&self->root, e_ui_element_free);
	g_clear_pointer (&self->accels, g_hash_table_unref);

	G_OBJECT_CLASS (e_ui_parser_parent_class)->finalize (object);
}

static void
e_ui_parser_class_init (EUIParserClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = e_ui_parser_finalize;

	/**
	 * EUIParser::changed:
	 * @parser: an #EUIParser
	 *
	 * Emitted when the content of the @parser changes.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_CHANGED] = g_signal_new ("changed",
		E_TYPE_UI_PARSER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/* void   (* accels_changed)	(EUIParser *parser,
					 const gchar *action_name,
					 GPtrArray *old_accels,
					 GPtrArray *new_accels); */
	/**
	 * EUIParser::accels-changed:
	 * @parser: an #EUIParser
	 * @action_name: an action name
	 * @old_accels: (element-type utf8) (nullable): accels used before the change, or %NULL
	 * @new_accels: (element-type utf8) (nullable): accels used after the change, or %NULL
	 *
	 * Emitted when the settings for the accels change. When the @old_accels
	 * is %NULL, the there had not been set any accels for the @action_name
	 * yet. When the @new_accels is %NULL, the accels for the @action_name had
	 * been removed. For the %NULL the accels defined on the #EUIAction should
	 * be used.
	 *
	 * Since: 3.56
	 **/
	signals[SIGNAL_ACCELS_CHANGED] = g_signal_new ("accels-changed",
		E_TYPE_UI_PARSER,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		NULL,
		G_TYPE_NONE, 3,
		G_TYPE_STRING,
		G_TYPE_PTR_ARRAY,
		G_TYPE_PTR_ARRAY);
}

static void
e_ui_parser_init (EUIParser *self)
{
}

/**
 * e_ui_parser_new:
 *
 * Creates a new #EUIParser. Use g_object_unref() to free it, when no longer needed.
 *
 * Returns: (transfer full): a new #EUIParser
 *
 * Since: 3.56
 **/
EUIParser *
e_ui_parser_new (void)
{
	return g_object_new (E_TYPE_UI_PARSER, NULL);
}

/**
 * e_ui_parser_merge_file:
 * @self: an #EUIParser
 * @filename: a filename to merge
 * @error: an output location to store a #GError at, or %NULL
 *
 * Adds content of the @filename into the UI definition. Items with
 * the same identifier are reused, allowing to add new items into
 * existing hierarchy.
 *
 * Returns: whether could successfully parse the file content. On failure,
 *   the @error is set.
 *
 * Since: 3.56
 **/
gboolean
e_ui_parser_merge_file (EUIParser *self,
			const gchar *filename,
			GError **error)
{
	gchar *full_filename = NULL;
	gchar *content = NULL;
	gsize content_len = 0;
	gboolean success;

	g_return_val_if_fail (E_IS_UI_PARSER (self), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	if (!strchr (filename, G_DIR_SEPARATOR))
		full_filename = g_build_filename (EVOLUTION_UIDIR, filename, NULL);

	success = g_file_get_contents (full_filename ? full_filename : filename, &content, &content_len, error);

	g_free (full_filename);

	if (!success)
		return FALSE;

	success = e_ui_parser_merge_data (self, content, content_len, error);

	g_free (content);

	return success;
}

static EUIElement * /* (transfer full) */
ui_parser_xml_read_item_element (const gchar *element_name,
				 const gchar **attribute_names,
				 const gchar **attribute_values,
				 GError **error)
{
	EUIElement *ui_elem;
	const gchar *action = NULL;
	const gchar *group = NULL;
	const gchar *label_priority = NULL;
	const gchar *icon_only = NULL;
	const gchar *text_only = NULL;
	const gchar *important = NULL;
	const gchar *css_classes = NULL;
	const gchar *order = NULL;
	gint order_index = 0;

	if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
		G_MARKUP_COLLECT_STRING, "action", &action,
		G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "group", &group,
		G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "css_classes", &css_classes,
		G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "label_priority", &label_priority, /* for headerbar */
		G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "icon_only", &icon_only, /* for headerbar */
		G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "order", &order, /* for headerbar */
		G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "text_only", &text_only, /* for menu */
		G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "important", &important, /* for toolbar */
		G_MARKUP_COLLECT_INVALID))
		return NULL;

	if (order && *order) {
		gchar *endptr = NULL;

		order_index = (gint) g_ascii_strtoll (order, &endptr, 10);
		if (!order_index && endptr == order) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"Element <%s> can have optional 'order' attribute of type integer, but the value '%s' is not a valid integer", element_name, order);
			return NULL;
		}
	} else {
		order = NULL;
	}

	if (icon_only && *icon_only &&
	    g_strcmp0 (icon_only, "true") != 0 &&
	    g_strcmp0 (icon_only, "false") != 0) {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			"The 'icon-only' expects only 'true' or 'false' value, but '%s' was provided instead", icon_only);
		return NULL;
	}

	if (text_only && *text_only &&
	    g_strcmp0 (text_only, "true") != 0 &&
	    g_strcmp0 (text_only, "false") != 0) {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			"The 'text-only' expects only 'true' or 'false' value, but '%s' was provided instead", text_only);
		return NULL;
	}

	if (important && *important &&
	    g_strcmp0 (important, "true") != 0 &&
	    g_strcmp0 (important, "false") != 0) {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			"The 'important' expects only 'true' or 'false' value, but '%s' was provided instead", important);
		return NULL;
	}

	ui_elem = e_ui_element_new (E_UI_ELEMENT_KIND_ITEM, NULL);
	ui_elem->data.item.css_classes = e_util_strdup_strip (css_classes);
	ui_elem->data.item.action = e_util_strdup_strip (action);
	ui_elem->data.item.group = e_util_strdup_strip (group);
	ui_elem->data.item.important = g_strcmp0 (important, "true") == 0;

	if (icon_only && *icon_only)
		ui_elem->data.item.icon_only = g_strcmp0 (icon_only, "true") == 0 ? 1 : 0;

	if (text_only && *text_only)
		ui_elem->data.item.text_only = g_strcmp0 (text_only, "true") == 0 ? 1 : 0;

	if (label_priority && *label_priority) {
		guint64 value;

		value = g_ascii_strtoull (label_priority, NULL, 10);
		ui_elem->data.item.label_priority = value;
	}

	if (order)
		ui_elem->data.item.order = order_index;

	return ui_elem;
}

typedef struct _ParseData {
	EUIParser *self;
	GPtrArray *reading_accels;
	GSList *elems_stack;
	gboolean changed;
} ParseData;

/* returns whether handled, not whether succeeded */
static gboolean
ui_parser_xml_handle_item_separator_placeholder (ParseData *pd,
						 const gchar *element_name,
						 const gchar **attribute_names,
						 const gchar **attribute_values,
						 GError **error)
{
	EUIElement *ui_elem = NULL, *parent_ui_elem;
	gboolean is_existing = FALSE;

	parent_ui_elem = pd->elems_stack->data;

	if (g_strcmp0 (element_name, "item") == 0) {
		ui_elem = ui_parser_xml_read_item_element (element_name, attribute_names, attribute_values, error);
		if (!ui_elem)
			return TRUE;
	} else if (g_strcmp0 (element_name, "separator") == 0) {
		ui_elem = e_ui_element_new (E_UI_ELEMENT_KIND_SEPARATOR, NULL);
	} else if (g_strcmp0 (element_name, "placeholder") == 0) {
		const gchar *id = NULL;

		if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
			G_MARKUP_COLLECT_STRING, "id", &id,
			G_MARKUP_COLLECT_INVALID))
			return TRUE;

		ui_elem = e_ui_element_get_child_by_id (parent_ui_elem, id);
		if (ui_elem) {
			if (ui_elem->kind != E_UI_ELEMENT_KIND_PLACEHOLDER) {
				g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
					"Duplicate element id \"%s\" of different kind detected, existing kind <%s>, requested kind <placeholder>",
					id, e_ui_element_kind_to_string (ui_elem->kind));
				return TRUE;
			}

			is_existing = TRUE;
		} else {
			ui_elem = e_ui_element_new (E_UI_ELEMENT_KIND_PLACEHOLDER, id);
		}
	} else {
		return FALSE;
	}

	if (!is_existing) {
		e_ui_element_add_child (parent_ui_elem, ui_elem);
		pd->changed = TRUE;
	}

	if (ui_elem->kind == E_UI_ELEMENT_KIND_PLACEHOLDER)
		pd->elems_stack = g_slist_prepend (pd->elems_stack, ui_elem);

	return TRUE;
}

static void
ui_parser_xml_handle_menu_submenu_start_element (ParseData *pd,
						 const gchar *element_name,
						 const gchar **attribute_names,
						 const gchar **attribute_values,
						 GError **error)
{
	EUIElement *ui_elem = NULL, *parent_ui_elem;
	const gchar *id = NULL, *action = NULL;

	if (ui_parser_xml_handle_item_separator_placeholder (pd, element_name, attribute_names, attribute_values, error))
		return;

	if (g_strcmp0 (element_name, "submenu") == 0) {
		if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
			G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "id", &id,
			G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "action", &action,
			G_MARKUP_COLLECT_INVALID))
			return;
	} else {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
			"Unknown element <%s>, expected <submenu>, <item>, <separator> or <placeholder>", element_name);
		return;
	}

	/* to not need to specify both 'id' and 'action' attributes */
	if (action && !id)
		id = action;

	parent_ui_elem = pd->elems_stack->data;
	if (id)
		ui_elem = e_ui_element_get_child_by_id (parent_ui_elem, id);
	if (ui_elem) {
		if (ui_elem->kind != E_UI_ELEMENT_KIND_SUBMENU) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"Duplicate element id \"%s\" of different kind detected, existing kind <%s>, requested kind <submenu>",
				id, e_ui_element_kind_to_string (ui_elem->kind));
			return;
		}
	} else if (!action) {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			"Element <%s> requires 'action' attribute", element_name);
		return;
	} else {
		ui_elem = e_ui_element_new (E_UI_ELEMENT_KIND_SUBMENU, id);
		e_ui_element_add_child (parent_ui_elem, ui_elem);
		pd->changed = TRUE;

		if (action)
			ui_elem->data.submenu.action = e_util_strdup_strip (action);
	}

	pd->elems_stack = g_slist_prepend (pd->elems_stack, ui_elem);
}

static void
ui_parser_xml_handle_subroot_start_element (ParseData *pd,
					    const gchar *element_name,
					    const gchar **attribute_names,
					    const gchar **attribute_values,
					    GError **error)
{
	EUIElementKind elem_kind = E_UI_ELEMENT_KIND_UNKNOWN;
	EUIElement *ui_elem, *parent_ui_elem;
	const gchar *id = NULL;
	const gchar *type = NULL;
	const gchar *primary = NULL;
	gboolean is_popup = FALSE;

	if (g_strcmp0 (element_name, "headerbar") == 0) {
		elem_kind = E_UI_ELEMENT_KIND_HEADERBAR;
	} else if (g_strcmp0 (element_name, "menu") == 0) {
		elem_kind = E_UI_ELEMENT_KIND_MENU;
	} else if (g_strcmp0 (element_name, "toolbar") == 0) {
		elem_kind = E_UI_ELEMENT_KIND_TOOLBAR;
	} else {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
			"Unknown element <%s>, expected <headerbar>, <menu> or <toolbar>", element_name);
		return;
	}

	if (elem_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
			G_MARKUP_COLLECT_STRING, "id", &id,
			G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "type", &type,
			G_MARKUP_COLLECT_INVALID))
			return;

		if (type && *type && g_strcmp0 (type, "gtk") != 0) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"Element <%s> can have optional 'type' attribute of value 'gtk' only, but type '%s' provided", element_name, type);
			return;
		}
	} else if (elem_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
			G_MARKUP_COLLECT_STRING, "id", &id,
			G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "primary", &primary,
			G_MARKUP_COLLECT_INVALID))
			return;

		if (primary && *primary && g_strcmp0 (primary, "true") != 0 && g_strcmp0 (primary, "false") != 0) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"Element <%s> can have optional 'primary' attribute of value 'true' or 'false' only, but value '%s' provided", element_name, primary);
			return;
		}
	} else { /* E_UI_ELEMENT_KIND_MENU */
		const gchar *is_popup_str = NULL;

		if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
			G_MARKUP_COLLECT_STRING, "id", &id,
			G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "is-popup", &is_popup_str,
			G_MARKUP_COLLECT_INVALID))
			return;

		if (is_popup_str && *is_popup_str && g_strcmp0 (is_popup_str, "true") != 0 && g_strcmp0 (is_popup_str, "false") != 0) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"Element <%s> can have optional 'is-popup' attribute of value 'true' or 'false' only, but value '%s' provided", element_name, is_popup_str);
			return;
		}

		is_popup = g_strcmp0 (is_popup_str, "true") == 0;
	}

	parent_ui_elem = pd->elems_stack->data;
	ui_elem = e_ui_element_get_child_by_id (parent_ui_elem, id);
	if (ui_elem) {
		if (ui_elem->kind != elem_kind) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"Duplicate element id \"%s\" of different kind detected, existing kind <%s>, requested kind <%s>",
				id, e_ui_element_kind_to_string (ui_elem->kind), e_ui_element_kind_to_string (elem_kind));
			return;
		}
	} else {
		ui_elem = e_ui_element_new (elem_kind, id);

		if (type && *type && g_strcmp0 (type, "gtk") == 0)
			ui_elem->data.headerbar.use_gtk_type = TRUE;
		if (primary && *primary && g_strcmp0 (primary, "true") == 0)
			ui_elem->data.toolbar.primary = TRUE;
		if (is_popup)
			ui_elem->data.menu.is_popup = TRUE;

		e_ui_element_add_child (parent_ui_elem, ui_elem);
		pd->changed = TRUE;
	}

	pd->elems_stack = g_slist_prepend (pd->elems_stack, ui_elem);
}

static gint
ui_parser_sort_by_order_cmp (gconstpointer aa,
			     gconstpointer bb)
{
	const EUIElement *elem1 = *((EUIElement **) aa);
	const EUIElement *elem2 = *((EUIElement **) bb);

	if (!elem1 || !elem2) {
		if (elem1)
			return 1;
		if (elem2)
			return -1;
		return 0;
	}

	return e_ui_element_item_get_order (elem1) - e_ui_element_item_get_order (elem2);
}

static void
ui_parser_xml_handle_start_end (ParseData *pd,
				const gchar *element_name,
				const gchar **attribute_names,
				const gchar **attribute_values,
				GError **error)
{
	EUIElementKind elem_kind = E_UI_ELEMENT_KIND_UNKNOWN;
	EUIElement *ui_elem = NULL, *parent_ui_elem;
	guint ii;

	if (g_strcmp0 (element_name, "start") == 0) {
		elem_kind = E_UI_ELEMENT_KIND_START;
	} else if (g_strcmp0 (element_name, "end") == 0) {
		elem_kind = E_UI_ELEMENT_KIND_END;
	} else {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
			"Unknown element <%s>, expected <start> or <end>", element_name);
		return;
	}

	if (attribute_names != NULL && attribute_names[0]) {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
			"Element <%s> doesn't have any attributes", element_name);
		return;
	}

	parent_ui_elem = pd->elems_stack->data;

	for (ii = 0; ii < e_ui_element_get_n_children (parent_ui_elem); ii++) {
		ui_elem = e_ui_element_get_child (parent_ui_elem, ii);
		if (ui_elem && ui_elem->kind == elem_kind)
			break;
		else
			ui_elem = NULL;
	}

	if (!ui_elem) {
		ui_elem = e_ui_element_new (elem_kind, e_ui_element_kind_to_string (elem_kind));
		e_ui_element_add_child (parent_ui_elem, ui_elem);
		pd->changed = TRUE;
	}

	pd->elems_stack = g_slist_prepend (pd->elems_stack, ui_elem);
}

static void
ui_parser_xml_handle_accel (ParseData *pd,
			    const gchar *element_name,
			    const gchar **attribute_names,
			    const gchar **attribute_values,
			    GError **error)
{
	const gchar *value = NULL;

	g_return_if_fail (pd->reading_accels != NULL);

	if (g_strcmp0 (element_name, "accel") != 0) {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
			"Unknown element <%s>, expected <accel>", element_name);
		return;
	}

	if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
		G_MARKUP_COLLECT_STRING, "value", &value,
		G_MARKUP_COLLECT_INVALID))
		return;

	if (value && *value)
		g_ptr_array_add (pd->reading_accels, g_strdup (value));
}

static void
ui_parser_xml_handle_accels (ParseData *pd,
			     const gchar *element_name,
			     const gchar **attribute_names,
			     const gchar **attribute_values,
			     GError **error)
{
	const gchar *action = NULL;

	g_return_if_fail (pd->reading_accels == NULL);

	if (g_strcmp0 (element_name, "accels") != 0) {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
			"Unknown element <%s>, expected <accels>", element_name);
		return;
	}

	if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
		G_MARKUP_COLLECT_STRING, "action", &action,
		G_MARKUP_COLLECT_INVALID))
		return;

	if (action && *action) {
		/* empty array is okay too, it removes in-code defined accels for the action */
		pd->reading_accels = g_ptr_array_new_with_free_func (g_free);

		if (!pd->self->accels)
			pd->self->accels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);

		g_hash_table_insert (pd->self->accels, g_strdup (action), pd->reading_accels);
	}
}

static void
ui_parser_xml_start_element (GMarkupParseContext *context,
			     const gchar *element_name,
			     const gchar **attribute_names,
			     const gchar **attribute_values,
			     gpointer user_data,
			     GError **error)
{
	ParseData *pd = user_data;
	EUIElement *ui_elem;
	EUIElementKind parent_kind;
	GSList *stack_link;

	if (!pd->elems_stack) {
		if (g_strcmp0 (element_name, "eui") != 0) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
				"Unknown element <%s>, expected <eui>", element_name);
			return;
		}

		if (!pd->self->root) {
			pd->self->root = e_ui_element_new (E_UI_ELEMENT_KIND_ROOT, NULL);
			pd->changed = TRUE;
		}

		pd->elems_stack = g_slist_prepend (pd->elems_stack, pd->self->root);

		return;
	}

	ui_elem = pd->elems_stack->data;
	parent_kind = ui_elem->kind;

	stack_link = pd->elems_stack;

	while (parent_kind == E_UI_ELEMENT_KIND_PLACEHOLDER && stack_link && stack_link->next) {
		EUIElement *parent_ui_elem;

		parent_ui_elem = stack_link->next->data;
		parent_kind = parent_ui_elem->kind;

		stack_link = stack_link->next;
	}

	if (pd->reading_accels) {
		ui_parser_xml_handle_accel (pd, element_name, attribute_names, attribute_values, error);
	} else if (g_strcmp0 (element_name, "accels") == 0) {
		ui_parser_xml_handle_accels (pd, element_name, attribute_names, attribute_values, error);
	} else if (parent_kind == E_UI_ELEMENT_KIND_ROOT) {
		ui_parser_xml_handle_subroot_start_element (pd, element_name, attribute_names, attribute_values, error);
	} else if (parent_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		ui_parser_xml_handle_start_end (pd, element_name, attribute_names, attribute_values, error);
	} else if (parent_kind == E_UI_ELEMENT_KIND_START || parent_kind == E_UI_ELEMENT_KIND_END) {
		if (!ui_parser_xml_handle_item_separator_placeholder (pd, element_name, attribute_names, attribute_values, error)) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
				"Unknown element <%s>, expected <item>, <separator> or <placeholder>", element_name);
		}
	} else if (parent_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		if (!ui_parser_xml_handle_item_separator_placeholder (pd, element_name, attribute_names, attribute_values, error)) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
				"Unknown element <%s>, expected <item>, <separator> or <placeholder>", element_name);
		}
	} else if (parent_kind == E_UI_ELEMENT_KIND_MENU ||
		   parent_kind == E_UI_ELEMENT_KIND_SUBMENU) {
		ui_parser_xml_handle_menu_submenu_start_element (pd, element_name, attribute_names, attribute_values, error);
	} else {
		g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
			"Unknown element <%s> under <%s>", element_name, e_ui_element_kind_to_string (parent_kind));
	}
}

static void
ui_parser_xml_end_element (GMarkupParseContext *context,
			   const gchar *element_name,
			   gpointer user_data,
			   GError **error)
{
	ParseData *pd = user_data;

	if (g_strcmp0 (element_name, "eui") == 0) {
		if (!pd->elems_stack) {
			g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Ends <eui> without root element");
		} else if (g_slist_length (pd->elems_stack) != 1) {
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
				"Expected <eui> end with single elem stack, but the stack has %u items",
				g_slist_length (pd->elems_stack));
		} else if (pd->elems_stack->data != pd->self->root) {
			g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Ends <eui> with incorrect stack top");
		} else {
			pd->elems_stack = g_slist_remove (pd->elems_stack, pd->elems_stack->data);
		}
	} else if (g_strcmp0 (element_name, "item") == 0 ||
		   g_strcmp0 (element_name, "separator") == 0 ||
		   g_strcmp0 (element_name, "accel") == 0) {
	} else if (g_strcmp0 (element_name, "accels") == 0) {
		if (!pd->reading_accels)
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Unexpected element end <%s>", element_name);
		pd->reading_accels = NULL;
	} else if (pd->elems_stack) {
		EUIElementKind expect_kind = E_UI_ELEMENT_KIND_UNKNOWN;

		if (g_strcmp0 (element_name, "headerbar") == 0)
			expect_kind = E_UI_ELEMENT_KIND_HEADERBAR;
		else if (g_strcmp0 (element_name, "toolbar") == 0)
			expect_kind = E_UI_ELEMENT_KIND_TOOLBAR;
		else if (g_strcmp0 (element_name, "menu") == 0)
			expect_kind = E_UI_ELEMENT_KIND_MENU;
		else if (g_strcmp0 (element_name, "submenu") == 0)
			expect_kind = E_UI_ELEMENT_KIND_SUBMENU;
		else if (g_strcmp0 (element_name, "placeholder") == 0)
			expect_kind = E_UI_ELEMENT_KIND_PLACEHOLDER;
		else if (g_strcmp0 (element_name, "start") == 0)
			expect_kind = E_UI_ELEMENT_KIND_START;
		else if (g_strcmp0 (element_name, "end") == 0)
			expect_kind = E_UI_ELEMENT_KIND_END;
		else
			g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Unexpected element end <%s>", element_name);

		if (expect_kind != E_UI_ELEMENT_KIND_UNKNOWN) {
			EUIElement *ui_elem = pd->elems_stack->data;

			if (ui_elem->kind == expect_kind)
				pd->elems_stack = g_slist_remove (pd->elems_stack, pd->elems_stack->data);
			else
				g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Unexpected element <%s> on stack, but ended was <%s>",
					e_ui_element_kind_to_string (ui_elem->kind), element_name);

			if (expect_kind == E_UI_ELEMENT_KIND_START || expect_kind == E_UI_ELEMENT_KIND_END) {
				guint ii;

				for (ii = 0; ii < e_ui_element_get_n_children (ui_elem); ii++) {
					EUIElement *child = e_ui_element_get_child (ui_elem, ii);

					if (child && e_ui_element_item_get_order (child) != 0)
						break;
				}

				/* when one element has non-zero order */
				if (ii < e_ui_element_get_n_children (ui_elem))
					g_ptr_array_sort (ui_elem->children, ui_parser_sort_by_order_cmp);
			}
		}
	}
}

/**
 * e_ui_parser_merge_data:
 * @self: an #EUIParser
 * @data: a data to merge
 * @data_len: length of the @data, or -1, when NUL-terminated
 * @error: an output location to store a #GError at, or %NULL
 *
 * Adds content of the @data into the UI definition. Items with
 * the same identifier are reused, allowing to add new items into
 * existing hierarchy.
 *
 * Returns: whether could successfully parse the file content. On failure,
 *   the @error is set.
 *
 * Since: 3.56
 **/
gboolean
e_ui_parser_merge_data (EUIParser *self,
			const gchar *data,
			gssize data_len,
			GError **error)
{
	static GMarkupParser xml_parser = {
		ui_parser_xml_start_element,
		ui_parser_xml_end_element,
		NULL, /* text */
		NULL, /* passthrough */
		NULL  /* error */
	};
	GMarkupParseContext *context;
	ParseData pd = { NULL, };
	gboolean success;

	g_return_val_if_fail (E_IS_UI_PARSER (self), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	pd.self = self;
	pd.reading_accels = NULL;
	pd.elems_stack = NULL;
	pd.changed = FALSE;

	context = g_markup_parse_context_new (&xml_parser, 0, &pd, NULL);

	success = g_markup_parse_context_parse (context, data, data_len, error) &&
		g_markup_parse_context_end_parse (context, error);

	g_markup_parse_context_free (context);

	if (pd.changed)
		g_signal_emit (self, signals[SIGNAL_CHANGED], 0, NULL);

	return success;
}

/**
 * e_ui_parser_clear:
 * @self: an #EUIParser
 *
 * Removes all content of the @self.
 *
 * Since: 3.56
 **/
void
e_ui_parser_clear (EUIParser *self)
{
	g_return_if_fail (E_IS_UI_PARSER (self));

	if (self->root) {
		g_clear_pointer (&self->root, e_ui_element_free);
		g_signal_emit (self, signals[SIGNAL_CHANGED], 0, NULL);
	}
}

/**
 * e_ui_parser_get_root:
 * @self: an #EUIParser
 *
 * Gets the root element of the @self.
 *
 * Returns: (nullable) (transfer none): a root #EUIElement, or %NULL, when nothing was parsed yet
 *
 * Since: 3.56
 **/
EUIElement *
e_ui_parser_get_root (EUIParser *self)
{
	g_return_val_if_fail (E_IS_UI_PARSER (self), NULL);

	return self->root;
}

/**
 * e_ui_parser_create_root:
 * @self: and #EUIParser
 *
 * Clears any current content of the @self and create a new root element,
 * which will be used by the @self.
 *
 * Returns: (transfer none): a new root element of the @self
 *
 * Since: 3.56
 **/
EUIElement *
e_ui_parser_create_root (EUIParser *self)
{
	g_return_val_if_fail (E_IS_UI_PARSER (self), NULL);

	e_ui_parser_clear (self);

	self->root = e_ui_element_new (E_UI_ELEMENT_KIND_ROOT, NULL);

	return self->root;
}

/**
 * e_ui_parser_get_accels:
 * @self: an #EUIParser
 * @action_name: an action name
 *
 * Returns an array of the defined accelerators for the @action_name, to be used
 * instead of those defined in the code. An empty array means no accels to be used,
 * while a %NULL means no accels had been set for the @action_name.
 *
 * The first item of the returned array is meant as the main accelerator,
 * while the following are secondary accelerators.
 *
 * Returns: (nullable) (transfer none) (element-type utf8): a #GPtrArray with
 *    the accelerators for the @action_name, or %NULL
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_parser_get_accels (EUIParser *self,
			const gchar *action_name)
{
	g_return_val_if_fail (E_IS_UI_PARSER (self), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	if (!self->accels)
		return NULL;

	return g_hash_table_lookup (self->accels, action_name);
}

/**
 * e_ui_parser_take_accels:
 * @self: an #EUIParser
 * @action_name: an action name
 * @accels: (nullable) (transfer full) (element-type utf8): accelerators to use, or %NULL to unset
 *
 * Sets the @accels as the accelerators for the action @action_name.
 * Use %NULL to unset any previous values.
 *
 * The function assumes ownership of the @accels.
 *
 * Since: 3.56
 **/
void
e_ui_parser_take_accels (EUIParser *self,
			 const gchar *action_name,
			 GPtrArray *accels)
{
	GPtrArray *old_accels;

	g_return_if_fail (E_IS_UI_PARSER (self));
	g_return_if_fail (action_name != NULL);

	if (!self->accels && !accels)
		return;

	if (!self->accels)
		self->accels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);

	old_accels = g_hash_table_lookup (self->accels, action_name);
	if (old_accels)
		g_ptr_array_ref (old_accels);

	if (!accels)
		g_hash_table_remove (self->accels, action_name);
	else
		g_hash_table_insert (self->accels, g_strdup (action_name), accels);

	g_signal_emit (self, signals[SIGNAL_ACCELS_CHANGED], 0, action_name, old_accels, accels, NULL);

	g_clear_pointer (&old_accels, g_ptr_array_unref);
}

/**
 * e_ui_parser_export:
 * @self: an #EUIParser
 * @flags: a bit-or of #EUIParserExportFlags flags
 *
 * Exports the content of the @self in a format suitable for e_ui_parser_merge_file()
 * and e_ui_parser_merge_data().
 *
 * Free the returned string with g_free(), when no longer needed.
 *
 * Returns: (transfer full) (nullable): a string representation of the @self content,
 *   or %NULL, when the @self is empty.
 *
 * Since: 3.56
 **/
gchar *
e_ui_parser_export (EUIParser *self,
		    EUIParserExportFlags flags)
{
	GString *str;

	g_return_val_if_fail (E_IS_UI_PARSER (self), NULL);

	if ((!self->root || !e_ui_element_get_n_children (self->root)) &&
	    (!self->accels || !g_hash_table_size (self->accels)))
		return NULL;

	if (!self->root)
		self->root = e_ui_element_new (E_UI_ELEMENT_KIND_ROOT, NULL);

	str = g_string_sized_new (1024);

	e_ui_element_export (self->root, str, (flags & E_UI_PARSER_EXPORT_FLAG_INDENT) ? 0 : -1, self->accels);

	if (!(flags & E_UI_PARSER_EXPORT_FLAG_INDENT))
		g_string_append_c (str, '\n');

	return g_string_free (str, FALSE);
}
