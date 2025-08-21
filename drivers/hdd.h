#include <stdint.h>

void read_from_disk(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes);
void write_to_disk(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes);
