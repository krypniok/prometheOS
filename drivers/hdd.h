#include <stdint.h>

void read_from_disk(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes);
void write_to_disk(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes);
void read_from_disk_fast(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes);
void write_to_disk_fast(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes);
int dma_read(uint32_t lba, void* buffer, uint32_t bytes);
int dma_write(uint32_t lba, void* buffer, uint32_t bytes);
