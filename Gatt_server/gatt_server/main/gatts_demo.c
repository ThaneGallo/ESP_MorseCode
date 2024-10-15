/*10/15/24*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"


#define GATTS_TAG "BLE-Server"
static uint8_t white_list_count = 1;

static const ble_addr_t serverAddr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xDE, 0xCA, 0xFB, 0xEE, 0xFE, 0xD2}
    };

static const ble_addr_t clientAddr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xCA, 0xFF, 0xED, 0xBE, 0xEE, 0xEF}
    };

static const ble_addr_t *serverPtr = &serverAddr;
static const ble_addr_t *clientPtr = &clientAddr;

// ________________________________________________________________________________________
// #define MBUF_PKTHDR_OVERHEAD    sizeof(struct os_mbuf_pkthdr) + sizeof(struct user_hdr)
// #define MBUF_MEMBLOCK_OVERHEAD  sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD

// #define MBUF_NUM_MBUFS      (32)
// #define MBUF_PAYLOAD_SIZE   (64)
// #define MBUF_BUF_SIZE       OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
// #define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
// #define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)
// struct os_mbuf_pool g_mbuf_pool;
// struct os_mempool g_mbuf_mempool;
// os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];
static struct os_mbuf *morse_data_buf;
// __________________________________________________________________________________________

// old 16 bits
// #define SERVICE_UUID 0xCAFE
// #define READ_UUID 0xCAFF
// #define WRITE_UUID  0xDECA
// new 128 bits
#define SERVICE_UUID {0xCAFECAFECAFECAFECAFECAFECAFECAFE} // 128-bit base UUID
#define READ_UUID {0xCAFFCAFFCAFFCAFFCAFFCAFFCAFFCAFF} // 128-bit base UUID
#define WRITE_UUID {0xDEBADEBADEBADEBADEBADEBADEBADEBA} // 128-bit base UUID

void ble_app_advertise(void);

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);

    char *data = (char *)ctxt->om->om_data;
    printf("%d\n", strcmp(data, (char *)"LIGHT ON") == 0);

    //override the read characteristic with data ctxt->om->om_data, at length

    return 0;
}

// Read data from ESP32 defined as server
// static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
// {
//     os_mbuf_append(ctxt->om, "Data from the server", strlen("Data from the server"));
//     //os_mbuf_append(ctxt->om, const pointer to data location, length of data);
//     return 0;
// }

// Read or write data from ESP32 defined as server
static int device_morse(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    switch(ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR: {
            //os_mbuf_append(ctxt->om, "Data from the server", strlen("Data from the server"));
            //rc = os_mbuf_copydata(morse_data_buf, 0, morse_data_buf->om_len, ctxt->om);
            rc = os_mbuf_append(ctxt->om, morse_data_buf->om_data, morse_data_buf->om_len);
            printf("Data requested by client: %.*s\n", morse_data_buf->om_len, morse_data_buf->om_data);
            return rc;
        }
        case BLE_GATT_ACCESS_OP_WRITE_CHR: {
            printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);
            rc = os_mbuf_copyinto(morse_data_buf, 0, ctxt->om->om_data, ctxt->om->om_len);
            // if (rc != 0) {
            //     return rc;
            // }
            return rc; // if we do anything after mbuf_copyinto in the future, maybe we add back the if statement
        }
        default: {
            printf("Bad Op: %u\n", ctxt->op);
            return ctxt->op; // this shouldn't ever come up in our usages.
        }
    }
    
    //os_mbuf_append(ctxt->om, const pointer to data location, length of data);
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID128_DECLARE(0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA), // Define UUID for device type
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID128_DECLARE(0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA), // Define UUID for reading
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
          .access_cb = device_morse},
         {.uuid = BLE_UUID128_DECLARE(0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE), // Define UUID for writing
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write},
         {0}}},
    {0}}; // remember that .type of 0 is BLE_GATT_SVC_TYPE_END, so we initialize everything to 0.

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(GATTS_TAG, "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0) // if no good connection, readvertise
        {
            ble_app_advertise();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(GATTS_TAG, "BLE GAP EVENT DISCONNECTED"); //breaks after first disconnect 
        ble_app_advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(GATTS_TAG, "BLE GAP EVENT");
        ble_app_advertise();
        break;
    default:
        ESP_LOGI(GATTS_TAG, "This event is not supported: %u", event->type);
        break;
    }
    return 0;
}

// Define the BLE connection
void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    // adv_params.filter_policy = BLE_HCI_SCAN_FILT_USE_WL; // ***USE A WHITELIST***
    adv_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL; // ***DONT USE A WHITELIST***
    ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    uint8_t err;
    //ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically

    err = ble_hs_id_set_rnd(serverPtr->val); 
    if (err != 0)
    {
        ESP_LOGI(GATTS_TAG, "BLE gap set random address failed %d", err);
    }

    err = ble_gap_wl_set(clientPtr, white_list_count); // sets white list for connection to other device
    if (err != 0)
    {
        ESP_LOGI(GATTS_TAG, "BLE gap set whitelist failed %d", err);
    }

    char greetings[] = "Hello World!";
    char *initial_buf_data = greetings;
    uint8_t initial_buf_len = 13;
    err = os_mbuf_copyinto(morse_data_buf, 0, initial_buf_data, initial_buf_len);
    if (err != 0)
    {
        ESP_LOGI(GATTS_TAG, "initial mbuf data fail %d", err);
    }

    ble_app_advertise(); // Define the BLE connection
}

// The infinite task
void host_task(void *param)
{

    ESP_LOGI(GATTS_TAG, "in host task");
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

void app_main()
{
    nvs_flash_init(); // 1 - Initialize NVS flash using
    // esp_nimble_hci_and_controller_init();      // 2 - Initialize ESP controller
    nimble_port_init();                        // 3 - Initialize the host stack
    ble_svc_gap_device_name_set("BLE-Server"); // 4 - Initialize NimBLE configuration - server name
    ble_svc_gap_init();                        // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                       // 4 - Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);            // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);             // 4 - Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;      // 5 - Initialize application
    nimble_port_freertos_init(host_task);      // 6 - Run the thread
}