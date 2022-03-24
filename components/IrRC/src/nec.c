/* NEC IR protocol
NEC-CODE S[0] A[0:7] ~A[0:7] C[0:7] ~C[0:7] E[0]
NEC-REPEAT ????
+ HighLevel / - LowLevel / each segment about 600us
S-Start -16+8
bit0 -1-1
bit1 -1+3
*/
#include "nec.h"
#include <esp_log.h>

static bool decode(rmt_item32_t *pRMTData)
{
    const uint16_t u16s = 563; // segment
    const uint16_t u16t = 100; // tolerance
    const uint16_t uLmin = u16s*3-u16t, uLmax = u16s*3+u16t;
    
    if(pRMTData->duration1 > uLmin && pRMTData->duration1 < uLmax)
        return true;
    else
        return false;
}

static uint8_t decode_range(rmt_item32_t *pRMTData, uint8_t u8s, uint8_t u8len)
{
    uint8_t u8value = 0;
    for(uint16_t i=u8s+u8len-1;i>=u8s;i--){
        rmt_item32_t *pX = pRMTData+i;
        bool b1 = decode(pX);
        u8value = (u8value << 1) | b1;
    }
    return u8value;
}

bool is_nec_protocol(rmt_item32_t *pRMTData, size_t len)
{
    const uint16_t u16s = 563; // segment
    const uint16_t u16t = 100; // tolerance

    if(len == 136){
        if(pRMTData->duration0 > u16s*16-u16t && pRMTData->duration0 < u16s*16+u16t
            && pRMTData->duration1 > u16s*8-u16t && pRMTData->duration1 < u16s*8+u16t){
            return true;
        }
    }
    else if(len == 8){
        if(pRMTData->duration0 > u16s*16-u16t && pRMTData->duration0 < u16s*16+u16t
            && pRMTData->duration1 > u16s*4-u16t && pRMTData->duration1 < u16s*4+u16t){
            return true;
        }
    }

    
    return false;
}

bool nec_decode(rmt_item32_t *pRMTData, size_t len,
                 uint8_t *pu8Code, uint16_t *pu16Addr)
{
    *pu8Code = 0, *pu16Addr = 0;
    uint8_t u8Code[4];

    uint16_t u16Count = len / 4;
    if(u16Count == 34){
        u8Code[0] = decode_range(pRMTData, 1, 8);
        u8Code[1] = decode_range(pRMTData, 9, 8);
        u8Code[2] = decode_range(pRMTData, 17, 8);
        u8Code[3] = decode_range(pRMTData, 25, 8);
        if((u8Code[0]&u8Code[1])==0)
            *pu16Addr = u8Code[0];
        else
            *pu16Addr = (u8Code[1]<<8)|u8Code[0];
        
        *pu8Code = u8Code[2];
        return (u8Code[2]&u8Code[3]) == 0;
    }
    else if(u16Count == 2){
        *pu8Code = 0xff;
        *pu16Addr = 0xffff;

        return true;
    }
    else{
        return false;
    }
}