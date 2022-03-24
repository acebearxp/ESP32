#pragma once
#include <stdio.h>
#include <driver/rmt.h>

bool is_sony_protocol(rmt_item32_t *pRMTData, size_t len);
bool sony_decode(rmt_item32_t *pRMTData, size_t len,
                 uint8_t *pu8Code, uint8_t *pu8Dev, uint8_t *pu8Ext);