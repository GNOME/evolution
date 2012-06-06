/*
 * e-mail-part-list.c
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

#include <camel/camel.h>

#include "e-mail-part-list.h"

G_DEFINE_TYPE (EMailPartList, e_mail_part_list, G_TYPE_OBJECT)

static void
unref_mail_part (gpointer user_data)
{
	if (user_data)
		e_mail_part_unref (user_data);
}

static void
e_mail_part_list_finalize (GObject *object)
{
	EMailPartList *part_list = E_MAIL_PART_LIST (object);

	g_clear_object (&part_list->folder);
	g_clear_object (&part_list->message);

	if (part_list->list) {
		g_slist_free_full (part_list->list, unref_mail_part);
		part_list->list = NULL;
	}

	if (part_list->message_uid) {
		g_free (part_list->message_uid);
		part_list->message_uid = NULL;
	}

	G_OBJECT_CLASS (e_mail_part_list_parent_class)->finalize (object);
}

static void
e_mail_part_list_class_init (EMailPartListClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = e_mail_part_list_finalize;
}

static void
e_mail_part_list_init (EMailPartList *part_list)
{

}

EMailPartList *
e_mail_part_list_new ()
{
	return g_object_new (E_TYPE_MAIL_PART_LIST, NULL);
}

EMailPart *
e_mail_part_list_find_part (EMailPartList *part_list,
                            const gchar *id)
{
	GSList *iter;
	gboolean by_cid;

	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);
	g_return_val_if_fail (id && *id, NULL);

	by_cid = (g_str_has_prefix (id, "cid:") || g_str_has_prefix (id, "CID:"));

	for (iter = part_list->list; iter; iter = iter->next) {

		EMailPart *part = iter->data;
		if (!part)
			continue;

		if ((by_cid && (g_strcmp0 (part->cid, id) == 0)) ||
		    (!by_cid && (g_strcmp0 (part->id, id) == 0)))
			return part;
	}

	return NULL;
}

/**
 * e_mail_part_list_get_iter:
 * @part_list: a #GSList of #EMailPart
 * @id: id of #EMailPart to lookup
 *
 * Returns iter of an #EMailPart within the @part_list.
 *
 * Return Value: a #GSList sublist. The list is owned by #EMailPartList and
 * must not be freed or altered.
 */
GSList *
e_mail_part_list_get_iter (GSList *list,
                           const gchar *id)
{
	GSList *iter;

	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (id && *id, NULL);

	for (iter = list; iter; iter = iter->next) {

		EMailPart *part = iter->data;
		if (!part)
			continue;

		if (g_strcmp0 (part->id, id) == 0)
			return iter;
	}

	return NULL;
}
