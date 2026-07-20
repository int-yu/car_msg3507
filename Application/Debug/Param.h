#ifndef APPLICATION_DEBUG_PARAM_H
#define APPLICATION_DEBUG_PARAM_H

/* 运行时调参注册表：K 命令的后端，参数一览见 README 3.3.1。
 * 所有参数掉电后恢复各模块头文件顶部的 #define 默认值；
 * 调好的数值由上位机保存，需要固化时人工写回 #define。 */

/* 处理 K 命令去掉前缀后的文本参数并直接通过 UART1 回应：
 * "?"          -> 逐行列出全部参数：K<id>=<值> <名> [<min>,<max>]
 * "<id>?"      -> OK K<id>=<值>
 * "<id>=<float>" -> 校验范围并写入，OK K<id>=<值>
 */
void Param_HandleCommand(const char *args);

#endif
