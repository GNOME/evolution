/*
  ECompleteTest
*/

#include <gnome.h>
#include "e-completion.h"
#include "e-entry.h"

#define TIMEOUT 50

/* Dictionary Lookup test */

static gint word_count = 0;
static gchar **word_array = NULL;

static void
read_dict (void)
{
  FILE *in = fopen ("/usr/share/dict/words", "r");
  gchar buffer[128];
  GList *word_list = NULL, *iter;
  gint i;

  while (fgets (buffer, 128, in)) {
    gint len = strlen (buffer);
    if (len > 0 && buffer[len-1] == '\n')
      buffer[len-1] = '\0';
    word_list = g_list_prepend (word_list, g_strdup (buffer));
    ++word_count;
  }
  fclose (in);

  word_array = g_new (gchar *, word_count);
  i = word_count-1;
  for (iter = word_list; iter != NULL; iter = g_list_next (iter)) {
    word_array[i] = (gchar *)iter->data;
    --i;
  }
}

static gint
find_word (const gchar *str)
{
  gint a, b;

  if (word_array == NULL)
    read_dict ();

  a = 0;
  b = word_count-1;

  while (b-a > 1) {
    gint m = (a+b)/2;
    gint cmp = g_strcasecmp (str, word_array[m]);

    if (cmp < 0)
      b = m;
    else if (cmp > 0)
      a = m;
    else
      return m;
  };

  return b;
}

struct {
  ECompletion *complete;
  const gchar *txt;
  gint start;
  gint current;
  gint len;
  gint limit;
} dict_info;
static guint dict_tag = 0;

static gboolean
dict_check (gpointer ptr)
{
  gint limit = dict_info.limit;
  gint i;

  /* If this is the first iteration, do the binary search in our word list to figure out
     where to start.  We do less work on the first iteration, to give more of a sense of
     immediate feedback. */
  if (dict_info.start < 0) {
    dict_info.start = dict_info.current = find_word (dict_info.txt);
  }

  i = dict_info.current;
  while (limit > 0 && i < word_count && g_strncasecmp (dict_info.txt, word_array[i], dict_info.len) == 0) {
    e_completion_found_match_full (dict_info.complete, word_array[i],
				   dict_info.len / (double)strlen (word_array[i]),
				   NULL, NULL);
    ++i;
    --limit;
  }
  dict_info.current = i;
  dict_info.limit = MIN (dict_info.limit*2, 200);
  
  if (limit != 0) {
    dict_tag = 0;
    e_completion_end_search (dict_info.complete);
    return FALSE;
  }



  return TRUE;
}

static void
begin_dict_search (ECompletion *complete, const gchar *txt, gint pos, gint limit, gpointer user_data)
{
  gint len = strlen (txt);

  if (dict_tag != 0) {
    gtk_timeout_remove (dict_tag);
    dict_tag = 0;
  }

  if (len > 0) {
    dict_info.complete = complete;
    dict_info.txt = txt;
    dict_info.start = -1;
    dict_info.current = -1;
    dict_info.len = len;
    dict_info.limit = 20;
    dict_tag = gtk_timeout_add (TIMEOUT, dict_check, NULL);
  } else {
    g_message ("halting");
    e_completion_end_search (complete);
  }
}

static void
end_dict_search (ECompletion *complete, gpointer user_data)
{
  if (dict_tag != 0) {
    gtk_timeout_remove (dict_tag);
    dict_tag = 0;
  }
}

int
main (int argc, gchar **argv)
{
  ECompletion* complete;
  GtkWidget *entry;
  GtkWidget *win;

  gnome_init ("ETextModelTest", "0.0", argc, argv);

  read_dict ();

  complete = e_completion_new ();
  gtk_signal_connect (GTK_OBJECT (complete),
		      "begin_completion",
		      GTK_SIGNAL_FUNC (begin_dict_search),
		      NULL);
  gtk_signal_connect (GTK_OBJECT (complete),
		      "end_completion",
		      GTK_SIGNAL_FUNC (end_dict_search),
		      NULL);
  gtk_signal_connect (GTK_OBJECT (complete),
		      "cancel_completion",
		      GTK_SIGNAL_FUNC (end_dict_search),
		      NULL);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  entry = e_entry_new ();
  e_entry_enable_completion_full (E_ENTRY (entry), complete, -1, NULL);
  e_entry_set_editable (E_ENTRY (entry), TRUE);

  gtk_container_add (GTK_CONTAINER (win), entry);
  gtk_widget_show_all (win);

  gtk_main ();

  return 0;
}
