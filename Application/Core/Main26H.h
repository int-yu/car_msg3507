#ifndef APPLICATION_CORE_MAIN_26H_H
#define APPLICATION_CORE_MAIN_26H_H

/*
 * 上电自动执行要求3。任务仍会等待MT6816建立绝对角基准、步进完成水平
 * 定位且K230钢球位置有效，不会在反馈尚未就绪时盲目启动。
 */
#define MAIN26H_BALL_AUTO_START_ENABLED       1U
#define MAIN26H_HORIZONTAL_READY_TOLERANCE_DEG 1.0f

/* Runs the complete 26H line-following and ball-control application. */
void Main26H_Run(void);

#endif
