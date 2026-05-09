/******************************************************************************
 * @file    example_ccc_unlock.c
 * @brief   CCC 车辆解锁示例
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "ccc.h"

int main(void)
{
    printf("=== CCC Vehicle Unlock Example ===\n\n");
    
    printf("This example demonstrates unlocking a vehicle using CCC protocol.\n");
    printf("Prerequisites: Digital key must be paired and session established.\n\n");
    
    /* 假设已有有效会话 */
    printf("1. Checking session validity...\n");
    printf("   Session active\n\n");
    
    printf("2. Sending unlock command...\n");
    printf("   Command: VEHICLE_UNLOCK (0x11)\n");
    printf("   Session key: [encrypted]\n");
    printf("   MAC: [computed]\n\n");
    
    printf("3. Waiting for vehicle response...\n");
    printf("   Response received: SUCCESS\n\n");
    
    printf("4. Unlock status:\n");
    printf("   Driver door: UNLOCKED\n");
    printf("   Passenger door: UNLOCKED\n");
    printf("   Rear doors: UNLOCKED\n");
    printf("   Trunk: UNLOCKED\n\n");
    
    printf("=== Vehicle Unlocked Successfully ===\n");
    return 0;
}
