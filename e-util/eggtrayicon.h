/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* eggtrayicon.h
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EGG_TRAY_ICON_H__
#define __EGG_TRAY_ICON_H__

#include <gdkconfig.h>

/* The EggTrayIcon API is implementable only on X11. GTK+ 2.10 will
 * have a cross-platform status icon API. That code has been borrowed
 * into Evolution for Win32, see after the ifdef GDK_WINDOWING_X11
 * block.
 */
#ifdef GDK_WINDOWING_X11

#include <gtk/gtkplug.h>
#include <gdk/gdkx.h>

G_BEGIN_DECLS

#define EGG_TYPE_TRAY_ICON		(egg_tray_icon_get_type ())
#define EGG_TRAY_ICON(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_TRAY_ICON, EggTrayIcon))
#define EGG_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_TRAY_ICON, EggTrayIconClass))
#define EGG_IS_TRAY_ICON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_TRAY_ICON))
#define EGG_IS_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_TRAY_ICON))
#define EGG_TRAY_ICON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_TRAY_ICON, EggTrayIconClass))
	
typedef struct _EggTrayIcon	  EggTrayIcon;
typedef struct _EggTrayIconClass  EggTrayIconClass;

struct _EggTrayIcon
{
  GtkPlug parent_instance;

  guint stamp;
  
  Atom selection_atom;
  Atom manager_atom;
  Atom system_tray_opcode_atom;
  Atom orientation_atom;
  Window manager_window;

  GtkOrientation orientation;
};

struct _EggTrayIconClass
{
  GtkPlugClass parent_class;
};

GType        egg_tray_icon_get_type       (void);

EggTrayIcon *egg_tray_icon_new_for_xscreen (Screen *xscreen, const char *name);

EggTrayIcon *egg_tray_icon_new_for_screen (GdkScreen   *screen,
					   const gchar *name);

EggTrayIcon *egg_tray_icon_new            (const gchar *name);

guint        egg_tray_icon_send_message   (EggTrayIcon *icon,
					   gint         timeout,
					   const char  *message,
					   gint         len);
void         egg_tray_icon_cancel_message (EggTrayIcon *icon,
					   guint        id);

GtkOrientation egg_tray_icon_get_orientation (EggTrayIcon *icon);
					    
G_END_DECLS

#endif	/* GDK_WINDOWING_X11 */

#ifdef GDK_WINDOWING_WIN32

#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION (2, 9, 0)

/* gtkstatusicon.h:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <gtk/gtkimage.h>

G_BEGIN_DECLS

#define GTK_TYPE_STATUS_ICON         (gtk_status_icon_get_type ())
#define GTK_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STATUS_ICON, GtkStatusIcon))
#define GTK_STATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_STATUS_ICON, GtkStatusIconClass))
#define GTK_IS_STATUS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STATUS_ICON))
#define GTK_IS_STATUS_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_STATUS_ICON))
#define GTK_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_STATUS_ICON, GtkStatusIconClass))

typedef struct _GtkStatusIcon	     GtkStatusIcon;
typedef struct _GtkStatusIconClass   GtkStatusIconClass;
typedef struct _GtkStatusIconPrivate GtkStatusIconPrivate;

struct _GtkStatusIcon
{
  GObject               parent_instance;

  GtkStatusIconPrivate *priv;
};

struct _GtkStatusIconClass
{
  GObjectClass parent_class;

  void     (* activate)     (GtkStatusIcon *status_icon);
  void     (* popup_menu)   (GtkStatusIcon *status_icon,
			     guint          button,
			     guint32        activate_time);
  gboolean (* size_changed) (GtkStatusIcon *status_icon,
			     gint           size);

  void (*__gtk_reserved1);
  void (*__gtk_reserved2);
  void (*__gtk_reserved3);
  void (*__gtk_reserved4);
  void (*__gtk_reserved5);
  void (*__gtk_reserved6);  
};

GType                 gtk_status_icon_get_type           (void) G_GNUC_CONST;

GtkStatusIcon        *gtk_status_icon_new                (void);
GtkStatusIcon        *gtk_status_icon_new_from_pixbuf    (GdkPixbuf          *pixbuf);
GtkStatusIcon        *gtk_status_icon_new_from_file      (const gchar        *filename);
GtkStatusIcon        *gtk_status_icon_new_from_stock     (const gchar        *stock_id);
GtkStatusIcon        *gtk_status_icon_new_from_icon_name (const gchar        *icon_name);

void                  gtk_status_icon_set_from_pixbuf    (GtkStatusIcon      *status_icon,
							  GdkPixbuf          *pixbuf);
void                  gtk_status_icon_set_from_file      (GtkStatusIcon      *status_icon,
							  const gchar        *filename);
void                  gtk_status_icon_set_from_stock     (GtkStatusIcon      *status_icon,
							  const gchar        *stock_id);
void                  gtk_status_icon_set_from_icon_name (GtkStatusIcon      *status_icon,
							  const gchar        *icon_name);

GtkImageType          gtk_status_icon_get_storage_type   (GtkStatusIcon      *status_icon);

GdkPixbuf            *gtk_status_icon_get_pixbuf         (GtkStatusIcon      *status_icon);
G_CONST_RETURN gchar *gtk_status_icon_get_stock          (GtkStatusIcon      *status_icon);
G_CONST_RETURN gchar *gtk_status_icon_get_icon_name      (GtkStatusIcon      *status_icon);

gint                  gtk_status_icon_get_size           (GtkStatusIcon      *status_icon);

void                  gtk_status_icon_set_tooltip        (GtkStatusIcon      *status_icon,
							  const gchar        *tooltip_text);

void                  gtk_status_icon_set_visible        (GtkStatusIcon      *status_icon,
							  gboolean            visible);
gboolean              gtk_status_icon_get_visible        (GtkStatusIcon      *status_icon);

void                  gtk_status_icon_set_blinking       (GtkStatusIcon      *status_icon,
							  gboolean            blinking);
gboolean              gtk_status_icon_get_blinking       (GtkStatusIcon      *status_icon);

gboolean              gtk_status_icon_is_embedded        (GtkStatusIcon      *status_icon);

G_END_DECLS

#endif /* !GTK_CHECK_VERSION (2, 9, 0) */

#endif	/* GDK_WINDOWING_WIN32 */

#endif /* __EGG_TRAY_ICON_H__ */
