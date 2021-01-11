#ifndef QTH_CLIENT_H
#define QTH_CLIENT_H

#include <stdbool.h>

#include "json.h"
#include "MQTTClient.h"

#define VERSION_STRING "v0.3.1"

// QoS value to use for all commands
#define QTH_QOS 2

// The type of command entered on the command line.
typedef enum {
	CMD_TYPE_AUTO = 0,
	CMD_TYPE_GET,
	CMD_TYPE_SET,
	CMD_TYPE_DELETE,
	CMD_TYPE_WATCH,
	CMD_TYPE_SEND,
	CMD_TYPE_LS,
} cmd_type_t;

// The type formatting to use when displaying JSON
typedef enum {
	JSON_FORMAT_SINGLE_LINE = 0,
	JSON_FORMAT_PRETTY,
	JSON_FORMAT_VERBATIM,
	JSON_FORMAT_QUIET,
} json_format_t;

// The list formatting to for directory listings
typedef enum {
	LS_FORMAT_SHORT = 0,
	LS_FORMAT_LONG,
	LS_FORMAT_JSON,
} ls_format_t;

// Where the value to be sent or set should be fetched from
typedef enum {
	VALUE_SOURCE_NONE = 0, // No value was provided
	VALUE_SOURCE_NULL,     // Just the JSON constant 'null'
	VALUE_SOURCE_ARG,      // Value in argument
	VALUE_SOURCE_STDIN,    // Read from stdin
} value_source_t;

// Struct defining the options specified on the commandline
typedef struct {
	// Which command was used?
	cmd_type_t cmd_type;
	
	// MQTT connection parameters
	char *mqtt_host;
	int mqtt_port;
	int mqtt_keep_alive;  // (seconds)
	
	// Client ID to use.
	char *client_id;
	
	// Qth meta access timeout (ms)
	int meta_timeout;
	
	// Value setting/fetching timeouts (ms)
	int get_timeout;
	int set_timeout;
	int delete_timeout;
	int watch_timeout;
	int send_timeout;
	
	// How many values to get/set before exiting
	int get_count;
	int set_count;
	int watch_count;
	int send_count;
	
	// How should JSON be displayed
	json_format_t json_format;
	
	// Should we check 1:N or N:1 state matches our action?
	bool strict;
	
	// Should topic type be ignored?
	bool force;
	
	// Should the topic type be registered with the Qth registrar
	bool register_topic;
	
	// Description to use when registering a topic.
	char *description;
	
	// Value to set a registered topic to upon unregistering it (or NULL if no
	// message to be sent or if it is to be deleted)
	char *on_unregister;
	
	// Should a property be deleted when it is unregistered
	bool delete_on_unregister;
	
	// Should ls print directories recursively
	bool ls_recursive;
	
	// ls listing format
	ls_format_t ls_format;
	
	// The topic specified
	char *topic;
	
	// Where the value should be taken from
	value_source_t value_source;
	
	// If value_source is VALUE_SOURCE_ARG, a pointer to the value used as an
	// argument, otherwise NULL.
	char *value;
} options_t;


options_t argparse(int argc, char *argv[]);

char *json_parse(const char *str, int len, json_object **obj);
char *json_validate(const char *str, int len);
char *json_to_format(const char *in_str, json_format_t json_format);

char *alloced_copy(const char *str);
char *alloced_copyn(const char *str, size_t len);
char *alloced_cat(const char *a, const char *b);

bool qth_is_directory_listing(json_object *dir);
const char **qth_subdirectory_get_behaviours(json_object *dir, const char *subpath);
bool qth_subdirectory_has_behaviour(json_object *dir, const char *subpath, const char *behaviour, bool strict);
char *qth_get_directory(MQTTClient *client, const char *path, char **dir, int meta_timeout);
char *qth_set_delete_or_send(MQTTClient *client, const char *topic, char *value,  bool is_property, int timeout);
char *qth_set_property(MQTTClient *client, const char *topic, char *value, int timeout);
char *qth_send_event(MQTTClient *client, const char *topic, char *value, int timeout);
char *get_topic_path(const char *topic);
const char *get_topic_name(const char *topic);
int verify_topic(MQTTClient *client, const char *topic,
                 const char *desired_behaviour, bool strict, int meta_timeout);
int get_topic_behaviour(MQTTClient *client, const char *topic,
                        int meta_timeout, char **behaviour);

int cmd_ls(MQTTClient *mqtt_client,
           const char *path,
           int meta_timeout,
           bool ls_recursive,
           ls_format_t ls_format,
           json_format_t json_format);

int cmd_set(MQTTClient *client,
            const char *topic,
            const char *value,
            bool is_registering,
            bool strict,
            bool force,
            int count,
            int timeout,
            int meta_timeout);

int cmd_delete(MQTTClient *client,
               const char *topic,
               bool is_registering,
               bool strict,
               bool force,
               int timeout,
               int meta_timeout);

int cmd_send(MQTTClient *client,
             const char *topic,
             const char *value,
             bool is_registering,
             bool strict,
             bool force,
             int count,
             int timeout,
             int meta_timeout);

int cmd_get(MQTTClient *client,
            const char *topic,
            json_format_t json_format,
            bool is_registering,
            bool strict,
            bool force,
            int count,
            int timeout,
            int meta_timeout);

int cmd_watch(MQTTClient *client,
              const char *topic,
              json_format_t json_format,
              bool is_registering,
              bool strict,
              bool force,
              int count,
              int timeout,
              int meta_timeout);

int cmd_auto(MQTTClient *client,
             bool strict,
             const char *topic,
             char **value,
             value_source_t *value_source,
             cmd_type_t *cmd_type,
             int meta_timeout);

#endif
