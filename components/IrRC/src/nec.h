#pragma once
#include <stdio.h>
#include <driver/rmt.h>

bool is_nec_protocol(rmt_item32_t *pRMTData, size_t len);
bool nec_decode(rmt_item32_t *pRMTData, size_t len,
                 uint8_t *pu8Code, uint16_t *pu16Addr);