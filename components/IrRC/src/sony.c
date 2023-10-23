/* SONY IR protocol
SONY-12 S[0] C[0:6] D[0:4]
SONY-15 S[0] C[0:6] D[0:7]
SONY-20 S[0] C[0:6] D[0:4] E[0:7]
+ HighLevel / - LowLevel / each segment about 600us
S-Start -4+1
bit0 -1+1
bit1 -2+1
*/
#include "sony.h"
#include <esp_log.h>

static bool decode(rmt_symbol_word_t *pRMTData)
{
    const uint16_t u16s = 600; // segment
    const uint16_t u16t = 100; // tolerance
    const uint16_t uLmin = u16s*2-u16t, uLmax = u16s*2+u16t;
    
    if(pRMTData->duration0 > uLmin && pRMTData->duration0 < uLmax)
        return true;
    else
        return false;
}

static uint8_t decode_range(rmt_symbol_word_t *pRMTData, uint8_t u8s, uint8_t u8len)
{
    uint8_t u8value = 0;
    for(uint16_t i=u8s+u8len-1;i>=u8s;i--){
        rmt_symbol_word_t *pX = pRMTData+i;
        bool b1 = decode(pX);
        u8value = (u8value << 1) | b1;
    }
    return u8value;
}

bool is_sony_protocol(rmt_symbol_word_t *pRMTData, size_t len)
{
    const uint16_t u16s = 600; // segment
    const uint16_t u16t = 100; // tolerance
    if(len == (12+1)*4 || len == (15+1)*4 || len == (20+1)*4){
        if(pRMTData->duration0 > u16s*4-u16t && pRMTData->duration0 < u16s*4+u16t
            && pRMTData->duration1 > u16s-u16t && pRMTData->duration1 < u16s+u16t){
            return true;
        }
    }
    
    return false;
}

bool sony_decode(rmt_symbol_word_t *pRMTData, size_t len,
                 uint8_t *pu8Code, uint8_t *pu8Dev, uint8_t *pu8Ext)
{
    *pu8Code = 0, *pu8Dev = 0, *pu8Ext = 0;

    uint16_t u16Count = len / 4;
    if(u16Count == (12+1) || u16Count == (20+1)){
        *pu8Code = decode_range(pRMTData, 1, 7);
        *pu8Dev = decode_range(pRMTData, 8, 5);
        if(u16Count == (20+1)){
             *pu8Ext = decode_range(pRMTData, 13, 8);
        }
        return true;
    }
    else if(u16Count == (15+1)){
        *pu8Code = decode_range(pRMTData, 1, 7);
        *pu8Dev = decode_range(pRMTData, 8, 8);
        return true;
    }
    else{
        return false;
    }
}