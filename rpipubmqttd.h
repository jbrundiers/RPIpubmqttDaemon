


#ifndef RPIPUBMQTTDAEMON_H
#define RPIPUBMQTTDAEMON_H


struct mosq_config {
    char *id;
    char *id_prefix;
    int protocol_version;
    int keepalive;
    char *host;
    int port;
    int qos;
    bool retain;
    int pub_mode; /* pub, rr */
    char *file_input; /* pub, rr */
    char *message; /* pub, rr */
    int msglen; /* pub, rr */
    char *topic; /* pub, rr */
    char *bind_address;
    int repeat_count; /* pub */
    struct timeval repeat_delay; /* pub */
#ifdef WITH_SRV
    bool use_srv;
#endif
    bool debug;
    bool quiet;
    unsigned int max_inflight;
    char *username;
    char *password;
    char *will_topic;
    char *will_payload;
    int will_payloadlen;
    int will_qos;
    bool will_retain;
#ifdef WITH_TLS
    char *cafile;
    char *capath;
    char *certfile;
    char *keyfile;
    char *ciphers;
    bool insecure;
    char *tls_alpn;
    char *tls_version;
    char *tls_engine;
    char *tls_engine_kpass_sha1;
    char *keyform;
#  ifdef FINAL_WITH_TLS_PSK
    char *psk;
    char *psk_identity;
#  endif
#endif
   bool clean_session;
    char **topics; /* sub */
    int topic_count; /* sub */
    bool exit_after_sub; /* sub */
    bool no_retain; /* sub */
    bool retained_only; /* sub */
    bool remove_retained; /* sub */
    char **filter_outs; /* sub */
    int filter_out_count; /* sub */
    char **unsub_topics; /* sub */
    int unsub_topic_count; /* sub */
    bool verbose; /* sub */
    bool eol; /* sub */
    int msg_count; /* sub */
    char *format; /* sub */
    unsigned int timeout; /* sub */
    int sub_opts; /* sub */
    long session_expiry_interval;
#ifdef WITH_SOCKS
    char *socks5_host;
    int socks5_port;
    char *socks5_username;
    char *socks5_password;
#endif
    mosquitto_property *connect_props;
    mosquitto_property *publish_props;
    mosquitto_property *subscribe_props;
    mosquitto_property *unsubscribe_props;
    mosquitto_property *disconnect_props;
    mosquitto_property *will_props;
    bool have_topic_alias; /* pub */
    char *response_topic; /* rr */
};

#endif

