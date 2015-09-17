#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"
#include "gpio.h"
#include "mem.h"
#include "driver/uart.h"
#include "user_config.h"
#include "httpclient.h"
#include "jsmn.h"

#if DEVICE_ID < 1
    #error "No device ID"
#endif

// Raw HTML files
static unsigned char header_html[] = {
    #include "header.html.h"
};
static unsigned char base_css[] = {
    #include "base.css.h"
};
static unsigned char footer_html[] = {
    #include "footer.html.h"
};
static unsigned char index_html[] = {
    #include "index.html.h"
};
static unsigned char connect_js[] = {
    #include "connect.js.h"
};
static unsigned char register_html[] = {
    #include "register.html.h"
};
static unsigned char register_js[] = {
    #include "register.js.h"
};

#define API_BASE "http://api.withmaia.io"

#define user_procTaskQueueLen    1
#define user_procTaskPrio        0
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

static volatile os_timer_t check_timer;

#define CONNECTION_UNCONFIGURED 0
#define CONNECTION_CONNECTING 1
#define CONNECTION_CONNECTED 2
#define CONNECTION_FAILED 3
int connection_status = CONNECTION_UNCONFIGURED;

#define REGISTER_UNREGISTERED 0
#define REGISTER_REGISTERING 1
#define REGISTER_REGISTERED 2
#define REGISTER_FAILED 3
int registration_status = REGISTER_UNREGISTERED;
//int registration_status = REGISTER_REGISTERED;

#define MODE_IDLE 0
#define MODE_AP 2
#define MODE_STATION 2
int mode = MODE_IDLE;

// Helpers
// -------------------------------------------------------------------------

void print(char *s) {
    ets_uart_printf(s);
    ets_uart_printf("\r\n");
}

// -------------------------------------------------------------------------

// Access point network setup

static char macaddr[6];

void ICACHE_FLASH_ATTR setup_stationmode() {
    // Set STATION mode
    wifi_set_opmode(STATION_MODE);
    mode = MODE_STATION;
}

int n_scanned = 0;
int scanned_dbs[32];
char scanned_stations[32][32];

void ICACHE_FLASH_ATTR wifi_scan_done(void *arg, STATUS status)
{
    struct bss_info* ap = (struct bss_info*)arg;
    if (status != OK || !ap)
    {
        //ets_uart_printf("ERR Scan failed: %d\r\n", status);
        return;
    }
    int si = 0;
    while ((ap = ap->next.stqe_next)) {
        //ets_uart_printf("AP CH%d %02x:%02x:%02x:%02x:%02x:%02x \"%s\" %ddBm%s\r\n",
                //ap->channel, ap->bssid[0], ap->bssid[1], ap->bssid[2],
                //ap->bssid[3], ap->bssid[4], ap->bssid[5], ap->ssid,
                //ap->rssi, ap->is_hidden ? " [HIDDEN]" : "");
        scanned_dbs[si] = ap->rssi;
        os_sprintf(scanned_stations[si], "%s", ap->ssid);
        si++;
    }
    n_scanned = si;
    //ets_uart_printf("My name is 0x%x", DEVICE_ID);
}

// Access point IP setup
void ICACHE_FLASH_ATTR setup_ap_ip() {
    struct ip_info ipinfo;
    wifi_softap_dhcps_stop();
    //wifi_get_ip_info(SOFTAP_IF, &ipinfo);
    IP4_ADDR(&ipinfo.ip, 10, 10, 10, 1); // Self IP
    IP4_ADDR(&ipinfo.gw, 10, 10, 10, 1); // Gateway IP (also self)
    IP4_ADDR(&ipinfo.netmask, 255, 255, 255, 0); // Netmask (class C /24)
    //IP4_ADDR(&dhcp_lease.start_ip, 10, 10, 10, 10);
    //IP4_ADDR(&dhcp_lease.end_ip, 10, 10, 10, 100);
    //wifi_softap_set_dhcps_lease(&dhcp_lease);
    wifi_set_ip_info(SOFTAP_IF, &ipinfo);
    wifi_softap_dhcps_start();
    //print("Set IP info");
}

void ICACHE_FLASH_ATTR setup_ap() {
    setup_ap_ip();

    // Set STATION+AP mode
    wifi_set_opmode(STATIONAP_MODE);

    // Store MAC address
    wifi_get_macaddr(SOFTAP_IF, macaddr);
    char macstr[255];
    os_sprintf(macstr, MACSTR, MAC2STR(macaddr));
    //ets_uart_printf("Got mac addr %s\r\n", macstr);

    // Set AP info
    char ssid[32];
    os_sprintf(ssid, "Maia Setup 0x%x", DEVICE_ID);
    char password[64] = "heyamaia";

    // Create config struct
    struct softap_config apConfig;
    wifi_softap_get_config(&apConfig);

    // Set SSID in struct
    os_memset(apConfig.ssid, 0, sizeof(apConfig.ssid));
    os_memcpy(apConfig.ssid, ssid, os_strlen(ssid));

    // Set Password in struct
    os_memset(apConfig.password, 0, sizeof(apConfig.password));
    os_memcpy(apConfig.password, password, os_strlen(password));

    // Set AP options
    apConfig.authmode = AUTH_WPA_WPA2_PSK;
    apConfig.channel = 7;
    apConfig.ssid_hidden = 0;
    apConfig.ssid_len = 0;
    apConfig.max_connection = 255;

    // Use config struct
    wifi_softap_set_config(&apConfig);
    //print("Set AP info");

    /* char info[1024]; */
    /* os_sprintf(info,"OPMODE: %u, SSID: %s, PASSWORD: %s, CHANNEL: %d, AUTHMODE: %d, MACADDRESS: %s\r\n", */
    /*         wifi_get_opmode(), */
    /*         apConfig.ssid, */
    /*         apConfig.password, */
    /*         apConfig.channel, */
    /*         apConfig.authmode, */
    /*         macstr); */
    //ets_uart_printf(info);

    wifi_station_scan(NULL, wifi_scan_done);
    mode = MODE_AP;
}

void ICACHE_FLASH_ATTR setup_station(char ssid[], char password[]) {
    // Stop previous connection
    wifi_station_disconnect();
    wifi_station_dhcpc_stop();

    // Create config struct
    struct station_config staConfig;
    wifi_station_get_config(&staConfig);

    // Set SSID in struct
    os_memset(staConfig.ssid, 0, sizeof(staConfig.ssid));
    os_memcpy(staConfig.ssid, ssid, os_strlen(ssid));

    // Set Password in struct
    os_memset(staConfig.password, 0, sizeof(staConfig.password));
    os_memcpy(staConfig.password, password, os_strlen(password));

    // Use config struct
    wifi_station_set_config(&staConfig);
    //print("Set Station info");
    wifi_station_connect();
    wifi_station_dhcpc_start();
    wifi_station_set_auto_connect(1);

    connection_status = CONNECTION_CONNECTING;
}

// UART
// =========================================================================

#define recvTaskPrio        1
#define recvTaskQueueLen    64
#define MAX_TXBUFFER 1024
#define MAX_CONN 5
#define UART0 0

os_event_t recvTaskQueue[recvTaskQueueLen];

#define MAX_UARTBUFFER (MAX_TXBUFFER/4)
static uint8 uartbuffer[MAX_UARTBUFFER];

char last_unknown[32] = "";
char last_temp[8] = "";
char last_hum[8] = "";

void post_measurement(char *kind, char *unit, char *value);

static void ICACHE_FLASH_ATTR recvTask(os_event_t *events) {
    RcvMsgBuff *rxBuff = (RcvMsgBuff *)events->par;
    char info[256];
    char recvd[200];
    int ri = 0;
    for (; rxBuff->pReadPos < rxBuff->pWritePos; rxBuff->pReadPos++) {
        recvd[ri++] = (char)*(rxBuff->pReadPos);
    }
    recvd[ri] = 0;
    if (ri < 1) return; // Empty line
    //print(recvd);

    if (registration_status != REGISTER_REGISTERED) {
        os_strcpy(last_unknown, recvd);
        return;
    }

    if (strncmp("TEMP", recvd, 4) == 0) {
        if (os_strcmp(last_temp, recvd+5) != 0) { // Not same as last time
            os_strcpy(last_temp, recvd+5);
            post_measurement("temperature", "F", last_temp);
        }
    }

    else if (strncmp("HUM", recvd, 3) == 0) {
        if (os_strcmp(last_hum, recvd+4) != 0) { // Not same as last time
            os_strcpy(last_hum, recvd+4);
            post_measurement("humidity", "%", last_hum);
        }
    }

    // Reset RX buffer positions
    rxBuff->pWritePos = rxBuff->pRcvMsgBuff;
    rxBuff->pReadPos = rxBuff->pRcvMsgBuff;
}

// HTTP Client
// =========================================================================

// Sending HTTP requests
void http_post_callback(char * response, int http_status, char * full_response) {
    // TODO: Parse response json
    if (registration_status == REGISTER_REGISTERING)
        registration_status = REGISTER_REGISTERED;
}

void ICACHE_FLASH_ATTR post_json(char *path, char *content) {
    http_post(path, content, "Content-Type: application/json\r\n", http_post_callback);
}

void measurement_json(char *json_str, char *kind, char *unit, char *value) {
    os_sprintf(json_str, "{"
        "\"device_id\": \"0x%x\","
        "\"kind\": \"%s\","
        "\"unit\": \"%s\","
        "\"value\": \"%s\""
    "}", DEVICE_ID, kind, unit, value);
}

void post_measurement(char *kind, char *unit, char *value) {
    char json_str[128] = "";
    measurement_json(json_str, kind, unit, value);
    post_json(API_BASE "/measurements.json", json_str);
}

// HTTP Server
// =========================================================================

uint16_t server_timeover = 10; // seconds?
struct espconn server; // TODO: Name this better (server_conn?)
const char *msg_welcome = "testing this welcome hi\r\n";

void token_string(char *into, char *json, jsmntok_t t) {
    char tv[100];
    os_strncpy(tv, json+t.start, t.end-t.start);
    tv[t.end-t.start] = 0;
    //print(tv);
    strcpy(into, tv);
}

// HTTP response helpers

void ICACHE_FLASH_ATTR send_resp(struct espconn *conn, char *response) {
    espconn_sent(conn, response, os_strlen(response));
}

void ICACHE_FLASH_ATTR send_404(struct espconn *conn) {
    send_resp(conn, "HTTP/1.1 404 Not found\r\n\r\n");
    espconn_disconnect(conn);
}

void ICACHE_FLASH_ATTR send_ok(struct espconn *conn, char *response) {
    send_resp(conn, "HTTP/1.1 200 OK\r\n\r\n");
    send_resp(conn, response);
    send_resp(conn, "\r\n");
    espconn_disconnect(conn);
}

void ICACHE_FLASH_ATTR send_json(struct espconn *conn, char *response) {
    send_resp(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
    send_resp(conn, response);
    send_resp(conn, "\r\n");
    espconn_disconnect(conn);
}

void ICACHE_FLASH_ATTR send_ok_templated(struct espconn *conn, char *response) {
    send_resp(conn, "HTTP/1.1 200 OK\r\n\r\n");
    send_resp(conn, header_html);
    send_resp(conn, response);
    send_resp(conn, footer_html);
    espconn_disconnect(conn);
}

void parse_http(char *http_raw, unsigned short length, char *method, char *path, char *headers, char *body) {

    // Parse HTTP request
    // TODO: Decipherable variable names
    char line[1024];
    char lc = 0;
    int li = 0;
    int i = 0;
    int m = 0; // 0=intro 1=headers 2=body

    for (; i<length; i++) {
        char c = http_raw[i];
        if (m < 2 && c == '\n' && lc == '\r') { // Got intro or header line
            line[li-1] = 0;

            if (li == 1) m++; // Blank line: end of headers / start of body

            else if (m == 0) {
                m++;
                // TODO: Parse method, path
                char ll[500];
                int im = 0;
                int ii = 0;
                int cii = 0;
                for (; ii<li; ii++) {
                    if (line[ii] == ' ') {
                        ll[cii] = 0;
                        if (im == 0) { // Method
                            os_strcpy(method, ll);
                            im++;
                        } else if (im == 1) { // Path
                            os_strcpy(path, ll);
                            break;
                        }
                        cii = 0;
                    } else {
                        ll[cii++] = line[ii];
                    }
                }

            } else if (m == 1) {
                // TODO: Parse header
            }

            li = 0;
            lc = 0;
        }
        else {
            lc = c;
            line[li++] = c;
        }
    }

    // Set body if it exists
    line[li] = 0;
    if (li > 0) {
        os_strcpy(body, line);
    }
}

// HTTP request handling 

void ICACHE_FLASH_ATTR server_recv_cb(void *arg, char *http_raw, unsigned short length) {
    struct espconn *pespconn = (struct espconn *)arg;
    //print("[server_recv_cb] Received data:");

    char method[10];
    char path[60];
    char headers[60];
    char body[256];
    parse_http(http_raw, length, method, path, headers, body);

    int GET = (os_strcmp(method, "GET") == 0);
    int POST = (os_strcmp(method, "POST") == 0);

    if (GET) { // No body if not [post/put/patch]ing

        // Static files

        if (os_strcmp(path, "/base.css") == 0) { send_ok(pespconn, base_css); }
        else if (os_strcmp(path, "/connect.js") == 0) { send_ok(pespconn, connect_js); }
        else if (os_strcmp(path, "/register.js") == 0) { send_ok(pespconn, register_js); }

        // JSON responses

        else if (os_strcmp(path, "/connection.json") == 0) {
            int station_connect_status = wifi_station_get_connect_status();

            if (station_connect_status == STATION_GOT_IP) {
                struct ip_info ipConfig;
                wifi_get_ip_info(STATION_IF, &ipConfig);
                char json_str[54];
                os_sprintf(json_str, "{\"status\": \"connected\", \"ip\": \"%d.%d.%d.%d\"}", IP2STR(&ipConfig.ip));
                send_json(pespconn, json_str);
            }

            else {
                char *status_str;

                if (connection_status == CONNECTION_UNCONFIGURED) status_str = "unconfigured";
                else
                switch (station_connect_status) {
                    case STATION_CONNECTING: status_str = "connecting"; break;
                    case STATION_WRONG_PASSWORD: status_str = "failed"; break;
                    case STATION_NO_AP_FOUND: status_str = "failed"; break;
                    case STATION_CONNECT_FAIL: status_str = "failed"; break;
                }

                char json_str[54];
                os_sprintf(json_str, "{\"status\": \"%s\"}", status_str);
                send_json(pespconn, json_str);
            }

        }

        else if (os_strcmp(path, "/registration.json") == 0) {
            char *status_str;

            switch (registration_status) {
                case REGISTER_UNREGISTERED: status_str = "unregistered"; break;
                case REGISTER_REGISTERING: status_str = "registering"; break;
                case REGISTER_REGISTERED: status_str = "registered"; break;
                case REGISTER_FAILED: status_str = "failed"; break;
            }

            char json_str[54];
            os_sprintf(json_str, "{\"status\": \"%s\"}", status_str);
            send_json(pespconn, json_str);
        }

        // HTML pages

        else if (os_strcmp(path, "/read") == 0) {
            if (registration_status == REGISTER_REGISTERED) {
                char temp_json_str[128];
                char hum_json_str[128];
                measurement_json(temp_json_str, "temperature", "F", last_temp);
                measurement_json(hum_json_str, "humidity", "%", last_hum);
                char full_json_str[256] = "";
                strcat(full_json_str, temp_json_str);
                strcat(full_json_str, hum_json_str);
                full_json_str[os_strlen(temp_json_str)+os_strlen(hum_json_str)] = 0;
                send_ok_templated(pespconn, full_json_str);
            } else {
                send_ok_templated(pespconn, last_unknown);
            }

        }

        else if (os_strcmp(path, "/register") == 0) {
            send_ok_templated(pespconn, register_html);
        }

        else if (os_strcmp(path, "/scan.json") == 0) {
            char json_str[256] = "[";
            
            int si = 0;
            for (; si < n_scanned; si++) {
                char json_obj[100];
                os_sprintf(json_obj, "{\"ssid\": \"%s\", \"rssi\": %d}", scanned_stations[si], scanned_dbs[si]);
                os_strcat(json_str, json_obj);
                if (si < n_scanned - 1) {
                    os_strcat(json_str, ",");
                } else {
                    os_strcat(json_str, "]");
                }
            }

            send_json(pespconn, json_str);
        }

        else if (os_strcmp(path, "/") == 0) {
            send_ok_templated(pespconn, index_html);
        }

        else {
            send_404(pespconn);
        }

        return;
    }

    else if (POST) {

        // Parse JSON with jsmn
        jsmn_parser parser;
        jsmn_init(&parser);
        jsmntok_t tokens[32];
        jsmnerr_t r;
        r = jsmn_parse(&parser, body, 1024, tokens, 256);
        if (r < 0) {
            //print("JSON Parse error?");
            return;
        }

        // Look for ssid and pass
        char station_ssid[20];
        char station_pass[20];

        //print("JSON Parse success?");

        if (os_strcmp(path, "/connect.json") == 0) {
            // Parse ssid and pass from JSON
            int ti = 0;
            int has_ssid = 0;
            int has_pass = 0;
            int on_ssid = 0;
            int on_pass = 0;
            for(; tokens[ti].end; ti++) {
                char tv[256];
                token_string(tv, body, tokens[ti]);
                if (on_ssid) {
                    //print("Found ssid");
                    on_ssid = 0;
                    os_strcpy(station_ssid, tv);
                    has_ssid = 1;
                }
                if (on_pass) {
                    //print("Found pass");
                    on_pass = 0;
                    os_strcpy(station_pass, tv);
                    has_pass = 1;
                    if (has_ssid) { break; }
                }
                on_ssid = ti % 2 == 1 && os_strcmp(tv, "ssid") == 0;
                on_pass = ti % 2 == 1 && os_strcmp(tv, "pass") == 0;
            }

            //ets_uart_printf("Hopefully ssid=%s and pass=%s\r\n", station_ssid, station_pass);
            send_ok(pespconn, "<h1>maia</h1><p>OK</p>");
            setup_station(station_ssid, station_pass);
        }

        else if (os_strcmp(path, "/register.json") == 0) {
            // Parse email and password from JSON
            int ti = 0;
            char user_email[64];
            char user_password[64];
            int has_email = 0;
            int has_password = 0;
            int on_email = 0;
            int on_password = 0;
            for(; tokens[ti].end; ti++) {
                char tv[256];
                token_string(tv, body, tokens[ti]);
                if (on_email) {
                    //print("Found email");
                    on_email = 0;
                    os_strcpy(user_email, tv);
                    has_email = 1;
                }
                if (on_password) {
                    //print("Found password");
                    on_password = 0;
                    os_strcpy(user_password, tv);
                    has_password = 1;
                    if (has_email) { break; }
                }
                on_email = ti % 2 == 1 && os_strcmp(tv, "email") == 0;
                on_password = ti % 2 == 1 && os_strcmp(tv, "password") == 0;
            }

            char register_response[256];
            os_sprintf(register_response, "Registering as %d...", DEVICE_ID);
            send_ok_templated(pespconn, register_response);
            char register_json[256];
            os_sprintf(register_json, "{"
                "\"device_id\": \"0x%x\","
                "\"kind\": \"%s\","
                "\"email\": \"%s\","
                "\"password\": \"%s\""
            "}", DEVICE_ID, DEVICE_KIND, user_email, user_password);
            registration_status = REGISTER_REGISTERING;
            post_json(API_BASE "/devices.json", register_json);
        }

        else {
            send_404(pespconn);
        }

        return;
    }

    send_404(pespconn);
    return;
}

// Server socket setup
// -------------------------------------------------------------------------

void ICACHE_FLASH_ATTR server_discon_cb(void *arg) {

}

// TCP Connected callback
void ICACHE_FLASH_ATTR tcpserver_connectcb(void *arg) {
    struct espconn *pespconn = (struct espconn *)arg;
    espconn_regist_recvcb(pespconn, server_recv_cb);
    espconn_regist_disconcb(pespconn, server_discon_cb);
    //ets_uart_printf("Server ready\r\n");
}

void ICACHE_FLASH_ATTR setup_server() {
    server.type = ESPCONN_TCP;
    server.state = ESPCONN_NONE;
    server.proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    server.proto.tcp->local_port = 80;
    espconn_regist_connectcb(&server, tcpserver_connectcb);
    espconn_accept(&server);
    espconn_regist_time(&server, server_timeover, 0);
}

void ICACHE_FLASH_ATTR stop_server() {
    espconn_disconnect(&server);
}

// Main init
// =========================================================================

// Status check function
void check_timerfunc(void *arg) {
    struct ip_info ipConfig;
    wifi_get_ip_info(STATION_IF, &ipConfig);
    if (ipConfig.ip.addr != 0) {
        connection_status = CONNECTION_CONNECTED;
        //ets_uart_printf("[check_timerfunc] Connected? ");
        ets_uart_printf("C %d.%d.%d.%d\r\n", IP2STR(&ipConfig.ip));
        //ets_uart_printf("C");

        if (mode == MODE_AP) {
            // Just Station and no AP
            setup_stationmode();
            // Turn off HTTP server
            stop_server();
        }

    } else if (connection_status == CONNECTION_CONNECTING) {
        ets_uart_printf("X");
        //print("[check_timerfunc] No IP");
    } else {
        ets_uart_printf("U");
        if (mode != MODE_AP) {
            // Setup AP and Server
            setup_ap();
            setup_server();
        }
    }
}

//Do nothing function
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events) {
    os_delay_us(10);
}

void ICACHE_FLASH_ATTR init_done() {
    setup_server();
}

#define DEBUG 1 

void ICACHE_FLASH_ATTR user_init() {
    // Initialize
    gpio_init();
    uart_init(2400, 2400);
    os_delay_us(10000);
    //print("\r\n:)\r\n");

    if (0 && DEBUG) {
        wifi_station_disconnect();
        wifi_station_dhcpc_stop();
    }

    //setup_server();

    // -------------------------------------------------------------------------

    ets_uart_printf("S");

    //Set GPIO2 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    //Set GPIO2 low
    gpio_output_set(0, BIT2, BIT2, 0);

    //Disarm timer
    os_timer_disarm(&check_timer);

    //Setup timer
    os_timer_setfn(&check_timer, (os_timer_func_t *)check_timerfunc, NULL);

    //Arm the timer
    //1000 is the fire time in ms
    //0 for once and 1 for repeating
    os_timer_arm(&check_timer, 5000, 1); // Start going
    
    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    system_os_task(recvTask, recvTaskPrio, recvTaskQueue, recvTaskQueueLen);

    system_init_done_cb(init_done);

}
