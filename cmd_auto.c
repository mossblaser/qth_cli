/**
 * Implementation of the automatic-mode command.
 */

#include <stropts.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>

#include "json.h"
#include "MQTTClient.h"

#include "qth_client.h"


int cmd_auto(MQTTClient *client,
             const char *topic,
             char **value,
             value_source_t *value_source,
             cmd_type_t *cmd_type,
             int meta_timeout) {
	char *behaviour = NULL;
	int retval = get_topic_behaviour(client, topic, meta_timeout, &behaviour);
	if (retval != 0) {
		return retval;
	}
	
	// Determine the actual command type
	if (strcmp(behaviour, "PROPERTY-1:N") == 0) {
		*cmd_type = CMD_TYPE_GET;
	} else if (strcmp(behaviour, "PROPERTY-N:1") == 0) {
		*cmd_type = CMD_TYPE_SET;
	} else if (strcmp(behaviour, "EVENT-1:N") == 0) {
		*cmd_type = CMD_TYPE_WATCH;
	} else if (strcmp(behaviour, "EVENT-N:1") == 0) {
		*cmd_type = CMD_TYPE_SEND;
	} else {
		fprintf(stderr, "Error: Topic has unsupported behaviour '%s'.\n", behaviour);
		free(behaviour);
		return 1;
	}
	
	// Amend the value and value_type if required
	if (*cmd_type == CMD_TYPE_SET || *cmd_type == CMD_TYPE_SEND) {
		// Default to 'null' value for writeable commands
		if (*value_source == VALUE_SOURCE_NONE) {
			*value_source = VALUE_SOURCE_NULL;
			*value = "null";
		}
	}
	
	// Fail if an value is supplied when not required
	if (*cmd_type == CMD_TYPE_GET || *cmd_type == CMD_TYPE_WATCH) {
		if (*value_source != VALUE_SOURCE_NONE) {
			fprintf(stderr,
			        "Error: Unexpected value for topic with behaviour '%s'\n",
			        behaviour);
			free(behaviour);
			return 1;
		}
	}
	
	
	free(behaviour);
	return 0;
}
