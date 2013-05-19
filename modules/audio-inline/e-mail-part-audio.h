/*
 * e-mail-part-audio.h
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

#ifndef E_MAIL_PART_AUDIO_H
#define E_MAIL_PART_AUDIO_H

#include <glib-object.h>

#include <em-format/e-mail-part.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _EMailPartAudio EMailPartAudio;

struct _EMailPartAudio {
	EMailPart parent;

	gchar *filename;
	GstElement *playbin;
	gulong      bus_id;
	GstState    target_state;
	GtkWidget  *play_button;
	GtkWidget  *pause_button;
	GtkWidget  *stop_button;
};

G_END_DECLS

#endif /* E_MAIL_PART_AUDIO_H */

