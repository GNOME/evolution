/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, Helix Code, Inc.
 */

#include <ebook/e-book.h>

/**
 * e_book_new:
 * @uri: A URI string describing the location of the backend
 * which the new #EBook will use.
 *
 * Returns: A new #EBook object, bound to the backend specified
 * by @uri, or %NULL if an error occurs.
 */
EBook *
e_book_new (const char *uri)
{
	e_book_get_card (
}

/**
 * e_book_get_type:
 *
 * Returns: The #GtkType for #EBook.
 */
GtkType
e_book_get_type (void)
{
}

/**
 * e_book_get_card:
 * @book: An #EBook object.
 * @id: A unique ID which specified a card stored inside
 * @book's backend store.
 *
 * This function fetches the card specified by @id from @book's
 * backend, parses it into a new #ECard object, and returns it.
 *
 * Returns: A newly created #ECard, filled with the card data
 * specified by @id.  The #EBook may maintain an internal card cache,
 * and will therefore hold a reference to each #ECard itself.  The
 * reference count on the returned #ECard will be incremented when the
 * card is returned.  When the client wishes to destroy the returned
 * #ECard, he should just dereference it.
 */
ECard *
e_book_get_card (EBook *book, const char *id)
{
}

/**
 * e_book_get_cards:
 * @book: An #EBook object.
 *
 * Returns: A #GList of all the #ECards stored in @book's backend.
 * The #GList is newly-allocated for the client and must be freed by
 * him.  The #ECard objects in the #GList are each referenced, and
 * must be unreferenced when the client is done using them.
 */
GList *
e_book_get_cards (EBook *book)
{
}

/**
 * e_book_get_ids:
 * @book: An #EBook object.
 *
 * Returns: A #GList of all the unique card ID strings stored in
 * @book's backend.  The #GList is newly-allocated, as are all of the
 * #ECard objects stored in it.  The client must free the #GList and
 * unreference all fo the cards when he is done with them.
 */
GList *
e_book_get_ids (EBook *book)
{
}

/**
 * e_book_sync_card:
 * @book: An #EBook object.
 * @card: A dirty #ECard object.
 *
 * Writes all the changes in @card into @book's card store.
 */
void
e_book_sync_card (EBook *book)
{
}

/**
 * e_book_update_card:
 * @book: An #EBook object.
 * @card: An #ECard object which has become out-of-date
 * and no longer contains the most current card data
 * in @book's card store.
 *
 * Updates @card with any changes which may have occured in its
 * corresponding backend data.
 */
void
e_book_update_card (EBook *book, ECard *card)
{
}

/**
 * e_book_add_card:
 * @book: An #EBook object.
 * @card: A newly-created ECard object.
 *
 * Adds @card to @book's card store.  Creates a unique ID for @card
 * and sets @card's ID field.  This action will cause a #card_added
 * signal to be raised on @book.
 *
 * Returns: The newly-created unique ID for @card.  The copy
 * returned is the same copy which is stored in @card.  The #EBook
 * may reference @card and keep a handle to it.
 */
const char *
e_book_add_card (EBook *book, ECard *card)
{
}

/**
 * e_book_remove_card:
 * @book: An #EBook object.
 * @id: A unique ID for a card stored in @book.
 *
 * Removes the card specified by @id from @book's card store.  If the
 * client has kept around an old #ECard object for the card being
 * removed, he will have to remove it himself.  The #ECard will not
 * receive a #card_removed signals.  A #card_removed signal will be
 * raised on @book, the card will be removed from the #EBook card
 * cache, and the corresponding #ECard's reference count will be
 * decremented.
 */
void
e_book_remove_card (EBook *book, const char *id)
{
}

/**
 * e_book_complete:
 * @book: An #EBook object.
 * @str: A string.
 *
 * The purpose of this function is to provide an easy way for the
 * client application to to implement typing completion in its address
 * entry dialogs.  The #EBook will compute a list of cards which
 * potentially complete @str.  The basis for the completion (address,
 * nickname, etc) is implementation-dependent, and I may add some
 * configurability to this later.
 * 
 * Returns: A list of #ECard objects which are potentially what the
 * user was getting at when he typed @str.  The list is sorted in
 * descending order of likelihood.  The returned #GList must be freed
 * by the client.  The #ECard objects in the #GList may have come from
 * @book's cache, and the client may already hold other references to
 * them.  For this reason, the reference count on each #ECard object
 * is incremented when the object is returned.
 */
GList *
e_book_complete (EBook *book, const char *str)
{
}       

/**
 * e_book_get_name:
 * @book: An #EBook object.
 *
 * Returns: The name of the card store to which @book is bound.
 * The returned string must be freed by the client.
 */
char *
e_book_get_name (EBook *book)
{
}

/**
 * e_book_set_name:
 * @book: An #EBook object.
 * @name: A string containing a new name for @book.
 *
 * Sets @book's name to @name.
 */
char *
e_book_set_name (EBook *book, const char *namen)
{
}
