/*
 * Borrowed from Moblin-Web-Browser: The web browser for Moblin
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _E_MAIL_TAB_PICKER_H
#define _E_MAIL_TAB_PICKER_H

#include <glib-object.h>
#include <clutter/clutter.h>
#include <mx/mx.h>
#include "e-mail-tab.h"

G_BEGIN_DECLS

#define E_MAIL_TYPE_TAB_PICKER e_mail_tab_picker_get_type()

#define E_MAIL_TAB_PICKER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  E_MAIL_TYPE_TAB_PICKER, EMailTabPicker))

#define E_MAIL_TAB_PICKER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  E_MAIL_TYPE_TAB_PICKER, EMailTabPickerClass))

#define E_MAIL_IS_TAB_PICKER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  E_MAIL_TYPE_TAB_PICKER))

#define E_MAIL_IS_TAB_PICKER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  E_MAIL_TYPE_TAB_PICKER))

#define E_MAIL_TAB_PICKER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  E_MAIL_TYPE_TAB_PICKER, EMailTabPickerClass))

typedef struct _EMailTabPickerPrivate EMailTabPickerPrivate;

typedef struct {
  MxWidget parent;

  EMailTabPickerPrivate *priv;
} EMailTabPicker;

typedef struct {
  MxWidgetClass parent_class;

  void (* tab_activated)   (EMailTabPicker *picker, EMailTab *tab);
  void (* chooser_clicked) (EMailTabPicker *picker);
} EMailTabPickerClass;

GType e_mail_tab_picker_get_type (void);

ClutterActor *e_mail_tab_picker_new (void);

void e_mail_tab_picker_add_tab (EMailTabPicker *picker, EMailTab *tab, gint position);

void e_mail_tab_picker_remove_tab (EMailTabPicker *picker, EMailTab *tab);

GList *e_mail_tab_picker_get_tabs (EMailTabPicker *picker);

gint e_mail_tab_picker_get_n_tabs (EMailTabPicker *picker);

EMailTab *e_mail_tab_picker_get_tab (EMailTabPicker *picker, gint tab);

gint e_mail_tab_picker_get_tab_no (EMailTabPicker *picker, EMailTab *tab);

gint e_mail_tab_picker_get_current_tab (EMailTabPicker *picker);

void e_mail_tab_picker_set_current_tab (EMailTabPicker *picker, gint tab);

void e_mail_tab_picker_reorder (EMailTabPicker *picker,
                             gint          old_position,
                             gint          new_position);

void e_mail_tab_picker_set_tab_width (EMailTabPicker *picker,
                                   gint          width);

gint e_mail_tab_picker_get_tab_width (EMailTabPicker *picker);

void
e_mail_tab_picker_get_preferred_height (EMailTabPicker *tab_picker,
                                     gfloat        for_width,
                                     gfloat       *min_height_p,
                                     gfloat       *natural_height_p,
                                     gboolean      with_previews);

void e_mail_tab_picker_set_preview_mode (EMailTabPicker *picker, gboolean preview);

gboolean e_mail_tab_picker_get_preview_mode (EMailTabPicker *picker);

void e_mail_tab_picker_enable_drop (EMailTabPicker *picker, gboolean enable);

G_END_DECLS

#endif /* _E_MAIL_TAB_PICKER_H */

