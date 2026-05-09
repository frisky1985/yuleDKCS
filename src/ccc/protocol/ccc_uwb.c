/******************************************************************************
 * @file    ccc_uwb.c
 * @brief   CCC Digital Key R3 UWB 测距实现
 ******************************************************************************/

#include "ccc_uwb.h"
#include <string.h>

static ccc_uwb_session_t g_uwb_session;

int ccc_uwb_init_ranging(const ccc_uwb_config_t *config) {
    if (config == NULL) return CCC_UWB_ERROR_INVALID_PARAM;
    
    memcpy(&g_uwb_session.config, config, sizeof(ccc_uwb_config_t));
    g_uwb_session.state = CCC_UWB_STATE_INITIALIZED;
    
    /* STS 密钥派生 */
    return CCC_UWB_SUCCESS;
}

int ccc_uwb_start_ranging(void) {
    if (g_uwb_session.state != CCC_UWB_STATE_INITIALIZED) {
        return CCC_UWB_ERROR_NOT_INITIALIZED;
    }
    
    /* 启动 UWB 测距 */
    g_uwb_session.state = CCC_UWB_STATE_RANGING;
    g_uwb_session.sequence_number = 0;
    
    return CCC_UWB_SUCCESS;
}

int ccc_uwb_stop_ranging(void) {
    g_uwb_session.state = CCC_UWB_STATE_INITIALIZED;
    return CCC_UWB_SUCCESS;
}

int ccc_uwb_get_distance(float *distance_m, float *accuracy_m) {
    if (distance_m == NULL || accuracy_m == NULL) return CCC_UWB_ERROR_INVALID_PARAM;
    if (g_uwb_session.state != CCC_UWB_STATE_RANGING) return CCC_UWB_ERROR_NOT_RANGING;
    
    /* 模拟测距结果 */
    *distance_m = 2.5f;
    *accuracy_m = 0.1f;
    
    return CCC_UWB_SUCCESS;
}

int ccc_uwb_get_azimuth(float *azimuth_deg) {
    if (azimuth_deg == NULL) return CCC_UWB_ERROR_INVALID_PARAM;
    *azimuth_deg = 0.0f;
    return CCC_UWB_SUCCESS;
}

int ccc_uwb_get_elevation(float *elevation_deg) {
    if (elevation_deg == NULL) return CCC_UWB_ERROR_INVALID_PARAM;
    *elevation_deg = 0.0f;
    return CCC_UWB_SUCCESS;
}
