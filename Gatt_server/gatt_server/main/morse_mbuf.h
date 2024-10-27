#ifndef MORSE_MBUF_H
#define MORSE_MBUF_H

#include <stdio.h>
#include <os/os_mbuf.h>

/**
 * Create a singular mbuf pool. Current implementation is limited
 * and only supports one mempool at a time.
 * 
 * @return  
 * 
 * @exception when either the mempool or pool of mbufs cannot be initialized.
 */
void mbuf_create_pool();

/**
 * Return a pointer to the memory buffer created in the morse_mbuf.c source file.
 * 
 * @return  A pointer to the mbuf.
 */
struct os_mbuf *mbuf_return_mbuf();

/**
 * Store the data at the given location and length into the mempool. 
 * Reassign the mbuf to the new membuf location.
 * Free up old memory.
 * 
 * @return 0 on success, non-zero on failure.
 */
int mbuf_store(const void *mydata, int mydata_length);

#endif