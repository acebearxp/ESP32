#include "GAP_NimBLE.h"
#include <esp_check.h>
#include <esp_log.h>
#include <services/gap/ble_svc_gap.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include "BlueSetupWiFi.h"

static char c_szTAG[] = "BlueSetupWiFi:GAP";
static int start_advertising();

static int on_gap_event(struct ble_gap_event *pEvent, void *pArgs)
{
    struct ble_gap_conn_desc desc;

    int nRet = 0;
    switch(pEvent->type){
        case BLE_GAP_EVENT_CONNECT:
            BlueSetupWiFi_SetLink(BlueSetupWiFi_Link_CONNECTED);
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            nRet = start_advertising();
            if(nRet) ESP_ERROR_CHECK(nRet);
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            ble_gap_conn_find(pEvent->connect.conn_handle, &desc);
            ESP_LOGI(c_szTAG, "NimBLE GAP: encryption %s (key size: %u)",
                desc.sec_state.encrypted?"encrypted":"unencrypted",
                desc.sec_state.key_size);
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            nRet = BLE_GAP_REPEAT_PAIRING_IGNORE;
            ESP_LOGI(c_szTAG, "NimBLE GAP: repeat pairing");
            break;
        case BLE_GAP_EVENT_SCAN_REQ_RCVD:
            ESP_LOGI(c_szTAG, "NimBLE GAP: scan request received!");
            break;
        default:
            ESP_LOGI(c_szTAG, "NimBLE GAP Event: %u", pEvent->type);
            break;
    }

    return nRet;
}

esp_err_t GAP_NimBLE_init(const char* szDevName)
{
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(szDevName));
    ESP_ERROR_CHECK(ble_svc_gap_device_appearance_set(0x0180));
    ble_svc_gap_init();
   
    return ESP_OK;
}

static int update_advertising_data()
{
    const char* szDevName = ble_svc_gap_device_name();

    struct ble_hs_adv_fields adv_data;
    memset(&adv_data, 0, sizeof(struct ble_hs_adv_fields));
    adv_data.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_data.tx_pwr_lvl_is_present = 1;
    adv_data.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_data.name = (uint8_t*)szDevName;
    adv_data.name_len = strlen(szDevName);
    adv_data.name_is_complete = 1;
    adv_data.appearance = ble_svc_gap_device_appearance();
    adv_data.appearance_is_present = 1;
    int nRet = 0;
    do{
        // truncate name if too long
        if(nRet == BLE_HS_EMSGSIZE) adv_data.name_len--;
        nRet = ble_gap_adv_set_fields(&adv_data);
    }
    while(nRet == BLE_HS_EMSGSIZE && adv_data.name_len > 1);
    ESP_ERROR_CHECK(nRet);

    return nRet;
}

static int start_advertising()
{
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(struct ble_gap_adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    // adv_params.itvl_min = 500;
    // adv_params.itvl_max = 5000;
    int nRet = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, 0, BLE_HS_FOREVER, &adv_params, on_gap_event, 0);
    BlueSetupWiFi_SetLink(nRet == 0 ? BlueSetupWiFi_Link_ADVERTISING : BlueSetupWiFi_Link_STOP);
    return nRet;
}

int GAP_Advertising_start()
{
    ESP_ERROR_CHECK(update_advertising_data());
    return start_advertising();
}

esp_err_t GAP_Advertising_stop()
{
    ESP_ERROR_CHECK(ble_gap_adv_stop());
    BlueSetupWiFi_SetLink(BlueSetupWiFi_Link_STOP);
    return ESP_OK;
}