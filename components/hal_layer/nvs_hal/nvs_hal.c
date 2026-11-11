#include "nvs_hal.h"

#include <string.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

// namespace的自定义名称
#define HAL_NVS_NS          "app"
// nvs的key的最大长度
#define HAL_NVS_KEY_MAX      15
// nvs的版本号（改了某个 key 的“含义”就+1）
#define HAL_NVS_VER          1


static const char *TAG = "nvs_hal";


// 打开自动的命名空间
static bool nvs_hal_open(nvs_handle_t *out)
{
    if (nvs_open(HAL_NVS_NS, NVS_READWRITE, out) != ESP_OK)
    {
        ESP_LOGE(TAG, "open namespace '%s' failed", HAL_NVS_NS);
        return false;
    }
    return true;
}

// 检查key长度是否在范围内
static bool nvs_hal_key_ok(const char *key)
{
    if (key == NULL)
    {
        return false;
    }

    size_t n = strlen(key);// 获取key的长度

    // 检查key的长度是否在范围内
    if (n == 0 || n > HAL_NVS_KEY_MAX)
    {
        ESP_LOGE(TAG, "key '%s' length %u invalid (1~%d)",
                 key, (unsigned)n, HAL_NVS_KEY_MAX);
        return false;
    }

    return true;
}

// 初始化nvs
bool hal_nvs_init(void)
{
    // 初始化nvs_flash
    esp_err_t err = nvs_flash_init();  
    
    // 分区满 / 版本不符：必须擦掉重建，否则永远失败
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "nvs corrupted (%s), erasing", esp_err_to_name(err));
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(err));
        return false;
    }

    uint8_t ver = 0;// 配置版本号，用于判断是否需要重置配置

    // 检查配置版本号是否匹配
    if (!hal_nvs_get_u8("cfg_ver", &ver) || ver != HAL_NVS_VER)    
    {
        // 配置版本号不符，需要重置配置
        ESP_LOGW(TAG, "cfg version %u -> %u, resetting config",
                 (unsigned)ver, HAL_NVS_VER);

        hal_nvs_erase_all();// 擦除所有配置项

        // 写入新的配置版本号
        if (!hal_nvs_set_u8("cfg_ver", HAL_NVS_VER))
        {
            return false;
        }
    }

    ESP_LOGI(TAG, "ready (ns='%s', ver=%d)", HAL_NVS_NS, HAL_NVS_VER);
    return true;
}

// 写入uint8_t
bool hal_nvs_set_u8(const char *key, uint8_t v)
{
    // 检查key是否有效
    if (!nvs_hal_key_ok(key))
    {
        return false;
    }

    nvs_handle_t h;// nvs的句柄，用于后续操作
    // 打开命名空间
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    // 写入配置项
    esp_err_t err = nvs_set_u8(h, key, v);
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set u8 '%s' failed: %s", key, esp_err_to_name(err));
    }

    return (err == ESP_OK);
}

// 写入uint16_t
bool hal_nvs_set_u16(const char *key, uint16_t v)
{
    if (!nvs_hal_key_ok(key))
    {
        return false;
    }

    nvs_handle_t h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    esp_err_t err = nvs_set_u16(h, key, v);
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set u16 '%s' failed: %s", key, esp_err_to_name(err));
    }

    return (err == ESP_OK);
}

// 写入uint32_t
bool hal_nvs_set_u32(const char *key, uint32_t v)
{
    if (!nvs_hal_key_ok(key))
    {
        return false;
    }

    nvs_handle_t h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    esp_err_t err = nvs_set_u32(h, key, v);
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set u32 '%s' failed: %s", key, esp_err_to_name(err));
    }

    return (err == ESP_OK);
}

// 读取uint8_t
bool hal_nvs_get_u8(const char *key, uint8_t *out)
{
    if (!nvs_hal_key_ok(key) || out == NULL)
    {
        return false;
    }

    nvs_handle h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    esp_err_t err = nvs_get_u8(h, key, out);
    nvs_close(h);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return false;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "get u8 '%s' failed: %s", key, esp_err_to_name(err));
    }

    return (err == ESP_OK);
}

// 读取uint16_t
bool hal_nvs_get_u16(const char *key, uint16_t *out)
{
    if (!nvs_hal_key_ok(key) || out == NULL)
    {
        return false;
    }

    nvs_handle h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    esp_err_t err = nvs_get_u16(h, key, out);
    nvs_close(h);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return false;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "get u16 '%s' failed: %s", key, esp_err_to_name(err));
    }

    return (err == ESP_OK);
}

// 读取uint32_t
bool hal_nvs_get_u32(const char *key, uint32_t *out)
{
    if (!nvs_hal_key_ok(key) || out == NULL)
    {
        return false;
    }

    nvs_handle h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    esp_err_t err = nvs_get_u32(h, key, out);
    nvs_close(h);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return false;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "get u32 '%s' failed: %s", key, esp_err_to_name(err));
    }

    return (err == ESP_OK);
}

// 写入字符串
bool hal_nvs_set_str(const char *key, const char *str)
{
    if (!nvs_hal_key_ok(key) || str == NULL)
    {
        return false;
    }

    nvs_handle h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    esp_err_t err = nvs_set_str(h, key, str);
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set str '%s' failed: %s", key, esp_err_to_name(err));
    }

    return (err == ESP_OK);
}

// 读取字符串
bool hal_nvs_get_str(const char *key, char *buf, size_t buf_len)
{
    if (!nvs_hal_key_ok(key) || buf == NULL || buf_len == 0)
    {
        return false;
    }

    nvs_handle h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    esp_err_t err = nvs_get_str(h, key, buf, &buf_len);
    nvs_close(h);

    if (err == ESP_ERR_NVS_NOT_FOUND)// 配置项不存在
    {
        return false;
    }

    if (err == ESP_ERR_NVS_INVALID_LENGTH)// 缓冲区太小
    {
        ESP_LOGE(TAG, "get str '%s': buffer too small (need %u)",
                 key, (unsigned)buf_len);
        return false;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "get str '%s' failed: %s", key, esp_err_to_name(err));
    }
    
    return (err == ESP_OK);
}

// 删除配置项
bool hal_nvs_erase_key(const char *key)
{
    if (!nvs_hal_key_ok(key))
    {
        return false;
    }

    nvs_handle h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }
    
    // 删除配置项
    esp_err_t err = nvs_erase_key(h, key);
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }
    nvs_close(h);

    // 本来就没有，也算"擦干净了"
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return true;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "erase '%s' failed: %s", key, esp_err_to_name(err));
    }
    return (err == ESP_OK);
}

// 清空本 namespace 下【全部】key
bool hal_nvs_erase_all(void)
{
    nvs_handle_t h;
    if (!nvs_hal_open(&h))
    {
        return false;
    }

    // 清空所有配置项
    esp_err_t err = nvs_erase_all(h);
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "erase all failed: %s", esp_err_to_name(err));
    }
    return (err == ESP_OK);
}
