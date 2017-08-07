#include <stdio.h>
#include <stdlib.h>

#include "qth_client.h"


int main(int argc, char *argv[]) {
	argparse(argc, argv);
	
	//const char *one_liner = json_validate("[\"\n\", {\"a\": 456}]");
	char *err = json_validate("1");
	if (err) {
		printf("ERROR: %s<<<\n", err);
		free(err);
	} else {
		printf("no prob\n");
	}
	//free(one_liner);
	
	return 0;
}


