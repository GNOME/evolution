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

#ifndef _E_MAIL_TAB_H
#define _E_MAIL_TAB_H

#include <glib-object.h>
#include <clutter/clutter.h>
#include <mx/mx.h>

G_BEGIN_DECLS

#define E_MAIL_TYPE_TAB e_mail_tab_get_type()

#define E_MAIL_TAB(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  E_MAIL_TYPE_TAB, EMailTab))

#define E_MAIL_TAB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  E_MAIL_TYPE_TAB, EMailTabClass))

#define E_MAIL_IS_TAB(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  E_MAIL_TYPE_TAB))

#define E_MAIL_IS_TAB_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  E_MAIL_TYPE_TAB))

#define E_MAIL_TAB_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  E_MAIL_TYPE_TAB, EMailTabClass))

typedef struct _EMailTabPrivate EMailTabPrivate;

typedef struct {
  MxWidget parent;

  EMailTabPrivate *priv;
} EMailTab;

typedef struct {
  MxWidgetClass parent_class;

  void (* clicked)             (EMailTab *tab);
  void (* closed)              (EMailTab *tab);
  void (* transition_complete) (EMailTab *tab);
} EMailTabClass;

GType e_mail_tab_get_type (void);

ClutterActor *e_mail_tab_new (void);
ClutterActor *e_mail_tab_new_full (const gchar  *text,
                                ClutterActor *icon,
                                gint          width);

void e_mail_tab_set_text                (EMailTab *tab, const gchar  *text);
void e_mail_tab_set_default_icon        (EMailTab *tab, ClutterActor *icon);
void e_mail_tab_set_icon                (EMailTab *tab, ClutterActor *icon);
void e_mail_tab_set_can_close           (EMailTab *tab, gboolean      can_close);
void e_mail_tab_set_width               (EMailTab *tab, gint          width);
void e_mail_tab_set_docking             (EMailTab *tab, gboolean      docking);
void e_mail_tab_set_preview_actor       (EMailTab *tab, ClutterActor *actor);
void e_mail_tab_set_preview_mode        (EMailTab *tab, gboolean      preview);
void e_mail_tab_set_preview_duration    (EMailTab *tab, guint         duration);
void e_mail_tab_set_spacing             (EMailTab *tab, gfloat        spacing);
void e_mail_tab_set_private             (EMailTab *tab, gboolean      private);
void e_mail_tab_set_active              (EMailTab *tab, gboolean      active);

const gchar  *e_mail_tab_get_text                (EMailTab *tab);
ClutterActor *e_mail_tab_get_icon                (EMailTab *tab);
gboolean      e_mail_tab_get_can_close           (EMailTab *tab);
gint          e_mail_tab_get_width               (EMailTab *tab);
gboolean      e_mail_tab_get_docking             (EMailTab *tab);
ClutterActor *e_mail_tab_get_preview_actor       (EMailTab *tab);
gboolean      e_mail_tab_get_preview_mode        (EMailTab *tab);
void          e_mail_tab_get_height_no_preview   (EMailTab *tab,
                                               gfloat  for_width,
                                               gfloat *min_height_p,
                                               gfloat *natural_height_p);
guint         e_mail_tab_get_preview_duration    (EMailTab *tab);
gfloat        e_mail_tab_get_spacing             (EMailTab *tab);
gboolean      e_mail_tab_get_private             (EMailTab *tab);
gboolean      e_mail_tab_get_active              (EMailTab *tab);

void e_mail_tab_alert                   (EMailTab *tab);
void e_mail_tab_enable_drag             (EMailTab *tab, gboolean enable);

G_END_DECLS

#endif /* _E_MAIL_TAB_H */

