#include <string.h>

#include "exchange-operations.h"

ExchangeConfigListener *exchange_global_config_listener=NULL;

static void
free_exchange_listener (void)
{
	g_object_unref (exchange_global_config_listener);
}

int
e_plugin_lib_enable (EPluginLib *eplib, int enable)
{
	if (!exchange_global_config_listener) {
		exchange_global_config_listener = exchange_config_listener_new ();
		g_atexit (free_exchange_listener);
	}
	g_print ("*** DEBUG: Exchange config listener is initialized ***\n");
	return 0;
}

gboolean
exchange_operations_tokenize_string (char **string, char *token, char delimit)
{
	int i=0;
	char *str=*string;
	while (*str!=delimit && *str!='\0') {
		token[i++]=*str++;
	}
	while (*str==delimit)
		str++;
	token[i]='\0';
	*string = str;
	if (i==0) 
		return FALSE;
	return TRUE;
}

gboolean
exchange_operations_cta_add_node_to_tree (GtkTreeStore *store, GtkTreeIter *parent, const char *nuri, const char *ruri) 
{
	GtkTreeIter iter;
	char *luri=(char *)nuri;
	char nodename[80];
	gchar *readname;
	gboolean status, found;
	
	g_print ("TOKENIZER: String passed to tokenizer %s\n", luri);
	exchange_operations_tokenize_string (&luri, nodename, '/');
	g_print ("TOKENIZER: Token - %s Residue - %s\n", nodename, luri);
       	if (!nodename[0]) {
		return TRUE;
	}
	if (!strcmp (nodename, "personal") && !parent) {
		strcpy (nodename, "Personal Folders");
	}

	found = FALSE;
	status = gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &iter, parent);
	while (status) {
		g_print ("Reading name...\n");
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &readname, -1);
		g_print ("Name read - %s\n", readname);
		if (!strcmp (nodename, readname)) {
			g_print ("Found. Inserting as child.\n");
			found = TRUE;
			exchange_operations_cta_add_node_to_tree (store, &iter, luri, ruri);
			break;
		}
		g_free (readname);
		status = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
	}
	if (!found) {
		g_print ("Not found. Inserting node %s\n", nodename);
		gtk_tree_store_append (store, &iter, parent);		
		gtk_tree_store_set (store, &iter, 0, nodename, 1, ruri, -1);		
		exchange_operations_cta_add_node_to_tree (store, &iter, luri, ruri);				
	}
	return TRUE;
}

ExchangeAccount *
exchange_operations_get_exchange_account (void) {
	ExchangeAccount *account;
	GSList *acclist;

	acclist = exchange_config_listener_get_accounts (exchange_global_config_listener);
	account = acclist->data; /* FIXME: Need to be changed for handling multiple accounts */
	
	return account;
}

