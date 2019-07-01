#include "csro_common.h"

// static void udp_task(void *args)
// {
//     static char udp_rx_buffer[100];
//     while (true)
//     {
//         bool sock_status = false;
//         while (sock_status == false)
//         {
//             vTaskDelay(1000 / portTICK_RATE_MS);
//             sock_status = create_udp_server();
//         }
//         while (true)
//         {
//             struct sockaddr_in source_addr;
//             socklen_t socklen = sizeof(source_addr);
//             bzero(udp_rx_buffer, 100);
//             int len = recvfrom(udp_sock, udp_rx_buffer, sizeof(udp_rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
//             if (len < 0)
//             {
//                 break;
//             }
//             printf("Udp message %s.\r\n", udp_rx_buffer);
//             cJSON *json = cJSON_Parse(udp_rx_buffer);
//             cJSON *server;
//             if (json != NULL)
//             {
//                 server = cJSON_GetObjectItem(json, "server");
//                 if ((server != NULL) && (server->type == cJSON_String) && strlen(server->valuestring) >= 7)
//                 {
//                     if (strcmp((char *)server->valuestring, (char *)mqttinfo.broker) != 0)
//                     {
//                         strcpy((char *)mqttinfo.broker, (char *)server->valuestring);
//                         mqtt_client_start();
//                     }
//                 }
//             }
//             cJSON_Delete(json);
//             //vTaskDelay(10000 / portTICK_RATE_MS);
//         }
//     }
//     vTaskDelete(NULL);
// }