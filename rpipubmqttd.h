


#ifndef RPIPUBMQTTDAEMON_H
#define RPIPUBMQTTDAEMON_H

struct mosq_config {

    char *id;           // String to use as the client id. If NULL, a random client id will be generated.  If id is NULL, clean_session must be true.
    bool clean_session;
    char *host;
    int port;

    char *username;
    char *password;

    char *topic; /* pub, rr */

    bool retain;
    mosquitto_property *publish_props;
    mosquitto_property *connect_props;
    /* ____________________________________________________________ */


    char *id_prefix;
    int protocol_version;
    int keepalive;
    int qos;
    int pub_mode; /* pub, rr */
    char *file_input; /* pub, rr */
    char *message; /* pub, rr */
    int msglen; /* pub, rr */
    char *bind_address;
    int repeat_count; /* pub */
    struct timeval repeat_delay; /* pub */
};

#endif

