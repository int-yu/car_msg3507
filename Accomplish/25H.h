#ifndef ACCOMPLISH_25H_H
#define ACCOMPLISH_25H_H

#include "Application/Mission/Mission.h"

/*
 * 25H 题目流程：
 * 等待 KEY1 -> 巡线 -> 最左侧两路同时检测到黑线
 * -> 向前直行 150 mm -> 左转到下一个绝对 90°目标 -> 继续巡线并循环。
 *
 * 角度使用 MPU6050 连续累计航向，不做正负 180°限幅。
 * 假设 KEY1 启动方向为 0°，各轮左转目标依次为 -90°、-180°、-270°……
 */

/* KEY1 在 App 按键位图中对应 bit0；使用按下沿启动。 */
#define ACCOMPLISH_25H_START_KEY_MASK             0x01U

/* 灰度 bit0 和 bit1 分别为最左侧、左内侧；两位都为 1 才触发。 */
#define ACCOMPLISH_25H_LEFT_MARKER_MASK           0x03U

/* 正常巡线速度。 */
#define ACCOMPLISH_25H_LINE_SPEED_MMPS            200.0f

/* 检测到左侧标志后继续向前行驶的距离。 */
#define ACCOMPLISH_25H_FORWARD_DISTANCE_MM        150U

/* 离开标志线时的直线速度。 */
#define ACCOMPLISH_25H_FORWARD_SPEED_MMPS         200.0f

/* 直行 150 mm 后减速至零，再开始原地转向。 */
#define ACCOMPLISH_25H_FORWARD_END_SPEED_MMPS     0.0f

/* 每轮绝对目标相对上一目标减少 90°，负号对应当前工程的左转方向。 */
#define ACCOMPLISH_25H_TURN_STEP_DEG              (-90.0f)

/* Nav 原地转向时每侧车轮的目标速度。 */
#define ACCOMPLISH_25H_TURN_SPEED_MMPS            80.0f

/* 返回 25H 的只读静态状态图；调用者不得修改状态表内容。 */
const Mission_GraphDefinition_t *Accomplish25H_GetMissionGraph(void);

#endif
