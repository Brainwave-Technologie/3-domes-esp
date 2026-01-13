#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"

static const char *TAG = "TX";

/* Broadcast peer */
const uint8_t BC_MAC[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

/* RX print */
void rx_cb(const esp_now_recv_info_t *info,const uint8_t *data,int len)
{
    printf("RX: ");
    for(int i=0;i<len;i++) putchar(data[i]);
    printf("\n");
}

/* Send ASCII framed command */
void send_ascii(const char *cmd)
{
    esp_now_send(BC_MAC, (uint8_t*)cmd, strlen(cmd));
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());

    uint8_t pmk[16]="pmk1234567890123";
    ESP_ERROR_CHECK(esp_now_set_pmk(pmk));

    ESP_ERROR_CHECK(esp_now_register_recv_cb(rx_cb));

    esp_now_peer_info_t peer={0};
    memcpy(peer.peer_addr,BC_MAC,6);
    peer.encrypt=false;
    peer.channel=0;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    vTaskDelay(pdMS_TO_TICKS(800));

    ESP_LOGI(TAG,"TX Ready");

    while(1)
    {
        send_ascii("@I02#TL");
        send_ascii("@I02#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@I04#TL");
        send_ascii("@I04#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@I06#TL");
        send_ascii("@I06#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@I08#TL");
        send_ascii("@I08#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@I0:#TL");
        send_ascii("@I0:#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@C05#TL");
        send_ascii("@C05#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@C+5#TL");
        send_ascii("@C+5#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@C-5#TL");
        send_ascii("@C-5#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@D_1#TL");
        send_ascii("@D_1#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        send_ascii("@D_0#TL");
        send_ascii("@D_0#TR");
        vTaskDelay(pdMS_TO_TICKS(2000));

        // send_ascii("@CFG#");        // Enter config mode
        // vTaskDelay(pdMS_TO_TICKS(5000));

        // send_ascii("@CFG_SAVE#");   // Save config
        // vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
