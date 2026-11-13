#include "flash_storage.h"
#include "ch5xx_flash.h"

static uint32_t* stored_word;

// Find the memory address of the last stored word
static void find_word() {
  // Sweep across all dedicated storage area
  for (uint32_t i = 0; i < STORAGE_SIZE; i += 4) {
    // If the next word is marked as erased, use current position
    if (*(stored_word+1) == MAGIC) break;
    stored_word = (uint32_t*)(STORAGE_ADDRESS+i);
  }
}

void erase_storage() {
  ch5xx_flash_cmd_erase(STORAGE_ADDRESS, STORAGE_SIZE);
  stored_word = (uint32_t*)STORAGE_ADDRESS;
}

uint32_t read_storage() {
  // If pointer is zero, consider uninitialized and find the actual address
  if (stored_word == 0) find_word();

  return *stored_word;
}

void update_storage(uint32_t new_word) {
  if (stored_word == 0) find_word();

  if ((uint32_t)stored_word == (STORAGE_ADDRESS + STORAGE_SIZE - 4)) {
    // Only need to erase storage if we've reached the end of the storage area
    erase_storage();
  } else {
    // Unless we at the start of uninitialized storage, increment the position
    if (*stored_word != MAGIC) stored_word = (uint32_t*)((uint32_t)stored_word + 4);
  }

  ch5xx_flash_cmd_write((uint32_t)stored_word, (uint8_t*)&new_word, 4);
}
