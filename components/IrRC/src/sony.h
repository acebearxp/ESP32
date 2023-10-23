#pragma once
#include <stdio.h>
#include <driver/rmt_rx.h>

bool is_sony_protocol(rmt_symbol_word_t *pRMTData, size_t len);
bool sony_decode(rmt_symbol_word_t *pRMTData, size_t len,
                 uint8_t *pu8Code, uint8_t *pu8Dev, uint8_t *pu8Ext);