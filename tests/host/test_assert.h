#ifndef TESTS_HOST_TEST_ASSERT_H
#define TESTS_HOST_TEST_ASSERT_H

/* 宿主机测试的极简断言。每个测试程序自己定义 s_failures 并在 main() 里
 * 按它决定退出码，这样一个用例失败不会中断后面的用例。 */

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
