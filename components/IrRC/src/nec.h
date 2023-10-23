#pragma once
#include <stdio.h>
#include <driver/rmt_rx.h>

bool is_nec_protocol(rmt_symbol_word_t *pRMTData, size_t len);
bool nec_decode(rmt_symbol_word_t *pRMTData, size_t len,
                 uint8_t *pu8Code, uint16_t *pu16Addr);