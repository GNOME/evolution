/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_PART_AUDIO_H
#define E_MAIL_PART_AUDIO_H

#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_AUDIO \
	(e_mail_part_audio_get_type ())
#define E_MAIL_PART_AUDIO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_AUDIO, EMailPartAudio))
#define E_MAIL_PART_AUDIO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_AUDIO, EMailPartAudioClass))
#define E_IS_MAIL_PART_AUDIO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_AUDIO))
#define E_IS_MAIL_PART_AUDIO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_AUDIO))
#define E_MAIL_PART_AUDIO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_AUDIO, EMailPartAudioClass))

G_BEGIN_DECLS

typedef struct _EMailPartAudio EMailPartAudio;
typedef struct _EMailPartAudioClass EMailPartAudioClass;
typedef struct _EMailPartAudioPrivate EMailPartAudioPrivate;

struct _EMailPartAudio {
	EMailPart parent;
	EMailPartAudioPrivate *priv;
};

struct _EMailPartAudioClass {
	EMailPartClass parent_class;
};

GType		e_mail_part_audio_get_type	(void) G_GNUC_CONST;
EMailPart *	e_mail_part_audio_new		(CamelMimePart *mime_part,
						 const gchar *id);

G_END_DECLS

#endif /* E_MAIL_PART_AUDIO_H */

