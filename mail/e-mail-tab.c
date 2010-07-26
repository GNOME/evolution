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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include "e-mail-tab.h"

#define E_MAIL_PIXBOUND(u) ((gfloat)((gint)(u)))

static void mx_draggable_iface_init (MxDraggableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailTab,
                         e_mail_tab,
                         MX_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (MX_TYPE_DRAGGABLE,
                                                mx_draggable_iface_init))

#define TAB_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), E_MAIL_TYPE_TAB, EMailTabPrivate))

enum
{
  PROP_0,

  PROP_ICON,
  PROP_TEXT,
  PROP_CAN_CLOSE,
  PROP_TAB_WIDTH,
  PROP_DOCKING,
  PROP_PREVIEW,
  PROP_PREVIEW_MODE,
  PROP_PREVIEW_DURATION,
  PROP_SPACING,
  PROP_PRIVATE,
  PROP_ACTIVE,

  PROP_DRAG_THRESHOLD,
  PROP_DRAG_AXIS,
 // PROP_DRAG_CONTAINMENT_TYPE,
  PROP_DRAG_CONTAINMENT_AREA,
  PROP_DRAG_ENABLED,
  PROP_DRAG_ACTOR,
};

enum
{
  CLICKED,
  CLOSED,
  TRANSITION_COMPLETE,

  LAST_SIGNAL
};

/* Animation stage lengths */
#define TAB_S1_ANIM 0.75
#define TAB_S2_ANIM (1.0-TAB_S1_ANIM)

static guint signals[LAST_SIGNAL] = { 0, };

static void e_mail_tab_close_clicked_cb (MxButton *button, EMailTab *self);

struct _EMailTabPrivate
{
  ClutterActor *icon;
  ClutterActor *default_icon;
  ClutterActor *label;
  ClutterActor *close_button;
  gboolean      can_close;
  gint          width;
  gboolean      docking;
  gfloat        spacing;
  gboolean      private;
  guint         alert_count;
  guint         alert_source;
  gboolean      has_text;

  guint         active : 1;
  guint         pressed : 1;
  guint         hover : 1;

  ClutterActor    *preview;
  gboolean         preview_mode;
  ClutterTimeline *preview_timeline;
  gdouble          preview_height_progress;
  guint            anim_length;

  ClutterActor    *old_bg;

  ClutterActor        *drag_actor;
  ClutterActorBox      drag_area;
  gboolean             drag_enabled;
  MxDragAxis           drag_axis;
 // MxDragContainment    containment;
  gint                 drag_threshold;
  gulong               drag_threshold_handler;
  gfloat               press_x;
  gfloat               press_y;
  gboolean             in_drag;
};

static void
e_mail_tab_drag_begin (MxDraggable         *draggable,
                    gfloat               event_x,
                    gfloat               event_y,
                    gint                 event_button,
                    ClutterModifierType  modifiers)
{
  gfloat x, y;

  EMailTabPrivate *priv = E_MAIL_TAB (draggable)->priv;
  ClutterActor *self = CLUTTER_ACTOR (draggable);
  ClutterActor *actor = mx_draggable_get_drag_actor (draggable);
  ClutterActor *stage = clutter_actor_get_stage (self);

  priv->in_drag = TRUE;

  clutter_actor_get_transformed_position (self, &x, &y);
  clutter_actor_set_position (actor, x, y);

  /* Start up animation */
  if (CLUTTER_IS_TEXTURE (actor))
    {
      /* TODO: Some neat deformation effect? */
    }
  else
    {
      /* Fade in */
      clutter_actor_set_opacity (actor, 0x00);
      clutter_actor_animate (actor, CLUTTER_LINEAR, 150,
                             "opacity", 0xff,
                             NULL);
    }
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);
}

static void
e_mail_tab_drag_motion (MxDraggable *draggable,
                     gfloat         delta_x,
                     gfloat         delta_y)
{
  ClutterActor *actor = mx_draggable_get_drag_actor (draggable);
  clutter_actor_move_by (actor, delta_x, delta_y);
}

static void
e_mail_tab_drag_end_anim_cb (ClutterAnimation *animation,
                          EMailTab           *tab)
{
  ClutterActor *actor =
    CLUTTER_ACTOR (clutter_animation_get_object (animation));
  ClutterActor *parent = clutter_actor_get_parent (actor);

  if (parent)
    clutter_container_remove_actor (CLUTTER_CONTAINER (parent), actor);
}

static void
e_mail_tab_drag_end (MxDraggable *draggable,
                  gfloat       event_x,
                  gfloat       event_y)
{
  EMailTab *self = E_MAIL_TAB (draggable);
  EMailTabPrivate *priv = self->priv;

  priv->in_drag = FALSE;

  if (priv->drag_actor)
    {
      ClutterActor *parent = clutter_actor_get_parent (priv->drag_actor);
      if (parent)
        {
          /* Animate drop */
          if (CLUTTER_IS_TEXTURE (priv->drag_actor))
            {
              /* TODO: Some neat deformation effect? */
              clutter_container_remove_actor (CLUTTER_CONTAINER (parent),
                                              priv->drag_actor);
            }
          else
            {
              clutter_actor_animate (priv->drag_actor, CLUTTER_LINEAR, 150,
                                     "opacity", 0x00,
                                     "signal::completed",
                                       G_CALLBACK (e_mail_tab_drag_end_anim_cb),
                                       self,
                                     NULL);
            }
        }
      g_object_unref (priv->drag_actor);
      priv->drag_actor = NULL;
    }
}

static void
mx_draggable_iface_init (MxDraggableIface *iface)
{
  iface->drag_begin = e_mail_tab_drag_begin;
  iface->drag_motion = e_mail_tab_drag_motion;
  iface->drag_end = e_mail_tab_drag_end;
}

static void
e_mail_tab_get_property (GObject *object, guint property_id,
                      GValue *value, GParamSpec *pspec)
{
  EMailTab *tab = E_MAIL_TAB (object);
  EMailTabPrivate *priv = tab->priv;

  switch (property_id)
    {
    case PROP_ICON:
      g_value_set_object (value, e_mail_tab_get_icon (tab));
      break;

    case PROP_TEXT:
      g_value_set_string (value, e_mail_tab_get_text (tab));
      break;

    case PROP_CAN_CLOSE:
      g_value_set_boolean (value, e_mail_tab_get_can_close (tab));
      break;

    case PROP_TAB_WIDTH:
      g_value_set_int (value, e_mail_tab_get_width (tab));
      break;

    case PROP_DOCKING:
      g_value_set_boolean (value, e_mail_tab_get_docking (tab));
      break;

    case PROP_PREVIEW:
      g_value_set_object (value, e_mail_tab_get_preview_actor (tab));
      break;

    case PROP_PREVIEW_MODE:
      g_value_set_boolean (value, e_mail_tab_get_preview_mode (tab));
      break;

    case PROP_PREVIEW_DURATION:
      g_value_set_uint (value, e_mail_tab_get_preview_duration (tab));
      break;

    case PROP_SPACING:
      g_value_set_float (value, e_mail_tab_get_spacing (tab));
      break;

    case PROP_PRIVATE:
      g_value_set_boolean (value, e_mail_tab_get_private (tab));
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, e_mail_tab_get_active (tab));
      break;

    case PROP_DRAG_THRESHOLD:
      g_value_set_uint (value, (guint)priv->drag_threshold);
      break;

    case PROP_DRAG_AXIS:
      g_value_set_enum (value, priv->drag_axis);
      break;

//    case PROP_DRAG_CONTAINMENT_TYPE:
  //    g_value_set_enum (value, priv->containment);
    //  break;

    case PROP_DRAG_CONTAINMENT_AREA:
      g_value_set_boxed (value, &priv->drag_area);
      break;

    case PROP_DRAG_ENABLED:
      g_value_set_boolean (value, priv->drag_enabled);
      break;

    case PROP_DRAG_ACTOR:
      if (!priv->drag_actor)
        {
          ClutterActor *fbo =
            /*clutter_texture_new_from_actor (CLUTTER_ACTOR (tab));*/
            NULL;
          if (fbo)
            {
              /* This is where we'd setup deformations, or something along
               * those lines.
               */
              priv->drag_actor = g_object_ref_sink (fbo);
            }
          else
            priv->drag_actor =
              g_object_ref_sink (clutter_clone_new (CLUTTER_ACTOR (tab)));
        }
      g_value_set_object (value, priv->drag_actor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
e_mail_tab_set_property (GObject *object, guint property_id,
                      const GValue *value, GParamSpec *pspec)
{
  EMailTab *tab = E_MAIL_TAB (object);
  EMailTabPrivate *priv = tab->priv;

  switch (property_id)
    {
    case PROP_ICON:
      e_mail_tab_set_icon (tab, g_value_get_object (value));
      break;

    case PROP_TEXT:
      e_mail_tab_set_text (tab, g_value_get_string (value));
      break;

    case PROP_CAN_CLOSE:
      e_mail_tab_set_can_close (tab, g_value_get_boolean (value));

    case PROP_TAB_WIDTH:
      e_mail_tab_set_width (tab, g_value_get_int (value));
      break;

    case PROP_DOCKING:
      e_mail_tab_set_docking (tab, g_value_get_boolean (value));
      break;

    case PROP_PREVIEW:
      e_mail_tab_set_preview_actor (tab,
                                 CLUTTER_ACTOR (g_value_get_object (value)));
      break;

    case PROP_PREVIEW_MODE:
      e_mail_tab_set_preview_mode (tab, g_value_get_boolean (value));
      break;

    case PROP_PREVIEW_DURATION:
      e_mail_tab_set_preview_duration (tab, g_value_get_uint (value));
      break;

    case PROP_SPACING:
      e_mail_tab_set_spacing (tab, g_value_get_float (value));
      break;

    case PROP_PRIVATE:
      e_mail_tab_set_private (tab, g_value_get_boolean (value));
      break;

    case PROP_ACTIVE:
      e_mail_tab_set_active (tab, g_value_get_boolean (value));
      break;

    case PROP_DRAG_THRESHOLD:
      break;

    case PROP_DRAG_AXIS:
      priv->drag_axis = g_value_get_enum (value);
      break;

//    case PROP_DRAG_CONTAINMENT_TYPE:
 //     priv->containment = g_value_get_enum (value);
   //   break;

    case PROP_DRAG_CONTAINMENT_AREA:
      {
        ClutterActorBox *box = g_value_get_boxed (value);

        if (box)
          priv->drag_area = *box;
        else
          memset (&priv->drag_area, 0, sizeof (ClutterActorBox));

        break;
      }

    case PROP_DRAG_ENABLED:
      priv->drag_enabled = g_value_get_boolean (value);
      break;

    case PROP_DRAG_ACTOR:
      {
        ClutterActor *new_actor = g_value_get_object (value);

        if (priv->drag_actor)
          {
            ClutterActor *parent = clutter_actor_get_parent (priv->drag_actor);

            /* We know it's a container because we added it ourselves */
            if (parent)
              clutter_container_remove_actor (CLUTTER_CONTAINER (parent),
                                              priv->drag_actor);

            g_object_unref (priv->drag_actor);
            priv->drag_actor = NULL;
          }

        if (new_actor)
          priv->drag_actor = g_object_ref_sink (new_actor);

        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
e_mail_tab_dispose_old_bg (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->old_bg)
    {
      if (clutter_actor_get_parent (priv->old_bg) == (ClutterActor *)tab)
        clutter_actor_unparent (priv->old_bg);
      g_object_unref (priv->old_bg);
      priv->old_bg = NULL;
    }
}

static void
e_mail_tab_dispose (GObject *object)
{
  EMailTab *tab = E_MAIL_TAB (object);
  EMailTabPrivate *priv = tab->priv;

  e_mail_tab_dispose_old_bg (tab);

  if (priv->icon)
    {
      clutter_actor_unparent (priv->icon);
      priv->icon = NULL;
    }

  if (priv->default_icon)
    {
      g_object_unref (priv->default_icon);
      priv->default_icon = NULL;
    }

  if (priv->label)
    {
      clutter_actor_unparent (CLUTTER_ACTOR (priv->label));
      priv->label = NULL;
    }

  if (priv->close_button)
    {
      clutter_actor_unparent (CLUTTER_ACTOR (priv->close_button));
      priv->close_button = NULL;
    }

  if (priv->preview)
    {
      clutter_actor_unparent (priv->preview);
      priv->preview = NULL;
    }

  if (priv->alert_source)
    {
      g_source_remove (priv->alert_source);
      priv->alert_source = 0;
    }

  if (priv->drag_actor)
    {
      ClutterActor *parent = clutter_actor_get_parent (priv->drag_actor);
      if (parent)
        clutter_container_remove_actor (CLUTTER_CONTAINER (parent),
                                        priv->drag_actor);
      g_object_unref (priv->drag_actor);
      priv->drag_actor = NULL;
    }

  if (priv->drag_threshold_handler)
    {
      g_signal_handler_disconnect (gtk_settings_get_default (),
                                   priv->drag_threshold_handler);
      priv->drag_threshold_handler = 0;
    }

  G_OBJECT_CLASS (e_mail_tab_parent_class)->dispose (object);
}

static void
e_mail_tab_finalize (GObject *object)
{
  G_OBJECT_CLASS (e_mail_tab_parent_class)->finalize (object);
}

static void
e_mail_tab_get_preferred_width (ClutterActor *actor,
                             gfloat        for_height,
                             gfloat       *min_width_p,
                             gfloat       *natural_width_p)
{
  MxPadding padding;
  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  /* Get padding */
  mx_widget_get_padding (MX_WIDGET (actor), &padding);
  if (min_width_p)
    *min_width_p = padding.left + padding.right;
  if (natural_width_p)
    *natural_width_p = padding.left + padding.right;

  if (priv->width >= 0)
    {
      if (natural_width_p)
        *natural_width_p += priv->width;
    }
  else
    {
      gfloat min_width, nat_width, acc_min_width, acc_nat_width;

      acc_min_width = acc_nat_width = 0;

      if (priv->has_text)
        clutter_actor_get_preferred_size (CLUTTER_ACTOR (priv->label),
                                          &acc_min_width, NULL,
                                          &acc_nat_width, NULL);

      if (priv->icon)
        {
          clutter_actor_get_preferred_size (priv->icon,
                                            &min_width, NULL,
                                            &nat_width, NULL);
          acc_min_width += min_width;
          acc_nat_width += nat_width;
        }

      if (priv->can_close)
        {
          clutter_actor_get_preferred_size (CLUTTER_ACTOR (priv->close_button),
                                            &min_width, NULL,
                                            &nat_width, NULL);
          acc_min_width += min_width;
          acc_nat_width += nat_width;
        }

      if (priv->preview && priv->preview_mode)
        {
          clutter_actor_get_preferred_size (priv->preview,
                                            &min_width, NULL,
                                            &nat_width, NULL);
          if (min_width > acc_min_width)
            acc_min_width = min_width;
          if (nat_width > acc_nat_width)
            acc_nat_width = nat_width;
        }

      if (min_width_p)
        *min_width_p += acc_min_width;
      if (natural_width_p)
        *natural_width_p += acc_nat_width;
    }
}

void
e_mail_tab_get_height_no_preview (EMailTab *tab,
                               gfloat  for_width,
                               gfloat *min_height_p,
                               gfloat *natural_height_p)
{
  MxPadding padding;
  gfloat min_height, nat_height, tmp_min_height, tmp_nat_height;

  ClutterActor *actor = CLUTTER_ACTOR (tab);
  EMailTabPrivate *priv = tab->priv;

  /* Get padding */
  mx_widget_get_padding (MX_WIDGET (actor), &padding);
  if (min_height_p)
    *min_height_p = padding.top + padding.bottom;
  if (natural_height_p)
    *natural_height_p = padding.top + padding.bottom;

  min_height = nat_height = 0;
  if (priv->has_text)
    clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->label), -1,
                                        &min_height, &nat_height);

  if (priv->icon)
    {
      clutter_actor_get_preferred_height (priv->icon, -1,
                                          &tmp_min_height, &tmp_nat_height);
      if (tmp_min_height > min_height)
        min_height = tmp_min_height;
      if (tmp_nat_height > nat_height)
        nat_height = tmp_nat_height;
    }

  if (priv->can_close)
    {
      clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->close_button),
                                          -1, &tmp_min_height, &tmp_nat_height);
      if (tmp_min_height > min_height)
        min_height = tmp_min_height;
      if (tmp_nat_height > nat_height)
        nat_height = tmp_nat_height;
    }

  if (min_height_p)
    *min_height_p += min_height;
  if (natural_height_p)
    *natural_height_p += nat_height;
}

static void
e_mail_tab_get_preferred_height (ClutterActor *actor,
                              gfloat        for_width,
                              gfloat       *min_height_p,
                              gfloat       *natural_height_p)
{
  EMailTab *tab = E_MAIL_TAB (actor);
  EMailTabPrivate *priv = tab->priv;

  e_mail_tab_get_height_no_preview (tab, for_width,
                                 min_height_p, natural_height_p);

  if (priv->preview)
    {
      MxPadding padding;
      gfloat min_height, nat_height, label_min_height, label_nat_height;

      /* Get preview + padding height */
      mx_widget_get_padding (MX_WIDGET (actor), &padding);

      clutter_actor_get_preferred_height (priv->preview,
                                          (gfloat)priv->width,
                                          &min_height,
                                          &nat_height);

      /* Add label height */
      clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->label), -1,
                                          &label_min_height, &label_nat_height);

      min_height = (min_height * priv->preview_height_progress) +
                   padding.top + padding.bottom + priv->spacing +
                   label_min_height;
      nat_height = (nat_height * priv->preview_height_progress) +
                   padding.top + padding.bottom + priv->spacing +
                   label_nat_height;

      /* Sometimes the preview's natural height will be nan due to
       * keeping of the aspect ratio. This guards against that and stops
       * Clutter from warning that the natural height is less than the
       * minimum height.
       */
      if (isnan (nat_height))
        nat_height = min_height;

      if (min_height_p && (min_height > *min_height_p))
        *min_height_p = min_height;
      if (natural_height_p && (nat_height > *natural_height_p))
        *natural_height_p = nat_height;
    }
}

static void
e_mail_tab_allocate (ClutterActor           *actor,
                  const ClutterActorBox  *box,
                  ClutterAllocationFlags  flags)
{
  MxPadding padding;
  ClutterActorBox child_box;
  gfloat icon_width, icon_height, label_width, label_height,
         close_width, close_height, preview_width, preview_height;

  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  /* Chain up to store box */
  CLUTTER_ACTOR_CLASS (e_mail_tab_parent_class)->allocate (actor, box, flags);

  /* Possibly synchronise an axis if we're dragging */
  if (priv->in_drag)
    {
      ClutterActor *drag_actor =
        mx_draggable_get_drag_actor (MX_DRAGGABLE (actor));

      if (drag_actor)
        {
          gfloat x, y;
          clutter_actor_get_transformed_position (actor, &x, &y);
          switch (mx_draggable_get_axis (MX_DRAGGABLE (actor)))
            {
            case MX_DRAG_AXIS_X :
              /* Synchronise y axis */
              clutter_actor_set_y (drag_actor, y);
              break;
            case MX_DRAG_AXIS_Y :
              /* Synchronise x axis */
              clutter_actor_set_x (drag_actor, x);
              break;
            default :
              break;
            }
        }
    }

  /* Allocate old background texture */
  if (priv->old_bg)
    {
      child_box.x1 = 0;
      child_box.y1 = 0;
      child_box.x2 = box->x2 - box->x1;
      child_box.y2 = box->y2 - box->y1;
      clutter_actor_allocate (priv->old_bg, &child_box, flags);
    }

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  /* Get the preferred width/height of the icon, label and close-button first */
  if (priv->icon)
    clutter_actor_get_preferred_size (priv->icon, NULL, NULL,
                                      &icon_width, &icon_height);
  clutter_actor_get_preferred_size (CLUTTER_ACTOR (priv->label), NULL, NULL,
                                    &label_width, &label_height);
  if (priv->can_close)
    clutter_actor_get_preferred_size (CLUTTER_ACTOR (priv->close_button),
                                      NULL, NULL, &close_width, &close_height);

  /* Allocate for icon */
  if (priv->icon)
    {
      child_box.x1 = padding.left;
      child_box.x2 = child_box.x1 + icon_width;
      child_box.y1 = E_MAIL_PIXBOUND ((box->y2 - box->y1)/2 - icon_height/2);
      child_box.y2 = child_box.y1 + icon_height;
      clutter_actor_allocate (priv->icon, &child_box, flags);
    }

  /* Allocate for close button */
  if (priv->can_close)
    {
      child_box.x2 = box->x2 - box->x1 - padding.right;
      child_box.x1 = child_box.x2 - close_width;
      child_box.y1 = E_MAIL_PIXBOUND ((box->y2 - box->y1)/2 - close_height/2);
      child_box.y2 = child_box.y1 + close_height;
      clutter_actor_allocate (CLUTTER_ACTOR (priv->close_button),
                              &child_box,
                              flags);
    }

  /* Allocate for preview widget */
  preview_height = 0;
  if (priv->preview)
    {
      preview_width = (box->x2 - box->x1 - padding.left - padding.right);
      preview_height = (box->y2 - box->y1 - padding.top - padding.bottom -
                        priv->spacing - label_height);

      child_box.x1 = E_MAIL_PIXBOUND (padding.left);
      child_box.y1 = E_MAIL_PIXBOUND (padding.top);
      child_box.x2 = child_box.x1 + preview_width;
      child_box.y2 = child_box.y1 + preview_height;
      clutter_actor_allocate (priv->preview, &child_box, flags);
    }

  /* Allocate for label */
  if ((priv->preview_height_progress <= TAB_S1_ANIM) || (!priv->preview))
    {
      if (priv->icon)
        child_box.x1 = E_MAIL_PIXBOUND (padding.left + icon_width + priv->spacing);
      else
        child_box.x1 = E_MAIL_PIXBOUND (padding.left);
      child_box.x2 = (box->x2 - box->x1 - padding.right);
      child_box.y1 = E_MAIL_PIXBOUND ((box->y2 - box->y1)/2 - label_height/2);
      child_box.y2 = child_box.y1 + label_height;

      /* If close button is visible, don't overlap it */
      if (priv->can_close)
        child_box.x2 -= close_width + priv->spacing;
    }
  else
    {
      /* Put label underneath preview */
      child_box.x1 = E_MAIL_PIXBOUND (padding.left);
      child_box.x2 = (box->x2 - box->x1 - padding.right);
      child_box.y1 = E_MAIL_PIXBOUND (padding.top + preview_height +
                                   priv->spacing);
      child_box.y2 = child_box.y1 + label_height;
    }

  clutter_actor_allocate (CLUTTER_ACTOR (priv->label), &child_box, flags);

  /* If we're in preview mode, re-allocate the background so it doesn't
   * encompass the label. (A bit hacky?)
   */
  if (priv->preview && CLUTTER_ACTOR_IS_VISIBLE (priv->preview))
    {
      gfloat max_height = padding.top + padding.bottom + preview_height;
      if (box->y2 - box->y1 > max_height)
        {
          MxWidget *widget = MX_WIDGET (actor);
          ClutterActor *background = mx_widget_get_border_image (widget);

          if (!background)
            background = mx_widget_get_background_image (widget);

          child_box.x1 = 0;
          child_box.x2 = box->x2 - box->x1;
          child_box.y1 = 0;
          child_box.y2 = max_height;

          if (background)
            clutter_actor_allocate (background, &child_box, flags);
          if (priv->old_bg && (priv->old_bg != background))
            clutter_actor_allocate (priv->old_bg, &child_box, flags);
        }
    }
}

static void
e_mail_tab_paint (ClutterActor *actor)
{
  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  /* Chain up to paint background */
  CLUTTER_ACTOR_CLASS (e_mail_tab_parent_class)->paint (actor);

  if (priv->old_bg)
    clutter_actor_paint (priv->old_bg);

  if (priv->icon)
    clutter_actor_paint (priv->icon);

  clutter_actor_paint (CLUTTER_ACTOR (priv->label));

  if (priv->can_close)
    clutter_actor_paint (CLUTTER_ACTOR (priv->close_button));

  if (priv->preview)
    clutter_actor_paint (CLUTTER_ACTOR (priv->preview));
}

static void
e_mail_tab_pick (ClutterActor *actor, const ClutterColor *c)
{
  CLUTTER_ACTOR_CLASS (e_mail_tab_parent_class)->pick (actor, c);

  if (clutter_actor_should_pick_paint (actor))
    e_mail_tab_paint (actor);
}

static void
e_mail_tab_map (ClutterActor *actor)
{
  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  CLUTTER_ACTOR_CLASS (e_mail_tab_parent_class)->map (actor);

  clutter_actor_map (CLUTTER_ACTOR (priv->label));
  clutter_actor_map (CLUTTER_ACTOR (priv->close_button));
  if (priv->icon)
    clutter_actor_map (priv->icon);
  if (priv->preview)
    clutter_actor_map (priv->preview);
  if (priv->old_bg)
    clutter_actor_map (priv->old_bg);
}

static void
e_mail_tab_unmap (ClutterActor *actor)
{
  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  CLUTTER_ACTOR_CLASS (e_mail_tab_parent_class)->unmap (actor);

  clutter_actor_unmap (CLUTTER_ACTOR (priv->label));
  clutter_actor_unmap (CLUTTER_ACTOR (priv->close_button));
  if (priv->icon)
    clutter_actor_unmap (priv->icon);
  if (priv->preview)
    clutter_actor_unmap (priv->preview);
  if (priv->old_bg)
    clutter_actor_unmap (priv->old_bg);
}

static gboolean
e_mail_tab_button_press_event (ClutterActor       *actor,
                            ClutterButtonEvent *event)
{
  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  if (event->button == 1)
    {
      mx_stylable_set_style_pseudo_class (MX_STYLABLE (actor), "active");
      clutter_grab_pointer (actor);
      priv->pressed = TRUE;

      priv->press_x = event->x;
      priv->press_y = event->y;
    }

  /* We have to always return false, or dragging won't work */
  return FALSE;
}

static gboolean
e_mail_tab_button_release_event (ClutterActor       *actor,
                              ClutterButtonEvent *event)
{
  EMailTab *tab = E_MAIL_TAB (actor);
  EMailTabPrivate *priv = tab->priv;

  if (priv->pressed)
    {
      clutter_ungrab_pointer ();
      priv->pressed = FALSE;

      /* Note, no need to set the pseudo class here as clicking always results
       * in being set active.
       */
      if (priv->hover)
        {
          if (!priv->active)
            e_mail_tab_set_active (tab, TRUE);

          g_signal_emit (actor, signals[CLICKED], 0);
        }
    }

  return FALSE;
}

static gboolean
e_mail_tab_motion_event (ClutterActor       *actor,
                      ClutterMotionEvent *event)
{
  EMailTab *tab = E_MAIL_TAB (actor);
  EMailTabPrivate *priv = tab->priv;

  if (priv->pressed && priv->drag_enabled)
    {
      if ((ABS (event->x - priv->press_x) >= priv->drag_threshold) ||
          (ABS (event->y - priv->press_y) >= priv->drag_threshold))
        {
          /* Ungrab the pointer so that the MxDraggable code can take over */
          clutter_ungrab_pointer ();
          priv->pressed = FALSE;
          if (!priv->active)
            {
              if (priv->hover)
                mx_stylable_set_style_pseudo_class (MX_STYLABLE (actor),
                                                    "hover");
              else
                mx_stylable_set_style_pseudo_class (MX_STYLABLE (actor), NULL);
            }
        }
    }

  return FALSE;
}

static gboolean
e_mail_tab_enter_event (ClutterActor         *actor,
                     ClutterCrossingEvent *event)
{
  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  if (event->source != actor)
    return FALSE;

  priv->hover = TRUE;

  if (priv->pressed)
    mx_stylable_set_style_pseudo_class (MX_STYLABLE (actor), "active");
  else if (!priv->active)
    mx_stylable_set_style_pseudo_class (MX_STYLABLE (actor), "hover");

  return FALSE;
}

static gboolean
e_mail_tab_leave_event (ClutterActor         *actor,
                     ClutterCrossingEvent *event)
{
  EMailTabPrivate *priv = E_MAIL_TAB (actor)->priv;

  if ((event->source != actor) ||
      (event->related == (ClutterActor *)priv->close_button))
    return FALSE;

  priv->hover = FALSE;

  if (!priv->active)
    mx_stylable_set_style_pseudo_class (MX_STYLABLE (actor), NULL);

  return FALSE;
}

static void
e_mail_tab_class_init (EMailTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EMailTabPrivate));

  object_class->get_property = e_mail_tab_get_property;
  object_class->set_property = e_mail_tab_set_property;
  object_class->dispose = e_mail_tab_dispose;
  object_class->finalize = e_mail_tab_finalize;

  actor_class->get_preferred_width = e_mail_tab_get_preferred_width;
  actor_class->get_preferred_height = e_mail_tab_get_preferred_height;
  actor_class->button_press_event = e_mail_tab_button_press_event;
  actor_class->button_release_event = e_mail_tab_button_release_event;
  actor_class->motion_event = e_mail_tab_motion_event;
  actor_class->enter_event = e_mail_tab_enter_event;
  actor_class->leave_event = e_mail_tab_leave_event;
  actor_class->allocate = e_mail_tab_allocate;
  actor_class->paint = e_mail_tab_paint;
  actor_class->pick = e_mail_tab_pick;
  actor_class->map = e_mail_tab_map;
  actor_class->unmap = e_mail_tab_unmap;

  g_object_class_install_property (object_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        "Icon",
                                                        "Icon actor.",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        "Text",
                                                        "Tab text.",
                                                        "",
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_CAN_CLOSE,
                                   g_param_spec_boolean ("can-close",
                                                         "Can close",
                                                         "Whether the tab can "
                                                         "close.",
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_TAB_WIDTH,
                                   g_param_spec_int ("tab-width",
                                                     "Tab width",
                                                     "Tab width.",
                                                     -1, G_MAXINT, -1,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_DOCKING,
                                   g_param_spec_boolean ("docking",
                                                         "Docking",
                                                         "Whether the tab "
                                                         "should dock to edges "
                                                         "when scrolled.",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_PREVIEW,
                                   g_param_spec_object ("preview",
                                                        "Preview actor",
                                                        "ClutterActor used "
                                                        "when in preview mode.",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_PREVIEW_MODE,
                                   g_param_spec_boolean ("preview-mode",
                                                         "Preview mode",
                                                         "Whether to display "
                                                         "in preview mode.",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_PREVIEW_DURATION,
                                   g_param_spec_uint ("preview-duration",
                                                      "Preview duration",
                                                      "How long the transition "
                                                      "between preview mode "
                                                      "states lasts, in ms.",
                                                      0, G_MAXUINT, 200,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_SPACING,
                                   g_param_spec_float ("spacing",
                                                       "Spacing",
                                                       "Spacing between "
                                                       "tab elements.",
                                                       0, G_MAXFLOAT,
                                                       6.0,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK |
                                                       G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_PRIVATE,
                                   g_param_spec_boolean ("private",
                                                         "Private",
                                                         "Set if the tab is "
                                                         "'private'.",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "Active",
                                                         "Set if the tab is "
                                                         "active.",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  g_object_class_override_property (object_class,
                                    PROP_DRAG_THRESHOLD,
                                    "drag-threshold");
  g_object_class_override_property (object_class,
                                    PROP_DRAG_AXIS,
                                    "axis");
 // g_object_class_override_property (object_class,
   //                                 PROP_DRAG_CONTAINMENT_TYPE,
     //                               "containment-type");
  g_object_class_override_property (object_class,
                                    PROP_DRAG_CONTAINMENT_AREA,
                                    "containment-area");
  g_object_class_override_property (object_class,
                                    PROP_DRAG_ENABLED,
                                    "drag-enabled");
  g_object_class_override_property (object_class,
                                    PROP_DRAG_ACTOR,
                                    "drag-actor");

  signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EMailTabClass, clicked),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[CLOSED] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EMailTabClass, closed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[TRANSITION_COMPLETE] =
    g_signal_new ("transition-complete",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EMailTabClass, transition_complete),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
e_mail_tab_close_clicked_cb (MxButton *button, EMailTab *self)
{
  g_signal_emit (self, signals[CLOSED], 0);
}

static void
e_mail_tab_anim_completed_cb (ClutterAnimation *animation,
                           EMailTab           *tab)
{
  e_mail_tab_dispose_old_bg (tab);
}

static void
e_mail_tab_style_changed_cb (MxWidget *widget)
{
  EMailTabPrivate *priv = E_MAIL_TAB (widget)->priv;

  /* Don't transition on hover */
  if (g_strcmp0 (mx_stylable_get_style_pseudo_class (MX_STYLABLE (widget)),
                                                     "hover") == 0)
    return;

  if (priv->old_bg)
    {
      if (!clutter_actor_get_parent (priv->old_bg))
        {
          ClutterActorBox box;
          ClutterActor *background;
          ClutterActor *actor = CLUTTER_ACTOR (widget);

          clutter_actor_set_parent (priv->old_bg, actor);

          /* Try to allocate the same size as the background widget,
           * otherwise allocate the same size as the widget itself.
           */
          background = mx_widget_get_border_image (widget);
          if (!background)
            background = mx_widget_get_background_image (widget);

          if (background)
            clutter_actor_get_allocation_box (background, &box);
          else
            {
              clutter_actor_get_allocation_box (actor, &box);
              box.x2 -= box.x1;
              box.y2 -= box.y1;
              box.x1 = 0;
              box.y1 = 0;
            }

          clutter_actor_allocate (priv->old_bg, &box, 0);
        }

      clutter_actor_animate (priv->old_bg,
                             CLUTTER_LINEAR,
                             150,
                             "opacity", 0,
                             "signal::completed",
                               G_CALLBACK (e_mail_tab_anim_completed_cb),
                               widget,
                             NULL);
    }
}

static void
e_mail_tab_stylable_changed_cb (MxStylable *stylable)
{
  EMailTab *tab = E_MAIL_TAB (stylable);
  EMailTabPrivate *priv = tab->priv;

  e_mail_tab_dispose_old_bg (tab);

  priv->old_bg = mx_widget_get_border_image (MX_WIDGET (tab));
  if (priv->old_bg)
    g_object_ref (priv->old_bg);
}

static void
e_mail_tab_dnd_notify_cb (GObject    *settings,
                       GParamSpec *pspec,
                       EMailTab     *tab)
{
  g_object_get (settings,
                "gtk-dnd-drag-threshold", &tab->priv->drag_threshold,
                NULL);
}

static void
e_mail_tab_init (EMailTab *self)
{
  ClutterActor *text;
  GtkSettings *settings;

  EMailTabPrivate *priv = self->priv = TAB_PRIVATE (self);

  priv->width = -1;
  priv->anim_length = 200;
  priv->spacing = 6.0;
  priv->can_close = TRUE;

  priv->label = mx_label_new ();
  g_object_set (G_OBJECT (priv->label), "clip-to-allocation", TRUE, NULL);
  text = mx_label_get_clutter_text (MX_LABEL (priv->label));
  clutter_text_set_ellipsize (CLUTTER_TEXT (text), PANGO_ELLIPSIZE_END);
  clutter_actor_set_parent (CLUTTER_ACTOR (priv->label),
                            CLUTTER_ACTOR (self));

  priv->close_button = mx_button_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (priv->close_button),
                          "tab-close-button");
  clutter_actor_set_parent (CLUTTER_ACTOR (priv->close_button),
                            CLUTTER_ACTOR (self));

  g_signal_connect (priv->close_button, "clicked",
                    G_CALLBACK (e_mail_tab_close_clicked_cb), self);

  /* Connect up styling signals */
  g_signal_connect (self, "style-changed",
                    G_CALLBACK (e_mail_tab_style_changed_cb), NULL);
  g_signal_connect (self, "stylable-changed",
                    G_CALLBACK (e_mail_tab_stylable_changed_cb), NULL);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

  settings = gtk_settings_get_default ();
  priv->drag_threshold_handler =
    g_signal_connect (settings, "notify::gtk-dnd-drag-threshold",
                      G_CALLBACK (e_mail_tab_dnd_notify_cb), self);
  g_object_get (G_OBJECT (settings),
                "gtk-dnd-drag-threshold", &priv->drag_threshold,
                NULL);
}

ClutterActor *
e_mail_tab_new (void)
{
  return g_object_new (E_MAIL_TYPE_TAB, NULL);
}

ClutterActor *
e_mail_tab_new_full (const gchar *text, ClutterActor *icon, gint width)
{
  return g_object_new (E_MAIL_TYPE_TAB,
                       "text", text,
                       "icon", icon,
                       "tab-width", width, NULL);
}

void
e_mail_tab_set_text (EMailTab *tab, const gchar *text)
{
  EMailTabPrivate *priv = tab->priv;

  if (!text)
    text = "";

  priv->has_text = (text[0] != '\0');

  if (priv->label)
    mx_label_set_text (MX_LABEL (priv->label), text);

  g_object_notify (G_OBJECT (tab), "text");
}

void
e_mail_tab_set_default_icon (EMailTab       *tab,
                          ClutterActor *icon)
{
  EMailTabPrivate *priv = tab->priv;
  gboolean changed = !priv->icon || (priv->icon == priv->default_icon);

  if (icon)
    g_object_ref_sink (icon);

  if (priv->default_icon)
    g_object_unref (priv->default_icon);

  priv->default_icon = icon;

  if (changed)
    e_mail_tab_set_icon (tab, NULL);
}

void
e_mail_tab_set_icon (EMailTab *tab, ClutterActor *icon)
{
  EMailTabPrivate *priv = tab->priv;

  /* passing NULL for icon will use default icon if available */

  if (priv->icon)
    clutter_actor_unparent (priv->icon);

  if (icon)
    priv->icon = icon;
  else
    priv->icon = priv->default_icon;

  if (priv->icon)
    {
      clutter_actor_set_parent (priv->icon, CLUTTER_ACTOR (tab));
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));

  g_object_notify (G_OBJECT (tab), "icon");
}

const gchar *
e_mail_tab_get_text (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->label)
    return mx_label_get_text (MX_LABEL (priv->label));
  else
    return NULL;
}

ClutterActor *
e_mail_tab_get_icon (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->icon == priv->default_icon ? NULL : priv->icon;
}

void
e_mail_tab_set_can_close (EMailTab *tab, gboolean can_close)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->can_close == can_close)
    return;

  priv->can_close = can_close;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));

  g_object_notify (G_OBJECT (tab), "can-close");
}

gboolean
e_mail_tab_get_can_close (EMailTab *tab)
{
  return tab->priv->can_close;
}

void
e_mail_tab_set_width (EMailTab *tab,
                   gint    width)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->width == width)
    return;

  priv->width = width;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));

  g_object_notify (G_OBJECT (tab), "tab-width");
}

gint
e_mail_tab_get_width (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->width;
}

void
e_mail_tab_set_docking (EMailTab   *tab,
                     gboolean  docking)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->docking == docking)
    return;

  priv->docking = docking;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));

  g_object_notify (G_OBJECT (tab), "docking");
}

gboolean
e_mail_tab_get_docking (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->docking;
}

void
e_mail_tab_set_preview_actor (EMailTab *tab, ClutterActor *actor)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->preview)
    clutter_actor_unparent (priv->preview);

  priv->preview = actor;

  if (actor)
    {
      clutter_actor_set_parent (actor, CLUTTER_ACTOR (tab));

      clutter_actor_set_opacity (actor, priv->preview_mode ? 0xff : 0x00);
      if (!priv->preview_mode)
        clutter_actor_hide (actor);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));

  g_object_notify (G_OBJECT (tab), "preview");
}

ClutterActor *
e_mail_tab_get_preview_actor (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->preview;
}

static void
preview_new_frame_cb (ClutterTimeline *timeline,
                      guint            msecs,
                      EMailTab          *tab)
{
  gboolean forwards;
  EMailTabPrivate *priv = tab->priv;

  forwards = (clutter_timeline_get_direction (timeline) ==
               CLUTTER_TIMELINE_FORWARD) ? TRUE : FALSE;
  if (priv->preview_mode)
    forwards = !forwards;

  priv->preview_height_progress = clutter_timeline_get_progress (timeline);
  if (forwards)
    priv->preview_height_progress = 1.0 - priv->preview_height_progress;

  if (priv->preview)
    clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));
}

static void
preview_completed_cb (ClutterTimeline *timeline,
                      EMailTab          *tab)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->preview_timeline)
    {
      clutter_timeline_stop (priv->preview_timeline);
      g_object_unref (priv->preview_timeline);
      priv->preview_timeline = NULL;

      if (priv->preview_mode)
        priv->preview_height_progress = 1.0;
      else
        {
          priv->preview_height_progress = 0.0;
          if (priv->preview)
            clutter_actor_hide (priv->preview);
          if (priv->can_close)
            clutter_actor_set_reactive (CLUTTER_ACTOR (priv->close_button),
                                        TRUE);
        }

      /* Remove style hint if we're not in preview mode */
      if (priv->preview)
        {
          if (!priv->preview_mode)
            clutter_actor_set_name (CLUTTER_ACTOR (tab),
                                    priv->private ? "private-tab" : NULL);
        }
      else
        {
          /* If there's no preview actor, disable the tab */
          clutter_actor_set_reactive (CLUTTER_ACTOR (tab), !priv->preview_mode);
        }

      if (priv->preview)
        clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));

      g_signal_emit (tab, signals[TRANSITION_COMPLETE], 0);
    }
}

static void
preview_s1_started_cb (ClutterTimeline *timeline,
                       EMailTab          *tab)
{
  EMailTabPrivate *priv = tab->priv;

  if (!priv->preview)
    clutter_actor_animate_with_timeline (CLUTTER_ACTOR (priv->label),
                                         CLUTTER_EASE_IN_OUT_QUAD,
                                         timeline,
                                         "opacity", 0xff,
                                         NULL);
}

static void
preview_s2_started_cb (ClutterTimeline *timeline,
                       EMailTab          *tab)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->preview)
    clutter_actor_animate_with_timeline (CLUTTER_ACTOR (priv->label),
                                         CLUTTER_EASE_IN_OUT_QUAD,
                                         timeline,
                                         "opacity", 0xff,
                                         NULL);
}

void
e_mail_tab_set_preview_mode (EMailTab *tab, gboolean preview)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->preview_mode != preview)
    {
      ClutterTimeline *timeline, *timeline2;
      gdouble progress, total_duration, duration1, duration2;

      priv->preview_mode = preview;

      /* Disable the close button in preview mode */
      if (preview && priv->can_close)
        clutter_actor_set_reactive (CLUTTER_ACTOR (priv->close_button), FALSE);

#define DEBUG_MULT 1
      if (priv->preview_timeline)
        {
          progress = 1.0 -
                     clutter_timeline_get_progress (priv->preview_timeline);
          clutter_timeline_stop (priv->preview_timeline);
          g_object_unref (priv->preview_timeline);
        }
      else
        progress = 0.0;

      total_duration = priv->anim_length * (1.0 - progress) * DEBUG_MULT;
      duration1 = total_duration * TAB_S1_ANIM;
      duration2 = total_duration * TAB_S2_ANIM;

      priv->preview_timeline =
        clutter_timeline_new (priv->anim_length * DEBUG_MULT);
      clutter_timeline_skip (priv->preview_timeline,
        clutter_timeline_get_duration (priv->preview_timeline) * progress);

      g_signal_connect (priv->preview_timeline, "completed",
                        G_CALLBACK (preview_completed_cb), tab);

      clutter_timeline_start (priv->preview_timeline);

      if (!priv->preview)
        {
          clutter_actor_animate_with_timeline (CLUTTER_ACTOR (tab),
                                               CLUTTER_EASE_IN_OUT_QUAD,
                                               priv->preview_timeline,
                                               "opacity", preview ? 0x00 : 0xff,
                                               NULL);
          return;
        }

      g_signal_connect (priv->preview_timeline, "new-frame",
                        G_CALLBACK (preview_new_frame_cb), tab);

      timeline = clutter_timeline_new ((guint)duration1);
      timeline2 = clutter_timeline_new ((guint)duration2);

      g_signal_connect (timeline, "started",
                        G_CALLBACK (preview_s1_started_cb), tab);
      g_signal_connect (timeline2, "started",
                        G_CALLBACK (preview_s2_started_cb), tab);

      if (preview)
        clutter_timeline_set_delay (timeline2, duration1);
      else
        clutter_timeline_set_delay (timeline, duration2);

      /* clutter_actor_animate_with_timeline will start the timelines */
      clutter_actor_animate_with_timeline (CLUTTER_ACTOR (priv->label),
                                           CLUTTER_EASE_IN_OUT_QUAD,
                                           preview ? timeline : timeline2,
                                           "opacity", 0x00,
                                           NULL);
      if (priv->icon)
        clutter_actor_animate_with_timeline (priv->icon,
                                             CLUTTER_EASE_IN_OUT_QUAD,
                                             timeline,
                                             "opacity", preview ? 0x00 : 0xff,
                                             NULL);
      if (priv->can_close)
        clutter_actor_animate_with_timeline (CLUTTER_ACTOR (priv->close_button),
                                             CLUTTER_EASE_IN_OUT_QUAD,
                                             timeline,
                                             "opacity", preview ? 0x00 : 0xff,
                                             NULL);

      if (priv->preview)
        {
          clutter_actor_show (priv->preview);
          clutter_actor_animate_with_timeline (priv->preview,
                                               CLUTTER_EASE_IN_OUT_QUAD,
                                               timeline2,
                                               "opacity", preview ? 0xff : 0x00,
                                               NULL);
        }

      /* The animations have references on these, drop ours */
      g_object_unref (timeline);
      g_object_unref (timeline2);

      /* Add an actor name, for style */
      clutter_actor_set_name (CLUTTER_ACTOR (tab),
                              priv->private ? "private-preview-tab" :
                                              "preview-tab");
    }
}

gboolean
e_mail_tab_get_preview_mode (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->preview_mode;
}

void
e_mail_tab_set_preview_duration (EMailTab *tab, guint duration)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->anim_length != duration)
    {
      priv->anim_length = duration;
      g_object_notify (G_OBJECT (tab), "preview-duration");
    }
}

guint
e_mail_tab_get_preview_duration (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->anim_length;
}

void
e_mail_tab_set_spacing (EMailTab *tab, gfloat spacing)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->spacing != spacing)
    {
      priv->spacing = spacing;
      g_object_notify (G_OBJECT (tab), "spacing");
      clutter_actor_queue_relayout (CLUTTER_ACTOR (tab));
    }
}

gfloat
e_mail_tab_get_spacing (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->spacing;
}

void
e_mail_tab_set_private (EMailTab *tab, gboolean private)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->private == private)
    return;

  priv->private = private;

  if (!priv->preview_mode)
    clutter_actor_set_name (CLUTTER_ACTOR (tab),
                            private ? "private-tab" : NULL);

  g_object_notify (G_OBJECT (tab), "private");
}

gboolean
e_mail_tab_get_private (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->private;
}

void
e_mail_tab_set_active (EMailTab *tab, gboolean active)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->active == active)
    return;

  priv->active = active;

  g_object_notify (G_OBJECT (tab), "active");

  if (active)
    mx_stylable_set_style_pseudo_class (MX_STYLABLE (tab), "active");
  else if (!priv->pressed)
    {
      if (priv->hover)
        mx_stylable_set_style_pseudo_class (MX_STYLABLE (tab), "hover");
      else
        mx_stylable_set_style_pseudo_class (MX_STYLABLE (tab), NULL);
    }
}

gboolean
e_mail_tab_get_active (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;
  return priv->active;
}

static gboolean
e_mail_tab_alert_cb (EMailTab *tab)
{
  const gchar *name;
  EMailTabPrivate *priv = tab->priv;

  /* FIXME: Work in preview mode */

  /* Alternate between private mode and non-private to alert */
  name = (priv->private ^ (priv->alert_count % 2)) ? NULL : "private-tab";
  if (!priv->preview_mode)
    clutter_actor_set_name (CLUTTER_ACTOR (tab), name);
  priv->alert_count++;

  if (priv->alert_count < 4)
    return TRUE;

  /* This call isn't needed, it should be in the correct state as long as
   * the above check always checks for < (an even number)
   */
  /*if (!priv->preview_mode)
    clutter_actor_set_name (CLUTTER_ACTOR (tab),
                            priv->private ? "private-tab" : NULL);*/
  priv->alert_source = 0;

  return FALSE;
}

void
e_mail_tab_alert (EMailTab *tab)
{
  EMailTabPrivate *priv = tab->priv;

  priv->alert_count = 0;
  if (!priv->alert_source)
    priv->alert_source =
      g_timeout_add_full (G_PRIORITY_HIGH,
                          500,
                          (GSourceFunc)e_mail_tab_alert_cb,
                          tab,
                          NULL);
}

void
e_mail_tab_enable_drag (EMailTab *tab, gboolean enable)
{
  EMailTabPrivate *priv = tab->priv;

  if (priv->drag_enabled == enable)
    return;

  priv->drag_enabled = enable;
  if (enable)
    mx_draggable_enable (MX_DRAGGABLE (tab));
  else
    mx_draggable_disable (MX_DRAGGABLE (tab));
}

