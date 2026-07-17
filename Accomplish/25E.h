#ifndef ACCOMPLISH_25E_H
#define ACCOMPLISH_25E_H

#include "Application/Mission/Mission.h"

/*
 * 25E 题目流程：
 * 等待 KEY1 按下 -> 最多直行 2000 mm -> 检测到黑线后巡线
 * -> 连续丢线 50 拍后转向绝对目标 -> 重新直行并循环。
 *
 * 本文件只保存需要按题目或实车调整的参数。
 * 距离单位为 mm，速度单位为 mm/s，角度单位为 °。
 */

/* KEY1 在 App 按键位图中对应 bit0；使用按下沿启动，长按不会重复触发。 */
#define ACCOMPLISH_25E_START_KEY_MASK              0x01U

/* 每轮直线寻找黑线的最大距离；走满仍未找到线时停车并回到等待。 */
#define ACCOMPLISH_25E_STRAIGHT_DISTANCE_MM        2000U

/* 直线阶段的巡航速度。 */
#define ACCOMPLISH_25E_STRAIGHT_SPEED_MMPS         300.0f

/* 直线走满最大距离后的终点速度；当前为 0，表示平滑减速至停车。 */
#define ACCOMPLISH_25E_STRAIGHT_END_SPEED_MMPS     0.0f

/* 检测到黑线并进入 MotionLine 后使用的巡线速度。 */
#define ACCOMPLISH_25E_LINE_SPEED_MMPS             200.0f

/* 直线阶段必须连续检测到黑线的节拍数；3 拍在 100 Hz 下约为 30 ms。 */
#define ACCOMPLISH_25E_LINE_DETECT_CONFIRM_TICKS   3U

/*
 * 每次转向时在上一绝对目标上增加的角度。
 * 以 KEY1 启动航向为基准，目标依次增加 180°、360°……
 */
#define ACCOMPLISH_25E_TURN_TARGET_OFFSET_DEG      180.0f

/* Nav 转向时每侧车轮的目标速度。 */
#define ACCOMPLISH_25E_TURN_SPEED_MMPS             80.0f

/*
 * 返回 25E 的只读静态状态图。
 * main.c 把该指针传给 Mission_Init()；调用者不得修改状态表内容。
 */
const Mission_GraphDefinition_t *Accomplish25E_GetMissionGraph(void);

#endif
