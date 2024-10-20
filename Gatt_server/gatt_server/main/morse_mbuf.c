// #include "morse_mbuf.h"

// #define MBUF_PKTHDR_OVERHEAD    sizeof(struct os_mbuf_pkthdr)
// #define MBUF_MEMBLOCK_OVERHEAD  sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD

// #define MBUF_NUM_MBUFS      (32)
// #define MBUF_PAYLOAD_SIZE   (64)
// #define MBUF_BUF_SIZE       OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
// #define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
// #define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

// struct os_mbuf_pool g_mbuf_pool;
// struct os_mempool g_mbuf_mempool;
// os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];


// void
// create_mbuf_pool(os_mbuf_pool g_mbuf_pool, os_mempool g_mbuf_mempool, os_membuf_t g_mbuf_buffer)
// {
//     int rc;

//     rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS,
//                           MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "mbuf_pool");
//     assert(rc == 0);

//     rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE,
//                            MBUF_NUM_MBUFS);
//     assert(rc == 0);
// }