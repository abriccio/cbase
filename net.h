#ifndef NET_H
#define NET_H

typedef enum {
    NetFail,
    NetSuccess,
} NetResult;

typedef enum {
    HttpConnectionSync,
    HttpConnectionAsync
} HttpConnectionType;

typedef enum {
    GET,
    PUT,
    POST,
} HttpMethod;

typedef struct HttpClient HttpClient;
typedef struct HttpRequest HttpRequest;

HttpClient net_client_init(const char *client_name, const char *url, HttpConnectionType conn_type);

void net_client_deinit(HttpClient *client);

NetResult net_client_connect(HttpClient *client);

HttpRequest net_client_open_request(HttpClient *client, HttpMethod method, const char *object);

void net_request_close(HttpRequest *req);

NetResult net_request_send(HttpRequest *req);

#endif
