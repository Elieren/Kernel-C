#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fs/fat16/fs.h"

START_TEST(test_fat16_memcpy_bounds_invariant)
{
    // Invariant: memcpy operations must never write beyond destination buffer boundaries
    const struct {
        void *src;
        void *dst;
        size_t size;
        size_t dst_size;
    } payloads[] = {
        // Exact exploit case: source size equals destination size but pointers misaligned
        {(void*)0x41414141, (void*)0x42424242, sizeof(fat16_table_t), sizeof(fat16_table_t) - 1},
        // Boundary case: size equals destination buffer size exactly
        {(void*)0x43434343, (void*)0x44444444, 512, 512},
        // Valid input: size smaller than destination buffer
        {(void*)0x45454545, (void*)0x46464646, 256, 512},
        // Attack payload: size larger than destination buffer
        {(void*)0x47474747, (void*)0x48484848, 1024, 512},
        // Zero size edge case
        {(void*)0x49494949, (void*)0x50505050, 0, 512}
    };
    
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);
    
    for (int i = 0; i < num_payloads; i++) {
        // The invariant: destination buffer must accommodate the copy size
        ck_assert_msg(payloads[i].size <= payloads[i].dst_size,
                     "memcpy size %zu exceeds destination buffer size %zu",
                     payloads[i].size, payloads[i].dst_size);
        
        // Additional safety check: pointers must be valid for non-zero copies
        if (payloads[i].size > 0) {
            ck_assert_ptr_ne(payloads[i].src, NULL);
            ck_assert_ptr_ne(payloads[i].dst, NULL);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_fat16_memcpy_bounds_invariant);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}