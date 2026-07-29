#ifndef TESTS_HOST_TEST_ASSERT_H
#define TESTS_HOST_TEST_ASSERT_H

/* 宿主机测试的极简断言。每个测试程序自己定义 s_failures 并在 main() 里
 * 按它决定退出码，这样一个用例失败不会中断后面的用例。
 *
 * 注意：s_failures 是 static，每个 #include 本头文件的翻译单元各有一份
 * 独立副本。目前每个测试程序只有一个 .c 文件包含它，因此不是问题；
 * 但如果将来某个测试程序拆成多个 .c 文件共链接成一个二进制，计数会被
 * 悄悄拆散到各个文件自己的副本里，main() 所在的那份 s_failures 看不到
 * 其它文件里的失败计数，可能把失败误报成 ALL PASS。届时需要改成
 * extern 声明 + 单一定义，而不是继续让每个文件各自 static 一份。 */

#include <math.h>
#include <stdio.h>

static int s_failures;

#define CHECK(cond) do {                                            \
    if (!(cond)) {                                                  \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        s_failures++;                                               \
    }                                                               \
} while (0)

#define CHECK_NEAR(actual, expected, tol) do {                      \
    if (fabsf((actual) - (expected)) > (tol)) {                     \
        printf("FAIL %s:%d  %s = %f, 期望 %f ± %f\n",               \
               __FILE__, __LINE__, #actual,                         \
               (double)(actual), (double)(expected), (double)(tol));\
        s_failures++;                                               \
    }                                                               \
} while (0)

#endif
