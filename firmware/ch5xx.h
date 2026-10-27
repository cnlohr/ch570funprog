#ifndef CH5XXH
#define CH5XXH


#undef R8_SAFE_ACCESS_SIG
#undef R8_WDOG_COUNT
#undef R8_RESET_STATUS
#undef R8_GLOB_ROM_CFG
#undef R8_GLOB_CFG_INFO
#undef R8_RST_WDOG_CTRL
#undef R8_GLOB_RESET_KEEP
#undef R32_FLASH_DATA
#undef R32_FLASH_CONTROL
#undef R8_FLASH_DATA
#undef R8_FLASH_CTRL
#undef R8_FLASH_CFG

#define R32_FLASH_DATA_A         0x40001800

#define MCFDetermineChipType(x)   DetermineChipTypeAndSectorInfo(x, NULL)
#define MCFDelayUS             Delay_Us
#define MCFReadByte            ReadByte
#define MCFReadWord            ReadWord
#define MCFWriteByte           WriteByte
#define MCFWriteWord           WriteWord
#define MCFWaitForDoneOp       WaitForDoneOp

int ch5xx_set_clock(struct SWIOState * iss, uint32_t clock);
uint8_t ch5xx_flash_wait(struct SWIOState * iss);
void ch5xx_flash_close(struct SWIOState * iss);
uint8_t ch5xx_flash_open(struct SWIOState * iss, uint8_t op);
uint8_t ch5xx_flash_addr(struct SWIOState * iss, uint8_t cmd, uint32_t addr);
uint8_t ch5xx_flash_in(struct SWIOState * iss);
void ch5xx_flash_out(struct SWIOState * iss, uint8_t addr);
void ch5xx_flash_end(struct SWIOState * iss);
uint8_t ch5xx_flash_begin(struct SWIOState * iss, uint8_t cmd);
void ch5xx_write_safe(struct SWIOState * iss, uint32_t addr, uint32_t value, uint8_t mode);
int ch5xx_read_options(struct SWIOState * iss, uint32_t addr, uint8_t* buffer);
int ch5xx_read_options_bulk(struct SWIOState * iss, uint32_t addr, uint8_t* buffer, uint32_t len);
int ch5xx_read_secret_uuid(struct SWIOState * iss, uint8_t* buffer);
int ch5xx_read_uuid(struct SWIOState * iss, uint8_t* buffer);
int ch5xx_write_flash(struct SWIOState * iss, uint32_t start_addr, uint8_t* data, uint32_t len);
int ch5xx_microblob_init(struct SWIOState * iss, uint32_t start_addr);
void ch5xx_microblob_end(struct SWIOState * iss);
int ch5xx_write_flash_using_microblob(struct SWIOState * iss, uint32_t start_addr, uint8_t* data, uint32_t len);
void ch570_disable_acauto(struct SWIOState * iss);
void ch570_disable_read_protection(struct SWIOState * iss);
int ch5xx_verify_data(struct SWIOState * iss, uint32_t addr, uint8_t* data, uint32_t len);
int CH5xxErase(struct SWIOState * iss, uint32_t addr, uint32_t len, enum MemoryArea area);
int ch5xx_microblob_init(struct SWIOState * iss, uint32_t start_addr);

#include "ch5xx.c"

#endif
