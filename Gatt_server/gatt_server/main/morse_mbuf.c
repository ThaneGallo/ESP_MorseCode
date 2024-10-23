#include "morse_mbuf.h"
#include "esp_log.h"

#define MBUF_PKTHDR_OURUSER     0
#define MBUF_PKTHDR_OVERHEAD    sizeof(struct os_mbuf_pkthdr) + MBUF_PKTHDR_OURUSER // replace ouruser header with sizeof when/if we use actual header
#define MBUF_MEMBLOCK_OVERHEAD  sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD

#define MBUF_NUM_MBUFS      (8)
#define MBUF_PAYLOAD_SIZE   (64)
#define MBUF_BUF_SIZE       OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

#define GATTS_TAG "BLE-Server"

struct os_mbuf_pool g_mbuf_pool;
struct os_mempool g_mbuf_mempool;
os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];
static struct os_mbuf *morse_data_buf;

void
mbuf_create_pool()
{
    int rc;

    rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS,
                          MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "mbuf_pool");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE,
                           MBUF_NUM_MBUFS);
    assert(rc == 0);
}

struct os_mbuf *
mbuf_return_mbuf() {
    return morse_data_buf;
};

int
mbuf_store(const void *mydata, int mydata_length)
{
    int rc;
    struct os_mbuf *om;
    uint8_t *src = mydata; // source pointer, typecast to uint8_t to allow char* as input

    /* check that the data is not too large for our pool */
    if (mydata_length + MBUF_MEMBLOCK_OVERHEAD > MBUF_MEMPOOL_SIZE) {
        /* Error! Would not be able to allocate enough mbufs for total packet length */
        ESP_LOGI(GATTS_TAG, "Huge Packet Detected! Would not be able to allocate enough mbufs for total packet length");
        return -1;
    }

    /* free up the space of the last mbuf now that we have successfully created the new one, assuming the last mbuf exists */
    if (morse_data_buf) {
        os_mbuf_free(morse_data_buf);
    }
    
    /* get a packet header mbuf */
    om = os_mbuf_get_pkthdr(&g_mbuf_pool, MBUF_PKTHDR_OURUSER);
    if (!om) {
        ESP_LOGI(GATTS_TAG, "om pointer failed for creating a mbuf");
        return -1;
    }
    /*
    * Copy user data into mbuf. NOTE: if mydata_length is greater than the
    * mbuf payload size (64 bytes using above example), mbufs are allocated
    * and chained together to accommodate the total packet length.
    */
    rc = os_mbuf_copyinto(om, 0, src, mydata_length);
    if (rc) {
        /* Error! Could not allocate enough mbufs for total packet length */
        ESP_LOGI(GATTS_TAG, "Could not allocate enough mbufs for total packet length");
        return -1;
    }

    /* if mbuf creation and copy is successfull, then reassign morse_data_buf */
    morse_data_buf = om;
    return 0;
    // /* Send packet to networking interface */
    // send_pkt(om);
}