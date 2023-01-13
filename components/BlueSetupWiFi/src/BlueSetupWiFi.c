#include "BlueSetupWifi.h"
#include <esp_check.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_nimble_hci.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include "GAP_NimBLE.h"
#include "GATT_NimBLE.h"

static const char c_szTAG[] = "BlueSetupWiFi";
static const char c_szKey[] = "SSID_PWD_LIST";

typedef struct BlueSetupWiFi_DATA_STRUCT{
    nvs_handle_t hNVS;
    char *pszSPL;
    uint32_t u32IPv4;
    BlueSetupWiFiLink link; // 0-stop 1-advertising 2-connected
    BlueSetupWiFi_OnStatusChanged onStatusChanged;
} BlueSetupWiFi_data_t;

static BlueSetupWiFi_data_t s_data;

static void on_ble_host_reset(int reason)
{
    ESP_LOGE(c_szTAG, "NimBLE host reset(%d)", reason);
}

static void on_ble_host_sync(void)
{
    // This callback is executed when the host and controller become synced.
    // This happens at startup and after a reset.
    int nRet = GAP_Advertising_start();
    if(nRet) ESP_ERROR_CHECK(nRet);
}

static void on_ble_host_gatt_server(struct ble_gatt_register_ctxt *pCtx, void *pArg)
{
    char buf[64];
    switch(pCtx->op){
        case BLE_GATT_REGISTER_OP_SVC:
            ble_uuid_to_str(pCtx->svc.svc_def->uuid, buf);
            ESP_LOGI(c_szTAG, "Service : %s", buf);
            break;
        case BLE_GATT_REGISTER_OP_CHR:
            ble_uuid_to_str(pCtx->chr.chr_def->uuid, buf);
            ESP_LOGI(c_szTAG, "Charater : %s", buf);
            break;
        case BLE_GATT_REGISTER_OP_DSC:
            ble_uuid_to_str(pCtx->dsc.dsc_def->uuid, buf);
            ESP_LOGI(c_szTAG, "Description : %s", buf);
            break;
        default:
            ESP_LOGW(c_szTAG, "unsupported GATT op: %u", pCtx->op);
    }
}

static void task_nimble(void *pArgs)
{
    // NimBLE event loop
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t BlueSetupWiFi_Init(bool bInitNvs)
{
    // the ESP chip depends on nvs storing RF calibration data internally
    if(bInitNvs){
        esp_err_t espRet = nvs_flash_init();
        if(espRet == ESP_ERR_NVS_NO_FREE_PAGES || espRet == ESP_ERR_NVS_NEW_VERSION_FOUND){
            ESP_ERROR_CHECK(nvs_flash_erase());
            ESP_ERROR_CHECK(nvs_flash_init());
        }
        ESP_LOGI(c_szTAG, "nvs initialized");
    }

    // open nvs for read/write configuration
    ESP_ERROR_CHECK(nvs_open(c_szTAG, NVS_READWRITE, &s_data.hNVS));
    BlueSetupWiFi_NVS_Read();

    // https://docs.espressif.com/projects/esp-idf/zh_CN/v5.0/esp32s3/migration-guides/release-5.x/bluetooth-low-energy.html?highlight=esp_nimble_hci_and_controller_init
    // Controller initialization, enable and HCI initialization calls have been moved to nimble_port_init. This function can be deleted directly.
    // ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

    // register 4 callbacks for ble host
    ble_hs_cfg.gatts_register_cb = on_ble_host_gatt_server;
    ble_hs_cfg.reset_cb = on_ble_host_reset;
    ble_hs_cfg.sync_cb = on_ble_host_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    return ESP_OK;
}

esp_err_t BlueSetupWiFi_Deinit(bool bDeinitNvs)
{
    free(s_data.pszSPL);
    s_data.pszSPL = NULL;

    nvs_close(s_data.hNVS);
    s_data.hNVS = 0;
    // ESP_ERROR_CHECK(esp_nimble_hci_and_controller_deinit());

    if(bDeinitNvs){
        ESP_ERROR_CHECK(nvs_flash_deinit());
        ESP_LOGI(c_szTAG, "nvs deinitialized");
    }

    return ESP_OK;
}

void BlueSetupWiFi_Start(const char* szDevName)
{
    nimble_port_init();

    ESP_ERROR_CHECK(GATT_NimBLE_init());
    // the szName may be truncated
    ESP_ERROR_CHECK(GAP_NimBLE_init(szDevName));

    // start NimBLE task event loop
    nimble_port_freertos_init(task_nimble);
}

esp_err_t BlueSetupWiFi_Stop(void)
{
    ESP_ERROR_CHECK(GATT_NimBLE_deinit());
    // nimble_port_stop() will stop all active GAP advetising internally
    // no need to call GAP_Advertising_stop() explicitly
    ESP_ERROR_CHECK(nimble_port_stop());
    nimble_port_deinit();
    BlueSetupWiFi_SetLink(BlueSetupWiFi_Link_STOP);

    return ESP_OK;
}

const char* BlueSetupWiFi_NVS_Read(void)
{
    size_t len;
    // check for length
    esp_err_t espRet = nvs_get_str(s_data.hNVS, c_szKey, 0, &len);
    if(espRet == ESP_OK){
        s_data.pszSPL = realloc(s_data.pszSPL, len);
        ESP_ERROR_CHECK(nvs_get_str(s_data.hNVS, c_szKey, s_data.pszSPL, &len));
        // ESP_LOGI(c_szTAG, "nvs_get_str: %s", s_pszSPL);
    }
    else if(espRet == ESP_ERR_NVS_NOT_FOUND){
        ESP_LOGI(c_szTAG, "nvs_get_str not found");
        s_data.pszSPL = realloc(s_data.pszSPL, 1);
        *s_data.pszSPL = 0;
    }
    else{
        ESP_LOGE(c_szTAG, "nvs_get_str failed: %d", espRet);
        return 0;
    }
    
    return s_data.pszSPL;
}

const char* BlueSetupWiFi_NVS_Save(char *szConfig)
{
    uint16_t u16Len = strlen(szConfig);
    s_data.pszSPL = realloc(s_data.pszSPL, u16Len+1);
    strcpy(s_data.pszSPL, szConfig);
    ESP_ERROR_CHECK(nvs_set_str(s_data.hNVS, c_szKey, s_data.pszSPL));
    ESP_ERROR_CHECK(nvs_commit(s_data.hNVS));
    // ESP_LOGI(c_szTAG, "nvs_set_str: %s", s_pszSPL);

    return s_data.pszSPL;
}

inline const char* BlueSetupWiFi_GetSPLConfig()
{
    return s_data.pszSPL;
}

inline BlueSetupWiFiLink BlueSetupWiFi_GetLink(void)
{
    return s_data.link;
}

void BlueSetupWiFi_SetLink(BlueSetupWiFiLink link)
{
    if(s_data.link != link){
        s_data.link = link;
        if(s_data.onStatusChanged) s_data.onStatusChanged(link);
    }
}

inline void BlueSetupWiFi_Set_OnStatusChanged(BlueSetupWiFi_OnStatusChanged onStatusChanged)
{
    s_data.onStatusChanged = onStatusChanged;
}

inline BlueSetupWiFi_OnStatusChanged BlueSetupWiFi_Get_OnStatusChanged(void)
{
    return s_data.onStatusChanged;
}

inline void BlueSetupWiFi_SetIPv4(uint32_t u32IPv4)
{
    s_data.u32IPv4 = u32IPv4;
}

inline uint32_t BlueSetupWiFi_GetIPv4(void)
{
    return s_data.u32IPv4;
}