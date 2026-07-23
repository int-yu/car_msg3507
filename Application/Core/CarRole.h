#ifndef APPLICATION_CORE_CAR_ROLE_H
#define APPLICATION_CORE_CAR_ROLE_H

/*
 * 主从身份（编译期二选一，烧两套固件）。
 *
 *   主车 = BLOOTH 接无线 daplink、负责和电脑/网页通信的那台。
 *   从车 = BLOOTH 悬空、被主车/网页远程指挥的那台。
 *
 * 两车共用同一份源码，只有这个宏不同：
 *   - 烧主车：默认即 CAR_ROLE_MASTER，直接编译。
 *   - 烧从车：把下面默认值改成 CAR_ROLE_SLAVE，或在编译选项里加
 *     -DCAR_ROLE=CAR_ROLE_SLAVE（-D 优先，不必改这个文件）。
 *
 * HC05 主从链路本身是对称的（CarLink 两边跑同一套代码，都能主动收发），
 * 身份只影响“谁转发网页命令 / 谁执行被转发的命令 / 谁在满足条件时触发事件”。
 */

#define CAR_ROLE_MASTER 1
#define CAR_ROLE_SLAVE  2

#ifndef CAR_ROLE
#define CAR_ROLE CAR_ROLE_MASTER /* ← 烧从车时改成 CAR_ROLE_SLAVE */
#endif

#if (CAR_ROLE != CAR_ROLE_MASTER) && (CAR_ROLE != CAR_ROLE_SLAVE)
#error "CAR_ROLE 必须是 CAR_ROLE_MASTER 或 CAR_ROLE_SLAVE"
#endif

#define CAR_IS_MASTER (CAR_ROLE == CAR_ROLE_MASTER)
#define CAR_IS_SLAVE  (CAR_ROLE == CAR_ROLE_SLAVE)

#endif
