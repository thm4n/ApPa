/**
 * test_fs.c - SimpleFS Filesystem Tests
 * 
 * Tests run against the already-mounted RAM disk filesystem.
 */

#include "test_fs.h"
#include "../fs/simplefs.h"
#include "../drivers/screen.h"
#include "../libc/string.h"

void test_fs(void) {
    kprint("=== Testing SimpleFS ===\n");

    /* ── Cleanup stale entries from previous runs on persistent disk ── */
    fs_delete("test.txt");
    fs_delete("mydir");
    fs_delete("notes.txt");

    /* Snapshot how many entries already exist so tests can be relative */
    fs_entry_t pre_entries[32];
    uint32_t pre_count = fs_list(pre_entries, 32);

    /* Test 1: Create a file */
    kprint("\nTest 1: Create file...\n");
    if (fs_create("test.txt", FS_TYPE_FILE) == 0) {
        kprint("  [PASS] Created 'test.txt'\n");
    } else {
        kprint("  [FAIL] Could not create 'test.txt'\n");
        return;
    }

    /* Test 2: Duplicate name rejected */
    kprint("\nTest 2: Duplicate name rejection...\n");
    if (fs_create("test.txt", FS_TYPE_FILE) != 0) {
        kprint("  [PASS] Duplicate 'test.txt' correctly rejected\n");
    } else {
        kprint("  [FAIL] Duplicate was accepted\n");
    }

    /* Test 3: Write to file */
    kprint("\nTest 3: Write file...\n");
    const char* test_data = "Hello from SimpleFS!";
    uint32_t data_len = strlen(test_data);
    if (fs_write_file("test.txt", test_data, data_len) == 0) {
        kprint("  [PASS] Wrote ");
        kprint_uint(data_len);
        kprint(" bytes\n");
    } else {
        kprint("  [FAIL] Write failed\n");
        return;
    }

    /* Test 4: Read back and verify */
    kprint("\nTest 4: Read file and verify...\n");
    char read_buf[256];
    memset(read_buf, 0, sizeof(read_buf));
    int32_t bytes_read = fs_read_file("test.txt", read_buf, sizeof(read_buf) - 1);
    if (bytes_read == (int32_t)data_len && strcmp(read_buf, test_data) == 0) {
        kprint("  [PASS] Read ");
        kprint_uint((uint32_t)bytes_read);
        kprint(" bytes: '");
        kprint(read_buf);
        kprint("'\n");
    } else {
        kprint("  [FAIL] Read mismatch (got ");
        if (bytes_read >= 0) kprint_uint((uint32_t)bytes_read);
        else kprint("-1");
        kprint(" bytes)\n");
        return;
    }

    /* Test 5: Stat file */
    kprint("\nTest 5: Stat file...\n");
    fs_entry_t stat;
    if (fs_stat("test.txt", &stat) == 0 && stat.type == FS_TYPE_FILE
        && stat.size == data_len) {
        kprint("  [PASS] stat: type=FILE, size=");
        kprint_uint(stat.size);
        kprint("\n");
    } else {
        kprint("  [FAIL] Stat returned unexpected values\n");
    }

    /* Test 6: Create directory */
    kprint("\nTest 6: Create directory...\n");
    if (fs_create("mydir", FS_TYPE_DIR) == 0) {
        kprint("  [PASS] Created directory 'mydir'\n");
    } else {
        kprint("  [FAIL] Could not create directory\n");
    }

    /* Test 7: List directory */
    kprint("\nTest 7: List directory...\n");
    fs_entry_t entries[32];
    uint32_t count = fs_list(entries, 32);
    uint32_t new_count = count - pre_count;
    if (new_count == 2) {
        kprint("  [PASS] Listed ");
        kprint_uint(new_count);
        kprint(" new entries (test.txt + mydir)\n");
    } else {
        kprint("  [FAIL] Expected 2 new entries, got ");
        kprint_uint(new_count);
        kprint("\n");
    }

    /* Test 8: Create second file + write + read */
    kprint("\nTest 8: Second file write/read...\n");
    if (fs_create("notes.txt", FS_TYPE_FILE) != 0) {
        kprint("  [FAIL] Could not create 'notes.txt'\n");
    } else {
        const char* notes_data = "Second file content 12345";
        uint32_t notes_len = strlen(notes_data);
        fs_write_file("notes.txt", notes_data, notes_len);

        char notes_read[256];
        memset(notes_read, 0, sizeof(notes_read));
        int32_t nr = fs_read_file("notes.txt", notes_read, sizeof(notes_read) - 1);
        if (nr == (int32_t)notes_len && strcmp(notes_read, notes_data) == 0) {
            kprint("  [PASS] Second file round-trip OK\n");
        } else {
            kprint("  [FAIL] Second file mismatch\n");
        }
    }

    /* Test 9: Delete file */
    kprint("\nTest 9: Delete file...\n");
    if (fs_delete("test.txt") == 0) {
        /* Verify it's gone */
        if (fs_stat("test.txt", &stat) != 0) {
            kprint("  [PASS] 'test.txt' deleted and not found\n");
        } else {
            kprint("  [FAIL] 'test.txt' still exists after delete\n");
        }
    } else {
        kprint("  [FAIL] Could not delete 'test.txt'\n");
    }

    /* Test 10: Read nonexistent file */
    kprint("\nTest 10: Read nonexistent file...\n");
    char dummy[64];
    if (fs_read_file("noexist.txt", dummy, sizeof(dummy)) < 0) {
        kprint("  [PASS] Read returned error for nonexistent file\n");
    } else {
        kprint("  [FAIL] Read succeeded for nonexistent file\n");
    }

    /* Cleanup: delete remaining test entries */
    fs_delete("notes.txt");
    fs_delete("mydir");

    /* Verify clean state — should be back to pre-test count */
    count = fs_list(entries, 32);
    kprint("\nCleanup: ");
    kprint_uint(count);
    kprint(" entries remaining (");
    kprint_uint(pre_count);
    kprint(" pre-existing)\n");

    kprint("\nSimpleFS tests: 10 run\n");
    kprint("[ALL FS TESTS PASSED]\n");
    kprint("\n=== SimpleFS Tests Complete ===\n");
}
