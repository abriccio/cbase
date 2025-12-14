#include "net.h"
#include <Windows.h>
#include <WinInet.h>
#include <stdio.h>

static DWORD print_win32_error_and_fail() {
    DWORD dw = GetLastError();
    printf("Error: %lu\n", dw);
    return dw;
}

struct HttpClient {
    const char *client_name;
    const char *url;
    HttpConnectionType connection_type;
    HINTERNET h_inet;
    HINTERNET h_http;
};

struct HttpRequest {
    HttpClient *client;
    HINTERNET h_req;
};

// Initialize a new client. Does not own strings for client name/URL
HttpClient net_client_init(const char *client_name, const char *url, HttpConnectionType conn_type) {
    return (HttpClient){
        .client_name = client_name,
        .url = url,
        .connection_type = conn_type,
    };
}

void net_client_deinit(HttpClient *client) {
    if (client->h_http)
        InternetCloseHandle(client->h_http);
    if (client->h_inet)
        InternetCloseHandle(client->h_inet);
}

NetResult net_client_connect(HttpClient *client) {
    DWORD flags = client->connection_type == HttpConnectionAsync ? INTERNET_FLAG_ASYNC : 0;
    client->h_inet = InternetOpenA(client->client_name, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, flags);
    client->h_http = InternetConnectA(client->h_inet, client->url, INTERNET_DEFAULT_HTTP_PORT,
                                      NULL, NULL, INTERNET_SERVICE_HTTP, INTERNET_FLAG_SECURE, (DWORD_PTR)client);
    if (!client->h_inet || !client->h_http)
        return NetFail;

    return NetSuccess;
}

HttpRequest net_client_open_request(HttpClient *client, HttpMethod method, const char *object) {
    const char *verb = NULL;
    switch (method) {
    case GET: verb = "GET"; break;
    case POST: verb = "POST"; break;
    case PUT: verb = "PUT"; break;
    }
    HttpRequest req;
    req.client = client;
    req.h_req = HttpOpenRequestA(client->h_http, verb, object, NULL, NULL, NULL, 0, (DWORD_PTR)client);

    return req;
}

void net_request_close(HttpRequest *req) {
    if (req->h_req)
        InternetCloseHandle(req->h_req);
}

NetResult net_request_send(HttpRequest *req) {
    const char *headers = "Content-Type: text/*";
    return HttpSendRequestA(req->h_req, headers, strlen(headers), NULL, 0) == TRUE;
}

#if NET_TEST
int main(int argc, char *argv[]) {
    HttpClient client = net_client_init("NET_CLIENT_TEST", "arborealaudio.com", Sync);
    if (net_client_connect(&client) != NetSuccess) {
        printf("Connection failed\n");
        return print_win32_error_and_fail();
    }
    printf("Connection succeeded\n");

    HttpRequest req = net_client_open_request(&client, GET, "/versions/index.json");
    if (!req.h_req) {
        printf("Request open failed\n");
        return print_win32_error_and_fail();
    }
    if (net_request_send(&req) != NetSuccess) {
        printf("Request send failed\n");
        return print_win32_error_and_fail();
    }

    printf("request succeeded\n");

    char buffer[4096];
    DWORD bytes_read;
    if (InternetReadFile(req.h_req, buffer, sizeof(buffer) - 1, &bytes_read) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Response:\n%s\n", buffer);
    }

    net_request_close(&req);
    net_client_deinit(&client);

    return 0;
}
#endif
