/*
 * e-mail-part-list.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <camel/camel.h>

#include "e-mail-part-list.h"

struct _EMailPartListPrivate {
	CamelFolder *folder;
	CamelMimeMessage *message;
	gchar *message_uid;
	GPtrArray *autocrypt_keys;

	GQueue queue;
	GMutex queue_lock;
};

enum {
	PROP_0,
	PROP_FOLDER,
	PROP_MESSAGE,
	PROP_MESSAGE_UID
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailPartList, e_mail_part_list, G_TYPE_OBJECT)

static CamelObjectBag *registry = NULL;
G_LOCK_DEFINE_STATIC (registry);

/* takes, aka assumes ownership, of all the pointers, which cannot be NULL */
EMailAutocryptKey *
e_mail_autocrypt_key_new (CamelGpgKeyInfo *info,
			  guint8 *keydata,
			  gsize keydata_size)
{
	EMailAutocryptKey *key;

	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (keydata != NULL, NULL);

	key = g_new0 (EMailAutocryptKey, 1);
	key->info = info;
	key->keydata = keydata;
	key->keydata_size = keydata_size;

	return key;
}

void
e_mail_autocrypt_key_free (EMailAutocryptKey *key)
{
	if (key) {
		camel_gpg_key_info_free (key->info);
		g_free (key->keydata);
		g_free (key);
	}
}

static void
mail_part_list_set_folder (EMailPartList *part_list,
                           CamelFolder *folder)
{
	g_return_if_fail (part_list->priv->folder == NULL);

	/* The folder property is optional. */
	if (folder != NULL) {
		g_return_if_fail (CAMEL_IS_FOLDER (folder));
		part_list->priv->folder = g_object_ref (folder);
	}
}

static void
mail_part_list_set_message (EMailPartList *part_list,
                            CamelMimeMessage *message)
{
	g_return_if_fail (part_list->priv->message == NULL);

	/* The message property is optional. */
	if (message != NULL) {
		g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
		part_list->priv->message = g_object_ref (message);
	}
}

static void
mail_part_list_set_message_uid (EMailPartList *part_list,
                                const gchar *message_uid)
{
	g_return_if_fail (part_list->priv->message_uid == NULL);

	/* The message_uid property is optional. */
	part_list->priv->message_uid = g_strdup (message_uid);
}

static void
mail_part_list_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER:
			mail_part_list_set_folder (
				E_MAIL_PART_LIST (object),
				g_value_get_object (value));
			return;

		case PROP_MESSAGE:
			mail_part_list_set_message (
				E_MAIL_PART_LIST (object),
				g_value_get_object (value));
			return;

		case PROP_MESSAGE_UID:
			mail_part_list_set_message_uid (
				E_MAIL_PART_LIST (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_list_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER:
			g_value_set_object (
				value,
				e_mail_part_list_get_folder (
				E_MAIL_PART_LIST (object)));
			return;

		case PROP_MESSAGE:
			g_value_set_object (
				value,
				e_mail_part_list_get_message (
				E_MAIL_PART_LIST (object)));
			return;

		case PROP_MESSAGE_UID:
			g_value_set_string (
				value,
				e_mail_part_list_get_message_uid (
				E_MAIL_PART_LIST (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_list_dispose (GObject *object)
{
	EMailPartList *self = E_MAIL_PART_LIST (object);

	g_clear_object (&self->priv->folder);
	g_clear_object (&self->priv->message);
	g_clear_pointer (&self->priv->autocrypt_keys, g_ptr_array_unref);

	g_mutex_lock (&self->priv->queue_lock);
	while (!g_queue_is_empty (&self->priv->queue))
		g_object_unref (g_queue_pop_head (&self->priv->queue));
	g_mutex_unlock (&self->priv->queue_lock);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_part_list_parent_class)->dispose (object);
}

static void
mail_part_list_finalize (GObject *object)
{
	EMailPartList *self = E_MAIL_PART_LIST (object);

	g_free (self->priv->message_uid);

	g_warn_if_fail (g_queue_is_empty (&self->priv->queue));
	g_mutex_clear (&self->priv->queue_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_list_parent_class)->finalize (object);
}

static void
e_mail_part_list_class_init (EMailPartListClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_part_list_set_property;
	object_class->get_property = mail_part_list_get_property;
	object_class->dispose = mail_part_list_dispose;
	object_class->finalize = mail_part_list_finalize;

	g_object_class_install_property (
		object_class,
		PROP_FOLDER,
		g_param_spec_object (
			"folder",
			"Folder",
			NULL,
			CAMEL_TYPE_FOLDER,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MESSAGE,
		g_param_spec_object (
			"message",
			"Message",
			NULL,
			CAMEL_TYPE_MIME_MESSAGE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MESSAGE_UID,
		g_param_spec_string (
			"message-uid",
			"Message UID",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_part_list_init (EMailPartList *part_list)
{
	part_list->priv = e_mail_part_list_get_instance_private (part_list);

	g_mutex_init (&part_list->priv->queue_lock);
}

EMailPartList *
e_mail_part_list_new (CamelMimeMessage *message,
                      const gchar *message_uid,
                      CamelFolder *folder)
{
	if (message != NULL)
		g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	if (folder != NULL)
		g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_LIST,
		"message", message,
		"message-uid", message_uid,
		"folder", folder, NULL);
}

CamelFolder *
e_mail_part_list_get_folder (EMailPartList *part_list)
{
	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);

	return part_list->priv->folder;
}

CamelMimeMessage *
e_mail_part_list_get_message (EMailPartList *part_list)
{
	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);

	return part_list->priv->message;
}

const gchar *
e_mail_part_list_get_message_uid (EMailPartList *part_list)
{
	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);

	return part_list->priv->message_uid;
}

void
e_mail_part_list_add_part (EMailPartList *part_list,
                           EMailPart *part)
{
	g_return_if_fail (E_IS_MAIL_PART_LIST (part_list));
	g_return_if_fail (E_IS_MAIL_PART (part));

	g_mutex_lock (&part_list->priv->queue_lock);

	g_queue_push_tail (
		&part_list->priv->queue,
		g_object_ref (part));

	g_mutex_unlock (&part_list->priv->queue_lock);

	e_mail_part_set_part_list (part, part_list);
}

EMailPart *
e_mail_part_list_ref_part (EMailPartList *part_list,
                           const gchar *part_id)
{
	EMailPart *match = NULL;
	GList *head, *link;
	gboolean by_cid;

	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);
	g_return_val_if_fail (part_id != NULL, NULL);

	by_cid = (g_ascii_strncasecmp (part_id, "cid:", 4) == 0);

	g_mutex_lock (&part_list->priv->queue_lock);

	head = g_queue_peek_head_link (&part_list->priv->queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *candidate = E_MAIL_PART (link->data);
		const gchar *candidate_id;

		if (by_cid)
			candidate_id = e_mail_part_get_cid (candidate);
		else
			candidate_id = e_mail_part_get_id (candidate);

		if (g_strcmp0 (candidate_id, part_id) == 0) {
			match = g_object_ref (candidate);
			break;
		}
	}

	g_mutex_unlock (&part_list->priv->queue_lock);

	return match;
}

/**
 * e_mail_part_list_queue_parts:
 * @part_list: an #EMailPartList
 * @part_id: the #EMailPart ID to begin queueing from, or %NULL
 * @result_queue: a #GQueue in which to deposit #EMailPart instances
 *
 * Populates @result_queue with a sequence of #EMailPart instances beginning
 * with the part having @part_id.  If @part_id is %NULL, the entire sequence
 * of #EMailPart instances is queued.
 *
 * Each #EMailPart is referenced for thread-safety and should be unreferenced
 * with g_object_unref().
 *
 * Returns: the number of parts added to @result_queue
 **/
guint
e_mail_part_list_queue_parts (EMailPartList *part_list,
                              const gchar *part_id,
                              GQueue *result_queue)
{
	GList *link;
	guint parts_queued = 0;

	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), FALSE);
	g_return_val_if_fail (result_queue != NULL, FALSE);

	g_mutex_lock (&part_list->priv->queue_lock);

	link = g_queue_peek_head_link (&part_list->priv->queue);

	if (part_id != NULL) {
		for (; link != NULL; link = g_list_next (link)) {
			EMailPart *candidate = E_MAIL_PART (link->data);
			const gchar *candidate_id;

			candidate_id = e_mail_part_get_id (candidate);

			if (g_strcmp0 (candidate_id, part_id) == 0)
				break;
		}
	}

	/* We skip the loop entirely if link is NULL. */
	for (; link != NULL; link = g_list_next (link)) {
		EMailPart *part = link->data;

		if (part == NULL)
			continue;

		g_queue_push_tail (result_queue, g_object_ref (part));
		parts_queued++;
	}

	g_mutex_unlock (&part_list->priv->queue_lock);

	return parts_queued;
}

/**
 * e_mail_part_list_is_empty:
 * @part_list: an #EMailPartList
 *
 * Returns: whether the part list is empty (it doesn't contain any #EMailpart).
 **/
gboolean
e_mail_part_list_is_empty (EMailPartList *part_list)
{
	gboolean is_empty;

	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), TRUE);

	g_mutex_lock (&part_list->priv->queue_lock);
	is_empty = g_queue_is_empty (&part_list->priv->queue);
	g_mutex_unlock (&part_list->priv->queue_lock);

	return is_empty;
}

void
e_mail_part_list_sum_validity (EMailPartList *part_list,
			       EMailPartValidityFlags *out_validity_pgp_sum,
			       EMailPartValidityFlags *out_validity_smime_sum)
{
	EMailPartValidityFlags validity_pgp_sum = 0;
	EMailPartValidityFlags validity_smime_sum = 0;
	GQueue queue = G_QUEUE_INIT;

	g_return_if_fail (E_IS_MAIL_PART_LIST (part_list));

	e_mail_part_list_queue_parts (part_list, NULL, &queue);

	while (!g_queue_is_empty (&queue)) {
		EMailPart *part = g_queue_pop_head (&queue);
		GList *head, *link;

		head = g_queue_peek_head_link (&part->validities);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPartValidityPair *vpair = link->data;

			if (vpair == NULL)
				continue;

			if ((vpair->validity_type & E_MAIL_PART_VALIDITY_PGP) != 0)
				validity_pgp_sum |= vpair->validity_type;
			if ((vpair->validity_type & E_MAIL_PART_VALIDITY_SMIME) != 0)
				validity_smime_sum |= vpair->validity_type;
		}

		g_object_unref (part);
	}

	if (out_validity_pgp_sum)
		*out_validity_pgp_sum = validity_pgp_sum;

	if (out_validity_smime_sum)
		*out_validity_smime_sum = validity_smime_sum;
}


GPtrArray * /* EMailAutocryptKey * */
e_mail_part_list_get_autocrypt_keys (EMailPartList *part_list)
{
	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);

	return part_list->priv->autocrypt_keys;
}

void
e_mail_part_list_take_autocrypt_keys (EMailPartList *part_list,
				      GPtrArray *keys) /* EMailAutocryptKey * */
{
	g_return_if_fail (E_IS_MAIL_PART_LIST (part_list));

	if (part_list->priv->autocrypt_keys != keys) {
		g_clear_pointer (&part_list->priv->autocrypt_keys, g_ptr_array_unref);
		part_list->priv->autocrypt_keys = keys;
	}
}

/**
 * e_mail_part_list_get_registry:
 *
 * Returns a #CamelObjectBag where parsed #EMailPartLists can be stored.
 */
CamelObjectBag *
e_mail_part_list_get_registry (void)
{
	G_LOCK (registry);
	if (registry == NULL) {
		registry = camel_object_bag_new (
				g_str_hash, g_str_equal,
				(CamelCopyFunc) g_strdup, g_free);
	}
	G_UNLOCK (registry);

	return registry;
}
