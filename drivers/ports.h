#pragma once

#include <stdint.h>

unsigned char port_byte_in(uint16_t port);

void port_byte_out(uint16_t port, uint8_t data);

unsigned short port_word_in(uint16_t port);

  void port_word_out(uint16_t port, uint16_t data);

unsigned int port_long_in(uint16_t port);

void port_long_out(uint16_t port, uint32_t data);

void io_wait();
