#include "GATT_NimBLE.h"
#include <esp_check.h>
#include <esp_log.h>
#include <host/ble_hs.h>
#include <services/gatt/ble_svc_gatt.h>
#include "BlueSetupWiFi.h"

static char c_szTAG[] = "BlueSetupWiFi:GATT";

// 54bc86d0-6e0c-47ac-9200-7a3e16c2a980
static const ble_uuid128_t c_u128GattSrvAx =
    BLE_UUID128_INIT(0x80, 0xa9, 0xc2, 0x16, 0x3e, 0x7a, 0x00, 0x92,
                     0xac, 0x47, 0x0c, 0x6e, 0xd0, 0x86, 0xbc, 0x54);

// 54bc86d0-6e0c-47ac-9300-7a3e16c2a981
static const ble_uuid128_t c_u128GattSrvAx_C1 =
    BLE_UUID128_INIT(0x81, 0xa9, 0xc2, 0x16, 0x3e, 0x7a, 0x00, 0x93,
                     0xac, 0x47, 0x0c, 0x6e, 0xd0, 0x86, 0xbc, 0x54);

// 54bc86d0-6e0c-47ac-9300-7a3e16c2a982
static const ble_uuid128_t c_u128GattSrvAx_C2 =
    BLE_UUID128_INIT(0x82, 0xa9, 0xc2, 0x16, 0x3e, 0x7a, 0x00, 0x93,
                     0xac, 0x47, 0x0c, 0x6e, 0xd0, 0x86, 0xbc, 0x54);

static int on_gatt_Ax_C0(uint16_t u16connection, uint16_t u16attribute,
    struct ble_gatt_access_ctxt *pCtx, void *pArg)
{
    int nRet = 0;
    switch (pCtx->op){
        case BLE_GATT_ACCESS_OP_READ_CHR:
        {
            uint32_t u32Idx = (uint32_t)pArg;
            if(u32Idx == 1){
                const char *pSPL = BlueSetupWiFi_GetSPLConfig();
                nRet = os_mbuf_append(pCtx->om, pSPL, strlen(pSPL));
            }
            else{
                uint32_t u32IPv4 = BlueSetupWiFi_GetIPv4();
                nRet = os_mbuf_append(pCtx->om, &u32IPv4, sizeof(uint32_t));
            }
            break;
        }
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
        {
            char szBuf[512];
            uint16_t u16Len = OS_MBUF_PKTLEN(pCtx->om);
            
            if(u16Len < 1 || u16Len >= 512) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

            int ret = ble_hs_mbuf_to_flat(pCtx->om, szBuf, 512, &u16Len);
            if(ret != 0) return BLE_ATT_ERR_UNLIKELY;
            szBuf[u16Len] = 0; // force end with '\0'
            ESP_LOGI(c_szTAG, "GATT writeing(%u)", u16Len);
            BlueSetupWiFi_NVS_Save(szBuf);

            break;
        }
        default:
            ESP_LOGW(c_szTAG, "op: %u", pCtx->op);
            break;
    }

    if(nRet != 0){
        ESP_LOGW(c_szTAG, "on_gatt_Ax_C0: %d", nRet);
    }
    return nRet;
}

static const struct ble_gatt_svc_def c_gatt_srv_data[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &(c_u128GattSrvAx.u),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &(c_u128GattSrvAx_C1.u),
                .access_cb = on_gatt_Ax_C0,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .arg = (void*)1
            },
            {
                .uuid = &(c_u128GattSrvAx_C2.u),
                .access_cb = on_gatt_Ax_C0,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = (void*)2
            },
            { 0 } // end characteristics
        }
    },
    { .type = BLE_GATT_SVC_TYPE_END } // end gatt server
};

esp_err_t GATT_NimBLE_init(void)
{
    // ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_gatts_count_cfg(c_gatt_srv_data));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(c_gatt_srv_data));

    return ESP_OK;
}

inline esp_err_t GATT_NimBLE_deinit(void)
{
    return ESP_OK;
}