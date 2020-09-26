#include <stdio.h>
#include "hash_table.h"


int main() {
	/* Create new hash table */
	ht_hash_table* ht = ht_new();

	/* insert new key */
	ht_insert(ht, "key", "value");

	/* get key and print value */
	char* value = ht_search(ht, "key");
	printf("%s\n", value);

	ht_del_hash_table(ht);

}
