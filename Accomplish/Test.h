#ifndef ACCOMPLISH_TEST_H
#define ACCOMPLISH_TEST_H

#include "Application/Mission/Mission.h"

/*
 * 刹车测试任务：
 * 等待 KEY2 -> 定距直行并按 MotionStraight 速度曲线减速 ->
 * MotionManager 短暂主动刹车 -> 返回等待。
 *
 * 使用时，在 main.c 中临时包含本头文件，并改为：
 * Mission_Init(AccomplishTest_GetMissionGraph());
 * 测试结束后恢复实际题目的 Accomplish 状态图。
 */

/* KEY2 在 App 按键位图中对应 bit1，使用按下沿触发一次测试。 */
#define ACCOMPLISH_TEST_START_KEY_MASK              0x02U

/* 测试用直线距离；修改后可观察到点前减速和刹车后的滑行量。 */
#define ACCOMPLISH_TEST_BRAKE_DISTANCE_MM           500U

/* 测试用巡航速度。速度越高，主动刹车与纯滑行的差异越容易观察。 */
#define ACCOMPLISH_TEST_BRAKE_SPEED_MMPS            200.0f

/* 必须设为零，直线模块才会完成并进入后续 BRAKE 状态。 */
#define ACCOMPLISH_TEST_BRAKE_END_SPEED_MMPS        0.0f

/* 返回刹车测试的只读静态状态图；调用者不得修改状态表内容。 */
const Mission_GraphDefinition_t *AccomplishTest_GetMissionGraph(void);

#endif
