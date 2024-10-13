#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora.h"
#include "esp_wifi.h"
#include "esp_private/wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

static const char *TAG = "+";
static bool s_sta_is_connected = false;
static bool wifi_is_connected = false;

#define CONFIG_EXAMPLE_WIFI_SSID "GenesisStation"
#define CONFIG_EXAMPLE_WIFI_PASSWORD "Hackathon!@_"

#define CONFIG_EXAMPLE_MAX_STA_CONN 4

#define BUILT_IN GPIO_NUM_25

uint8_t but[301];

typedef struct
{
    uint8_t buf[301];
    uint16_t len;
} Xmsg;

SemaphoreHandle_t xSemaphore;
esp_timer_handle_t TX_timer;
QueueHandle_t xQueue1;

static void timeout_timer_callback(void *arg)
{
    Xmsg TXmsg;

    // Use xQueueReceive em vez de xQueueReceiveFromISR
    if (xQueueReceive(xQueue1, &TXmsg, 0))
    {
        if (xSemaphore != NULL)
        {
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
            {
                printf("\n->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->");
                printf("\nReading the xQueue and sending to LoRa. LORA TX TICK: %lu\n", xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
                printf("->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->\n");

                lora_send_packet(TXmsg.buf, TXmsg.len);

                xSemaphoreGive(xSemaphore);
            }
        }
    }

    // Trate possíveis erros de forma mais graciosa
    esp_err_t err = esp_timer_start_once(TX_timer, 1000000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TX_timer: %s", esp_err_to_name(err));
        // Implementar lógica adicional se necessário
    }
}

void lora_rx_mode(void)
{
    if (xSemaphore != NULL)
    {
        // See if we can obtain the semaphore. If the semaphore is not available
        // wait indefinitely to see if it becomes free.
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            // We were able to obtain the semaphore and can now access the
            // shared resource.

            lora_receive(); // Put into receive mode

            // We have finished accessing the shared resource. Release the
            // semaphore.
            xSemaphoreGive(xSemaphore);
        }
    }
}

int lora_rxed(void)
{
    int ans = 0;
    if (xSemaphore != NULL)
    {
        // See if we can obtain the semaphore. If the semaphore is not available
        // wait indefinitely to see if it becomes free.
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            // We were able to obtain the semaphore and can now access the
            // shared resource.

            ans = lora_received(); // Verify if it has received a message

            // We have finished accessing the shared resource. Release the
            // semaphore.
            xSemaphoreGive(xSemaphore);
        }
    }
    return ans;
}

int lora_rx_msg(uint8_t *buf, int size)
{
    int ans = 0;
    if (xSemaphore != NULL)
    {
        // See if we can obtain the semaphore. If the semaphore is not available
        // wait indefinitely to see if it becomes free.
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            // We were able to obtain the semaphore and can now access the
            // shared resource.

            ans = lora_receive_packet(buf, size); // Receive the message

            // We have finished accessing the shared resource. Release the
            // semaphore.
            xSemaphoreGive(xSemaphore);
        }
    }
    return ans;
}

void wifi_tx_msg(void *buffer, uint16_t len)
{
    if (wifi_is_connected) // Verify if the WiFi is connected
    {
        if (xSemaphore != NULL)
        {
            // Try to obtain the semaphore
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
            {
                // Transmit the packet and verifies for errors
                esp_err_t err = esp_wifi_internal_tx(ESP_IF_WIFI_STA, buffer, len);
                if (err != ESP_OK)
                {
                    printf("Error on Wi-Fi tx: %d\n", err);
                }
                else
                {                                        
                    printf("\n->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->");
                    printf("\nWi-Fi tx successful, length: %d", len);
                    printf("\n->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->");
                }

                xSemaphoreGive(xSemaphore); // Release the semaphore
            }
            else
            {
                printf("It was not possible to obtain the semaphore\n");
            }
        }
        else
        {
            printf("Semaphore not started\n");
        }
    }
    else
    {
        printf("Wi-Fi isn't connected\n");
    }
}

void task_rx(void *p)
{
    int x = 0, i = 0;

    for(;;)
    {
        lora_rx_mode();
        while(lora_rxed())
        {
            x = lora_rx_msg(but, sizeof(but));
            but[x] = 0;
            if(x != 0)
            {
                if (but[12] == 0x12 && but[13] == 0x34) // Verify the type of the message
                {
                    // Print the received tick:
                    printf("\n<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-");
                    printf("\nLORA RX TICK: %lu\n", xTaskGetTickCount() * portTICK_PERIOD_MS);
                    printf("<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n");

                    // Update the timer:
                    esp_timer_stop(TX_timer);
                    esp_timer_start_once(TX_timer, (500e3 - (x / 1200) * 1e6));

                    printf("\nLoRa Received (size = %u)\n", x);

                    if (but[21] == 0)
                    {
                        for (i = 0; i < 30; i++)
                        {
                            printf(" %02X", (unsigned int)but[i]);
                        }
                    }
                    else
                    {
                        for (i = 0; i < 22; i++)
                        {
                            printf(" %02X", (unsigned int)but[i]);
                        }
                    }

                    printf("\n");

                    for (; i < x; i++)
                    {
                        printf("%c", (char)but[i]);
                    }

                    printf("\n");

                    wifi_tx_msg((void *)but, x); // Send the message through WiFi
                }
                else
                {
                    printf("Not NG type message\n");
                }
            }
            else
            {
                printf("Empty message\n");
            }
        }

        vTaskDelay(1);
    }
}

// Forward packets from Wi-Fi to LoRa
static esp_err_t WiFi2xQueue(void *buffer, uint16_t len, void *eb)
{
    Xmsg RXmsg;
    if (((char *)buffer)[12] == 0x12 && ((char *)buffer)[13] == 0x34)
    {
        printf("\n<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-");
        printf("\nWiFi Received(size = %u). Putting the message fragment on the xQueue.\n", len);
        printf("<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n");

        if (xSemaphore != NULL)
        {
            // See if we can obtain the semaphore. If the semaphore is not available
            // wait indefinitely to see if it becomes free.
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
            {
                // We were able to obtain the semaphore and can now access the
                // shared resource.
                if (xQueue1 != 0)
                {
                    // Send a packet to the end of the queue.
                    memcpy(&RXmsg.buf[0], (char *)buffer, len);
                    RXmsg.len = len;

                    if (xQueueSendToBack(xQueue1, (void *)&RXmsg, (TickType_t)0) != pdPASS)
                    {
                        printf("MSG QUEUE FAILED");
                        // Failed to post the message.
                    }
                }

                // We have finished accessing the shared resource. Release the
                // semaphore.
                xSemaphoreGive(xSemaphore);
            }
        }
        int i;
        if (((char *)buffer)[21] == 0)
        {
            for (i = 0; i < 30; i++)
            {
                printf(" %02X", (unsigned int)((char *)buffer)[i]);
            }
        }
        else
        {
            for (i = 0; i < 22; i++)
            {
                printf(" %02X", (unsigned int)((char *)buffer)[i]);
            }
        }
        printf(" ");
        for (; i < len; i++)
        {
            printf("%c", ((char *)buffer)[i]);
        }
        printf("\n");
    }
    else
        printf(".");

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

// Event handler for Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            printf("WIFI_EVENT_STA_START\r\n");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            printf("WIFI_EVENT_STA_CONNECTED\r\n");
            wifi_is_connected = true;
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            printf("WIFI_EVENT_STA_DISCONNECTED\r\n");
            wifi_is_connected = false;
            ESP_ERROR_CHECK(esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL));
            break;

        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "Wi-Fi AP got a station connected");
            s_sta_is_connected = true;
            ESP_ERROR_CHECK(esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, WiFi2xQueue));
            break;

        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "Wi-Fi AP got a station disconnected");
            s_sta_is_connected = false;
            ESP_ERROR_CHECK(esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, NULL));
            break;

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:

            printf("IP_EVENT_STA_GOT_IP\r\n");
			// Register the RX callback here to ensure it is ready to receive packets
            
            gpio_set_level(BUILT_IN, 1); //Turn the led on to inform connection

            ESP_ERROR_CHECK(esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, WiFi2xQueue));
            break;

        default:
            break;
        }
    }
}

static void initialize_wifi(void)
{
    esp_netif_init(); // Initialize the network interface

    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Initialize the event loop
    esp_netif_create_default_wifi_sta(); // Create the default Wi-Fi station interface

    // Register wifi_event_handler() as the WiFi callback function
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Configure the operation mode of the WiFi

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_EXAMPLE_WIFI_SSID,
            .password = CONFIG_EXAMPLE_WIFI_PASSWORD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    esp_wifi_set_max_tx_power(78); // Set the maximum transmit power of the WiFi

    ESP_ERROR_CHECK(esp_wifi_start()); // Initialize Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_connect()); // Connect to the network

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             CONFIG_EXAMPLE_WIFI_SSID, CONFIG_EXAMPLE_WIFI_PASSWORD);

    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to assure connection
}

void app_main()
{
    const esp_timer_create_args_t timeout_timer_args = {
        .callback = &timeout_timer_callback,
        /* name is optional, but may help identify the timer when debugging */
        .name = "timeout"};

    ESP_ERROR_CHECK(esp_timer_create(&timeout_timer_args, &TX_timer));
    /* The timer has been created but is not running yet */

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Start Wi-Fi:
    initialize_wifi();

    // Start LoRa:
    lora_init();
    lora_set_frequency(915e6);
    lora_set_bandwidth(250e3);
    lora_set_spreading_factor(7);
    lora_enable_crc();
    
    // Define o LED Built-IN:
    gpio_pad_select_gpio(BUILT_IN);
    gpio_set_direction(BUILT_IN, GPIO_MODE_OUTPUT);

    //Cria a fila:
    xSemaphore = xSemaphoreCreateMutex();
    if (xSemaphore == NULL)
    {
        ESP_LOGI(TAG, "ERRO: Semaphore failed");
    }
    else
    {
        ESP_LOGI(TAG, "Semaphore created");
    }

    xQueue1 = xQueueCreate(50, sizeof(Xmsg));
    if (xQueue1 == NULL)
    {
        ESP_LOGI(TAG, "ERRO: Queue failed");
    }
    else
    {
        ESP_LOGI(TAG, "Queue created");
    }

    xTaskCreatePinnedToCore(&task_rx, "task_rx", 8192, NULL, 5, NULL, 1);
}