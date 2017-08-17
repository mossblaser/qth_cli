/**
 * Implementation of the get, set, delete, watch and send commands.
 */

#include <stropts.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>

#include "json.h"
#include "MQTTClient.h"

#include "qth_client.h"

/**
 * Read a line from stdin, discarding the newline character. The returned
 * string should be freed by the caller. If the stream is closed, returns NULL.
 * While reading, keeps the MQTTClient alive.
 */
char *getline_and_keepalive(void) {
	size_t len = 64 * 1024;  // Should be enough for anybody...
	char *out = malloc(len);
	out[0] = 0;
	
	bool data_ready = false;
	while (!data_ready) {
		// Watch stdin
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		
		// Timing out after 1ms (to allow frequent calls to MQTTClient_yield).
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		
		int select_retval = select(1, &rfds, NULL, NULL, &tv);
		if (select_retval == -1) {
			// Some error occurred
			free(out);
			return NULL;
		} else if (select_retval == 0) {
			// Timeout
			MQTTClient_yield();
		} else {
			// Read ready!
			data_ready = true;
		}
	}
	
	// We rely on receiving a whole line in one go otherwise the MQTT will not be
	// kept alive.
	char *c = fgets(out, len-1, stdin);
	if (c == NULL) {
		// EOF
		free(out);
		return NULL;
	} else {
		// Trim off trailing newline
		size_t nl_pos = strlen(c) - 1;
		if (c[nl_pos] == '\n') {
			c[nl_pos] = '\0';
		}
		
		return out;
	}
}


int cmd_set_delete_or_send(MQTTClient *client, const char *topic,
                           const char *value, bool is_registering,
                           bool is_property, bool force,
                           int count, int timeout, int meta_timeout) {
	// Verify that the type is as expected
	if (!force) {
		char *desired_behaviour;
		if (is_property) {
			desired_behaviour = is_registering ? "PROPERTY-1:N" : "PROPERTY-N:1";
		} else {
			desired_behaviour = is_registering ? "EVENT-1:N" : "EVENT-N:1";
		}
		if (verify_topic(client, topic, desired_behaviour, meta_timeout)) {
			return 1;
		}
	}
	
	// Set the value accordingly
	while (true) {
		// Get the value to be sent
		char *value_to_send = value ? alloced_copy(value) : NULL;
		if (!value_to_send) {
			value_to_send = getline_and_keepalive();
			if (!value_to_send) {
				// Stop at end of file
				break;
			}
			
			// Replace empty lines with 'null'.
			if (value_to_send[0] == '\0') {
				free(value_to_send);
				value_to_send = alloced_copy("null");
			}
			char *err = json_validate(value_to_send, -1);
			if (err) {
				free(value_to_send);
				fprintf(stderr, "Error: Value must be valid JSON: %s\n", err);
				free(err);
				return 1;
			}
		}
		
		// Send the value
		char *err = qth_set_delete_or_send(client, topic, value_to_send, is_property, timeout);
		free(value_to_send);
		if (err) {
			fprintf(stderr, "Error: %s\n", err);
			free(err);
			return 1;
		}
		
		// Repeat?
		if (count > 0) {
			if (--count == 0) {
				break;
			}
		}
	}
	
	return 0;
}

int cmd_set(MQTTClient *client,
            const char *topic,
            const char *value,
            bool is_registering,
            bool force,
            int count,
            int timeout,
            int meta_timeout) {
	return cmd_set_delete_or_send(client, topic, value,
	                              is_registering, true, force,
	                              count, timeout, meta_timeout);
}

int cmd_delete(MQTTClient *client,
               const char *topic,
               bool is_registering,
               bool force,
               int timeout,
               int meta_timeout) {
	return cmd_set_delete_or_send(client, topic, "",
	                              is_registering, true, force,
	                              1, timeout, meta_timeout);
}

int cmd_send(MQTTClient *client,
             const char *topic,
             const char *value,
             bool is_registering,
             bool force,
             int count,
             int timeout,
             int meta_timeout) {
	return cmd_set_delete_or_send(client, topic, value,
	                              is_registering, false, force,
	                              count, timeout, meta_timeout);
}


int cmd_get_or_watch(MQTTClient *client, const char *topic,
                     json_format_t json_format, bool is_registering,
                     bool is_property, bool force,
                     int count, int timeout, int meta_timeout) {
	// Verify that the type is as expected
	if (!force) {
		char *desired_behaviour;
		if (is_property) {
			desired_behaviour = is_registering ? "PROPERTY-N:1" : "PROPERTY-1:N";
		} else {
			desired_behaviour = is_registering ? "EVENT-N:1" : "EVENT-1:N";
		}
		if (verify_topic(client, topic, desired_behaviour, meta_timeout)) {
			return 1;
		}
	}
	
	// Subscribe
	int err = MQTTClient_subscribe(client, topic, 2);
	if (err != MQTTCLIENT_SUCCESS) {
		fprintf(stderr, "Error: Could not subscribe to topic.\n");
		return 1;
	}
	
	// Watch the value over time (breaking out of the loop upon failure rather
	// than returning)
	int return_code = 0;
	while (return_code == 0) {
		// Receive the message, waiting for as long as necessary if the timeout is
		// specified as zero.
		char *rx_topic = NULL;
		int rx_topic_len = 0;
		MQTTClient_message *message = NULL;
		while (message == NULL) {
			int err = MQTTClient_receive(client, &rx_topic, &rx_topic_len, &message,
			                             timeout ? timeout : 1000);
			if (err != MQTTCLIENT_SUCCESS) {
				fprintf(stderr, "Error: Unable to recieve MQTT message.\n");
				return_code = 1;
				break;
			}
			if (timeout > 0) {
				break;
			}
		}
		if (return_code != 0) {
			break;
		}
		
		if (message == NULL) {
			if (is_property) {
				fprintf(stderr, "Error: Timeout (property may not have been set).\n");
			} else {
				fprintf(stderr, "Error: Timeout.\n");
			}
			return_code = 1;
			break;
		}
		
		// Verify the topic is what we asked for
		if (strncmp(rx_topic, topic, rx_topic_len) != 0) {
			fprintf(stderr, "Error: Received message from unexpected topic.\n");
			MQTTClient_free(rx_topic);
			MQTTClient_freeMessage(&message);
			return_code = 1;
			break;
		}
		
		// Get a null-terminated copy of the payload
		char *payload = alloced_copyn(message->payload, message->payloadlen);
		MQTTClient_free(rx_topic);
		MQTTClient_freeMessage(&message);
		
		if (strlen(payload) == 0) {
			if (is_property) {
				fprintf(stderr, "Error: Property was deleted.\n");
			} else {
				fprintf(stderr, "Error: Empty (non-JSON) event payload received.\n");
			}
			free(payload);
			return_code = 1;
			break;
		}
		char *err = json_validate(payload, -1);
		if (err) {
			fprintf(stderr, "Error: Not a valid JSON value: %s\n", err);
			free(err);
			free(payload);
			return_code = 1;
			break;
		}
		
		// Output the received message
		char *out = json_to_format(payload, json_format);
		printf("%s\n", out);
		free(out);
		
		// Clean up
		free(payload);
		
		// Repeat?
		if (count > 0) {
			if (--count == 0) {
				break;
			}
		}
	}
	
	// Unsubscribe again
	if (MQTTClient_unsubscribe(client, topic) != MQTTCLIENT_SUCCESS) {
		fprintf(stderr, "Error: Unable to recieve MQTT message.\n");
	}
	
	return return_code;;
}

int cmd_get(MQTTClient *client, const char *topic,
            json_format_t json_format, bool is_registering,
            bool force, int count, int timeout, int meta_timeout) {
	return cmd_get_or_watch(client, topic, json_format, is_registering, true,
	                        force, count, timeout, meta_timeout);
}

int cmd_watch(MQTTClient *client, const char *topic,
              json_format_t json_format, bool is_registering,
              bool force, int count, int timeout, int meta_timeout) {
	return cmd_get_or_watch(client, topic, json_format, is_registering, false,
	                        force, count, timeout, meta_timeout);
}
