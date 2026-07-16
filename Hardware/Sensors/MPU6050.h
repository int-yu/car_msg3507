#ifndef MPU6050_H
#define MPU6050_H

/* MPU6050 软件 I2C 驱动：PA10 为 SCL，PA11 为 SDA。 */

#include <stdint.h>

void MPU6050_Init(void);
uint8_t MPU6050_IsReady(void);
uint8_t MPU6050_GetID(void);
void MPU6050_GetData(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz);
int16_t MPU6050_GetGyroZ(void);

#endif
