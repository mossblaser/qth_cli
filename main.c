#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "json.h"
#include "MQTTClient.h"

#include "qth_client.h"

MQTTClient mqtt_client;
MQTTClient_connectOptions mqtt_opts = MQTTClient_connectOptions_initializer;
MQTTClient_willOptions mqtt_will_opts = MQTTClient_willOptions_initializer;


/**
 * Generate a Paho-MQTT compatible connection URL from a hostname and port
 * number. The user must free the resulting string when they're finished with
 * it.
 */
char *get_mqtt_url(const char *host, int port) {
	size_t mqtt_addr_len = 6 +  // tcp://
	                       strlen(host) +  // hostname
	                       1 +  // :
	                       ceil(log(port) / log(10)) +  // port
	                       1;  // null
	char *mqtt_addr = malloc(mqtt_addr_len);
	snprintf(mqtt_addr, mqtt_addr_len, "tcp://%s:%d", host, port);
	return mqtt_addr;
}

/**
 * Sanitise a string for use as a client ID. Replaces disallowed characters
 * with '-'.
 */
void sanitise_client_id(char *client_id) {
	for (char *c = client_id; *c != '\0'; c++) {
		if (!((*c >= 'A' && *c <= 'Z') ||
		      (*c >= 'a' && *c <= 'z') ||
		      (*c >= '0' && *c <= '9') ||
		      *c == '-' || *c == '_' || *c == '.' || *c == ':')) {
			*c = '-';
		}
	}
}

/**
 * Generate a random client ID, the caller should free the memory used and seed
 * the random number generator with srand.
 */
char *get_random_client_id(const char *app_name) {
	// Get the system hostname
	const size_t max_hostname_len = 20;
	char hostname[max_hostname_len];
	gethostname(hostname, max_hostname_len);
	hostname[max_hostname_len-1] = '\0';
	
	// Process ID
	int pid = (int)getpid();
	size_t pid_len = ceil(log(pid)/log(10));
	
	// Random ID
	const size_t random_part_len = 5;
	char random_part[random_part_len + 1];
	for (size_t i = 0; i < random_part_len; i++) {
		random_part[i] = '0' + (int)floor((10.0 * rand()) / (RAND_MAX + 1.0));
	}
	random_part[random_part_len] = '\0';
	
	// Assemble the string
	size_t client_id_len = sizeof(app_name) +  // app name
	                       1 +  // -
	                       sizeof(hostname) +  // hostname
	                       1 +  // -
	                       pid_len +  // pid
	                       1 +  // -
	                       random_part_len +  // random part
	                       1; // null
	char *client_id = malloc(client_id_len);
	snprintf(client_id, client_id_len, "%s-%s-%d-%s",
	         app_name, hostname, pid, random_part);
	return client_id;
}

/**
 * Given a Client ID, return the URL to send registration details to.
 */
char *get_registration_url(const char *client_id) {
	size_t url_len = 13 +  // meta/clients/
	                 strlen(client_id) +  // client ID
	                 1;  // null
	char *url = malloc(url_len);
	snprintf(url, url_len, "meta/clients/%s", client_id);
	return url;
}


/**
 * Return a JSON formatted registration string (to be freed by the caller).
 */
char *get_registration_msg(const char *topic, const char *description,
                           cmd_type_t cmd_type, const char *on_unregister,
                           bool delete_on_unregister) {
	const char *behaviour = NULL;
	switch (cmd_type) {
		case CMD_TYPE_GET: behaviour = "PROPERTY-N:1"; break;
		case CMD_TYPE_SET: behaviour = "PROPERTY-1:N"; break;
		case CMD_TYPE_WATCH: behaviour = "EVENT-N:1"; break;
		case CMD_TYPE_SEND: behaviour = "EVENT-1:N"; break;
		default: behaviour = NULL; break;
	}
	if (behaviour) {
		// Create the registration message
		json_object *reg = json_object_new_object();
		json_object_object_add(reg, "description",
			json_object_new_string("An instance of the Qth commandline tool."));
		
		json_object *topics = json_object_new_object();
		json_object_object_add(reg, "topics", topics);
		
		json_object *topic_obj = json_object_new_object();
		json_object_object_add(topics, topic, topic_obj);
		
		json_object_object_add(topic_obj, "behaviour",
			json_object_new_string(behaviour));
		json_object_object_add(topic_obj, "description",
			json_object_new_string(description));
		
		if (on_unregister) {
			json_object_object_add(topic_obj, "on_unregister",
				json_tokener_parse(on_unregister));
		}
		if (delete_on_unregister) {
			json_object_object_add(topic_obj, "delete_on_unregister",
				json_object_new_boolean(true));
		}
		
		char *out = alloced_copy(json_object_to_json_string(reg));
		json_object_put(reg);
		return out;
	} else {
		return NULL;
	}
}


int main(int argc, char *argv[]) {
	setlinebuf(stdin);
	setlinebuf(stdout);
	
	options_t opts = argparse(argc, argv);
	
	// Use a random client ID if required
	srand(time(NULL));
	char *random_client_id = get_random_client_id(argv[0]);
	if (!opts.client_id) {
		opts.client_id = random_client_id;
	}
	sanitise_client_id(opts.client_id);
	
	// Setup registration details
	char *registration_url = get_registration_url(opts.client_id);
	char *registration_msg = get_registration_msg(opts.topic,
	                                              opts.description,
	                                              opts.cmd_type,
	                                              opts.on_unregister,
	                                              opts.delete_on_unregister);
	
	// Create an MQTT connection
	char *mqtt_url = get_mqtt_url(opts.mqtt_host, opts.mqtt_port);
	if (MQTTClient_create(&mqtt_client, mqtt_url, opts.client_id,
	                      MQTTCLIENT_PERSISTENCE_NONE, NULL) != 0) {
		printf("Couldn't create an MQTT connection object!\n");
		return 1;
	}
	
	// Setup connection options
	mqtt_opts.keepAliveInterval = opts.mqtt_keep_alive;
	mqtt_opts.reliable = 0;
	mqtt_opts.cleansession = 1;
	
	// Setup a will to unregister the client, if required.
	if (opts.register_topic) {
		mqtt_opts.will = &mqtt_will_opts;
		mqtt_will_opts.topicName = registration_url;
		mqtt_will_opts.message = "";
		mqtt_will_opts.retained = 1;
	} else {
		// No Will required if not registering
		mqtt_opts.will = NULL;
	}
	
	// Connect to MQTT
	if (MQTTClient_connect(mqtt_client, &mqtt_opts) != 0) {
		printf("Couldn't connect to MQTT broker!\n");
		return 1;
	}
	
	// Register with the server
	if (opts.register_topic) {
		char *err = qth_set_property(mqtt_client, registration_url,
		                             registration_msg, opts.meta_timeout);
		if (err) {
			fprintf(stderr, "Error: Couldn't register: %s\n", err);
			free(err);
			return 1;
		}
	}
	
	// If automatic, work out what command is needed.
	if (opts.cmd_type == CMD_TYPE_AUTO) {
		int retval = cmd_auto(mqtt_client,
	                        opts.strict,
	                        opts.topic,
	                        &opts.value,
	                        &opts.value_source,
	                        &opts.cmd_type,
	                        opts.meta_timeout);
		if (retval != 0) {
			return retval;
		}
		
		// Don't make the command check the topic type a second time
		opts.force = true;
	}
	
	// Perform the requested operation.
	int retval = 1;
	switch (opts.cmd_type) {
		case CMD_TYPE_LS:
			retval = cmd_ls(mqtt_client,
			                opts.topic,
			                opts.meta_timeout,
			                opts.ls_recursive,
			                opts.ls_format,
			                opts.json_format);
			break;
		
		case CMD_TYPE_GET:
			retval = cmd_get(mqtt_client,
			                 opts.topic,
			                 opts.json_format,
			                 opts.register_topic,
			                 opts.strict,
			                 opts.force,
			                 opts.get_count,
			                 opts.get_timeout,
			                 opts.meta_timeout);
			break;
		
		case CMD_TYPE_SET:
			retval = cmd_set(mqtt_client,
			                 opts.topic,
			                 opts.value,
			                 opts.register_topic,
			                 opts.strict,
			                 opts.force,
			                 opts.set_count,
			                 opts.set_timeout,
			                 opts.meta_timeout);
			break;
		
		case CMD_TYPE_DELETE:
			retval = cmd_delete(mqtt_client,
			                    opts.topic,
			                    opts.register_topic,
			                    opts.strict,
			                    opts.force,
			                    opts.set_timeout,
			                    opts.meta_timeout);
			break;
		
		case CMD_TYPE_WATCH:
			retval = cmd_watch(mqtt_client,
			                   opts.topic,
			                   opts.json_format,
			                   opts.register_topic,
			                   opts.strict,
			                   opts.force,
			                   opts.watch_count,
			                   opts.watch_timeout,
			                   opts.meta_timeout);
			break;
		
		case CMD_TYPE_SEND:
			retval = cmd_send(mqtt_client,
			                  opts.topic,
			                  opts.value,
			                  opts.register_topic,
			                  opts.strict,
			                  opts.force,
			                  opts.send_count,
			                  opts.send_timeout,
			                  opts.meta_timeout);
			break;
		
		default:
			fprintf(stderr, "Error: Not implemented!\n");
			return 1;
	}
	
	// Unregister from Qth
	bool cleanlyDisconnect = true;
	if (opts.register_topic) {
		char *err = qth_set_property(mqtt_client, registration_url, "", opts.meta_timeout);
		if (err) {
			fprintf(stderr, "Error: Couldn't unregister: %s\n", err);
			free(err);
			cleanlyDisconnect = false;
		}
	}
	
	// Close the connection
	if (cleanlyDisconnect) {
		int status = MQTTClient_disconnect(mqtt_client, opts.meta_timeout);
		if (status != MQTTCLIENT_SUCCESS) {
			fprintf(stderr, "Error: Couldn't cleanly disconnect from MQTT broker.\n");
		}
	}
	MQTTClient_destroy(&mqtt_client);
	
	free(mqtt_url);
	free(random_client_id);
	free(registration_url);
	free(registration_msg);
	
	return retval;
}
