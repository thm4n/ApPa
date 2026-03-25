#ifndef TESTS_H
#define TESTS_H

// Include all test headers
#include "test_varargs.h"
#include "test_printf.h"
#include "test_scroll_log.h"
#include "test_pmm.h"
#include "test_paging.h"
#include "test_ata.h"
#include "test_fs.h"
#include "test_multitask.h"
#include "test_userspace.h"
#include "test_addrspace.h"

// Master function to run all tests
void run_all_tests();

#endif
