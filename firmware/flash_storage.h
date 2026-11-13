#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include <stdint.h>
#include "ch32fun.h"

// This is "erased" value that flash controller marks words that are actually 0xffffffff
// This is usually annoying, but here it helps us determine the last stored position
// Because even if we write 0xffffffff it will read back correctly. Only unwritten flash
// is marked with "magic" value.
#define MAGIC 0xf3f9bda9
#define STORAGE_ADDRESS 0x3b000
#define STORAGE_SIZE 4096

void erase_storage();
uint32_t read_storage();
void update_storage(uint32_t new_word);

#endif
