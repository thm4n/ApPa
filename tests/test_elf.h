#ifndef TEST_ELF_H
#define TEST_ELF_H

/**
 * test_elf - Run ELF loader tests
 *
 * Tests ELF validation, loading from memory, loading from SimpleFS,
 * and resource cleanup.  Must be called after scheduler, filesystem,
 * and paging are initialized.
 */
void test_elf(void);

#endif /* TEST_ELF_H */
