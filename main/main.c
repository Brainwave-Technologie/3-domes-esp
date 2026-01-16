#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "driver/gpio.h"

static const char *TAG = "REMOTE";

/* GPIO */
#define BTN1_GPIO 12
#define BTN2_GPIO 14
#define BTN3_GPIO 27
#define LED_R_GPIO 25
#define LED_G_GPIO 26
#define LED_B_GPIO 13

/* ESPNOW */
const uint8_t BC_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* MODES */
typedef enum
{
    MODE_INTENSITY,
    MODE_COLOR,
    MODE_DEPTH
} mode_t;

mode_t active_mode = MODE_INTENSITY;

/* POWER */
uint8_t device_on = 0;

/* SLIDERS */
int i_idx = 1, c_idx = 1, d_idx = 0;

const char *I_CMD[] = {"@I01#TL", "@I02#TL", "@I04#TL", "@I06#TL", "@I08#TL", "@I0:#TL"};
const char *C_CMD[] = {"@C-5#TL", "@C05#TL", "@C+5#TL"};
const char *D_CMD[] = {"@D_0#TL", "@D_1#TL"};

uint32_t last_activity_ms = 0;
uint32_t btn1_press_ms = 0;
uint8_t btn1_prev = 0;

/* ------------------------------------------------------- */

/* ESP-NOW SEND CALLBACK */
void send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    ESP_LOGI("ESP-NOW", "Send status: %s",
             status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}


/* ------------------------------------------------------- */

uint8_t btn(uint8_t pin)
{
    return gpio_get_level(pin) == 0;
}

/* MULTI-CHANNEL SEND (TL/TR/TM) */
void send_ascii_multi(const char *cmd)
{
    if (!device_on)
        return;

    char buf[16];

    /* TL */
    esp_now_send(BC_MAC, (uint8_t *)cmd, strlen(cmd));
    ESP_LOGI("TX", "%s", cmd);
    vTaskDelay(pdMS_TO_TICKS(15));

    /* TR */
    strcpy(buf, cmd);
    buf[strlen(buf) - 1] = 'R';
    esp_now_send(BC_MAC, (uint8_t *)buf, strlen(buf));
    ESP_LOGI("TX", "%s", buf);
    vTaskDelay(pdMS_TO_TICKS(15));

    /* TM */
    strcpy(buf, cmd);
    buf[strlen(buf) - 1] = 'M';
    esp_now_send(BC_MAC, (uint8_t *)buf, strlen(buf));
    ESP_LOGI("TX", "%s", buf);
    vTaskDelay(pdMS_TO_TICKS(15));

    last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* SEND COMPLETE STATE */
void send_full_state(void)
{
    ESP_LOGW("STATE", "I=%d C=%d D=%d", i_idx, c_idx, d_idx);

    send_ascii_multi(I_CMD[i_idx]);
    vTaskDelay(pdMS_TO_TICKS(30));

    send_ascii_multi(C_CMD[c_idx]);
    vTaskDelay(pdMS_TO_TICKS(30));

    send_ascii_multi(D_CMD[d_idx]);
}

/* STARTUP PACKETS WITH SAFE TIMING */
void send_startup_packets(void)
{
    ESP_LOGW("STARTUP", "Sending default startup packets");

    const char *packets[] = {
        "@I01#TL", "@D_0#TL", "@L_1#TL", "@F_0#TL", "@E_0#TL", "@C05#TL",
        "@I01#TR", "@D_0#TR", "@L_1#TR", "@F_0#TR", "@E_0#TR", "@C05#TR",
        "@I01#TM", "@D_0#TM", "@L_1#TM", "@F_0#TM", "@E_0#TM", "@C05#TM"};

    for (int i = 0; i < 18; i++)
    {
        esp_now_send(BC_MAC, (uint8_t *)packets[i], strlen(packets[i]));
        ESP_LOGI("STARTUP-TX", "%s", packets[i]);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

/* ------------------------------------------------------- */

void handle_btn1(void)
{
    uint8_t now = btn(BTN1_GPIO);
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (now && !btn1_prev)
    {
        btn1_press_ms = now_ms;
        ESP_LOGI("BTN1", "PRESS");
    }

    if (!now && btn1_prev)
    {
        uint32_t dur = now_ms - btn1_press_ms;
        ESP_LOGI("BTN1", "RELEASE %lu ms", dur);

        if (!device_on && dur >= 5000)
        {
            device_on = 1;
            last_activity_ms = now_ms;
            ESP_LOGW("POWER", "ON");

            send_startup_packets();
        }
        else if (device_on && dur >= 2000)
        {
            device_on = 0;
            ESP_LOGW("POWER", "OFF");
        }
        else if (device_on && dur < 600)
        {
            active_mode++;
            if (active_mode > MODE_DEPTH)
                active_mode = MODE_INTENSITY;

            ESP_LOGI("MODE", "%d", active_mode);
        }
    }

    btn1_prev = now;
}

/* ------------------------------------------------------- */

void handle_btn2(void)
{
    if (!device_on || !btn(BTN2_GPIO))
        return;

    ESP_LOGI("BTN2", "INC");

    if (active_mode == MODE_INTENSITY && i_idx < 5)
        i_idx++;
    else if (active_mode == MODE_COLOR && c_idx < 2)
        c_idx++;
    else if (active_mode == MODE_DEPTH && d_idx == 0)
        d_idx++;

    send_full_state();
    vTaskDelay(pdMS_TO_TICKS(250));
}

void handle_btn3(void)
{
    if (!device_on || !btn(BTN3_GPIO))
        return;

    ESP_LOGI("BTN3", "DEC");

    if (active_mode == MODE_INTENSITY && i_idx > 0)
        i_idx--;
    else if (active_mode == MODE_COLOR && c_idx > 0)
        c_idx--;
    else if (active_mode == MODE_DEPTH && d_idx == 1)
        d_idx--;

    send_full_state();
    vTaskDelay(pdMS_TO_TICKS(250));
}

/* ------------------------------------------------------- */

void update_led(void)
{
    gpio_set_level(LED_R_GPIO, device_on && active_mode == MODE_INTENSITY);
    gpio_set_level(LED_G_GPIO, device_on && active_mode == MODE_COLOR);
    gpio_set_level(LED_B_GPIO, device_on && active_mode == MODE_DEPTH);
}

/* ------------------------------------------------------- */

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* FORCE FIXED CHANNEL FOR RELIABILITY */
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());

    esp_now_register_send_cb(send_cb);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, BC_MAC, 6);
    peer.channel = 1;
    peer.ifidx = ESP_IF_WIFI_STA;
    peer.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    gpio_config_t in = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN1_GPIO) | (1ULL << BTN2_GPIO) | (1ULL << BTN3_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE};

    gpio_config(&in);

    gpio_config_t out = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_R_GPIO) | (1ULL << LED_G_GPIO) | (1ULL << LED_B_GPIO)};

    gpio_config(&out);

    ESP_LOGW(TAG, "REMOTE READY");

    while (1)
    {
        handle_btn1();
        handle_btn2();
        handle_btn3();
        update_led();

        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (device_on && (now_ms - last_activity_ms > 60000))
        {
            device_on = 0;
            ESP_LOGW("AUTO", "POWER OFF (IDLE)");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
