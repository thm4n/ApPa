/**
 * elf.c — ELF32 Loader
 *
 * Parses and loads statically-linked ELF32 i386 executables into
 * per-process address spaces and spawns them as Ring 3 user tasks.
 *
 * Loading flow:
 *   1. Read file from SimpleFS (or accept a memory buffer)
 *   2. Validate ELF header (magic, class, endianness, machine, type)
 *   3. Create a per-process page directory (clone kernel mappings)
 *   4. Walk PT_LOAD program headers:
 *      - Allocate physical pages for the segment
 *      - Copy file data (identity-mapped PMM pages allow direct memcpy)
 *      - Map pages at segment's virtual address in the new directory
 *   5. Allocate and map a user stack at USER_STACK_VIRT
 *   6. Create a Ring 3 task with entry point = e_entry
 *   7. Free the temporary file buffer
 */

#include "elf.h"
#include "../mem/paging.h"
#include "../mem/pmm.h"
#include "../mem/kmalloc.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../sys/klog.h"
#include "../../klibc/string.h"
#include "../../klibc/stdio.h"
#include "../../fs/simplefs.h"

/* ─── Internal helpers ──────────────────────────────────────────────────── */

/**
 * elf_validate — Check whether a buffer holds a valid ELF32 i386 executable
 */
int elf_validate(const void *buf, uint32_t size) {
    if (!buf || size < sizeof(elf32_ehdr_t)) {
        return -1;
    }

    const elf32_ehdr_t *ehdr = (const elf32_ehdr_t *)buf;

    /* Check magic: 0x7f 'E' 'L' 'F' */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return -1;
    }

    /* Must be 32-bit, little-endian */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) return -1;
    if (ehdr->e_ident[EI_DATA]  != ELFDATA2LSB) return -1;

    /* Must be a static executable for i386 */
    if (ehdr->e_type    != ET_EXEC)  return -1;
    if (ehdr->e_machine != EM_386)   return -1;

    /* Program header table must be present and sane */
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) return -1;
    if (ehdr->e_phentsize < sizeof(elf32_phdr_t))  return -1;

    /* Program header table must fit within the file */
    uint32_t ph_end = ehdr->e_phoff + (uint32_t)ehdr->e_phnum * ehdr->e_phentsize;
    if (ph_end > size) return -1;

    /* Entry point must not be zero */
    if (ehdr->e_entry == 0) return -1;

    return 0;
}

/**
 * load_segments — Map PT_LOAD segments into a page directory
 *
 * For each PT_LOAD segment:
 *   - Allocates physical pages covering [p_vaddr .. p_vaddr + p_memsz)
 *   - Copies p_filesz bytes from the file buffer into the pages
 *   - Zeroes the BSS area (p_memsz - p_filesz)
 *   - Maps pages in @dir with PAGE_USER | PAGE_PRESENT [| PAGE_WRITABLE]
 *
 * Returns: 0 on success, -1 on failure (caller must clean up)
 */
static int load_segments(const void *buf, uint32_t size,
                         const elf32_ehdr_t *ehdr,
                         page_directory_t *dir) {
    const uint8_t *file = (const uint8_t *)buf;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf32_phdr_t *phdr = (const elf32_phdr_t *)
            (file + ehdr->e_phoff + i * ehdr->e_phentsize);

        /* Skip non-loadable segments */
        if (phdr->p_type != PT_LOAD) continue;

        /* Sanity checks */
        if (phdr->p_memsz == 0) continue;
        if (phdr->p_filesz > phdr->p_memsz) return -1;
        if (phdr->p_offset + phdr->p_filesz > size) return -1;

        /* Virtual address range must not overlap kernel (0 - 16MB) */
        if (phdr->p_vaddr < (KERNEL_PDE_COUNT * 4 * 1024 * 1024)) {
            klog_error("ELF: segment vaddr 0x%x overlaps kernel", phdr->p_vaddr);
            return -1;
        }

        /* Determine page flags */
        uint32_t flags = PAGE_PRESENT | PAGE_USER;
        if (phdr->p_flags & PF_W) {
            flags |= PAGE_WRITABLE;
        }

        /* Walk pages in [p_vaddr .. p_vaddr + p_memsz) */
        uint32_t seg_start = phdr->p_vaddr & PAGE_FRAME_MASK;
        uint32_t seg_end   = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1)
                             & PAGE_FRAME_MASK;

        for (uint32_t vpage = seg_start; vpage < seg_end; vpage += PAGE_SIZE) {
            uint32_t phys = alloc_page();
            if (!phys) {
                klog_error("ELF: out of physical memory mapping segment %d", i);
                return -1;
            }

            /* Zero the entire page first (covers BSS and alignment gaps) */
            memset((void *)phys, 0, PAGE_SIZE);

            /* Copy file data that falls within this page */
            uint32_t page_start = vpage;                /* VA of this page */
            uint32_t page_end   = vpage + PAGE_SIZE;    /* VA of next page */

            /* Overlap between this page and the file-backed portion */
            uint32_t file_va_start = phdr->p_vaddr;
            uint32_t file_va_end   = phdr->p_vaddr + phdr->p_filesz;

            uint32_t copy_start = (file_va_start > page_start) ? file_va_start : page_start;
            uint32_t copy_end   = (file_va_end   < page_end)   ? file_va_end   : page_end;

            if (copy_start < copy_end) {
                uint32_t dst_offset  = copy_start - page_start;
                uint32_t file_offset = phdr->p_offset + (copy_start - phdr->p_vaddr);
                uint32_t copy_len    = copy_end - copy_start;

                memcpy((void *)(phys + dst_offset),
                       file + file_offset,
                       copy_len);
            }

            /* Map this page in the process's directory */
            paging_map_page_in(dir, vpage, phys, flags);
        }
    }

    return 0;
}

/* ─── Maximum argument data that fits in one stack page ─────────────────── */

#define ARGV_MAX_TOTAL  (PAGE_SIZE - 256)  /* Reserve 256 bytes for stack frames */

/**
 * setup_user_stack_args — Write argv/argc onto the user stack page
 *
 * Layout written (high address → low address):
 *
 *   USER_STACK_TOP  (0xC0000000)
 *   ┌──────────────────────────────────┐
 *   │  "arg2\0" "arg1\0" "arg0\0"     │  String data (top of page)
 *   │  NULL              (sentinel)    │  argv[argc]
 *   │  ptr → "arg2"      (argv[2])    │  argv[2]   ← virtual pointers
 *   │  ptr → "arg1"      (argv[1])    │  argv[1]
 *   │  ptr → "arg0"      (argv[0])    │  argv[0]
 *   │  argv pointer       (→argv[0])  │  [ESP+4]
 *   │  argc = 3                       │  [ESP]   ← returned ESP
 *   └──────────────────────────────────┘
 *   USER_STACK_VIRT (0xBFFFF000)
 *
 * @ustack_phys: Physical address of the zeroed user stack page
 * @argv:        NULL-terminated argument string array (may be NULL)
 * @argc:        Number of arguments (0 = no arguments)
 *
 * Returns: Virtual ESP value for the user task's iret frame.
 *          If argc == 0 or argv == NULL, returns USER_STACK_TOP (no args).
 */
static uint32_t setup_user_stack_args(uint32_t ustack_phys,
                                      const char **argv, int argc) {
    if (!argv || argc <= 0)
        return USER_STACK_TOP;

    uint8_t *page_base = (uint8_t *)ustack_phys;        /* phys page start */
    uint8_t *page_top  = page_base + PAGE_SIZE;          /* one past end    */

    /* ── Step 1: Compute total string size and bounds-check ────────── */
    uint32_t total_str = 0;
    for (int i = 0; i < argc; i++) {
        uint32_t len = 0;
        const char *s = argv[i];
        while (s[len]) len++;
        total_str += len + 1;  /* include NUL */
    }

    /* pointer table: argc pointers + 1 NULL sentinel + argv ptr + argc value */
    uint32_t ptr_space = (uint32_t)(argc + 1 + 1 + 1) * 4;
    if (total_str + ptr_space > ARGV_MAX_TOTAL) {
        klog_error("ELF: argv too large (%u + %u > %u)",
                   total_str, ptr_space, ARGV_MAX_TOTAL);
        return USER_STACK_TOP;  /* fallback: no args */
    }

    /* ── Step 2: Copy strings at the top of the physical page ──────── */
    uint8_t *str_cursor = page_top;
    uint32_t virt_addrs[64];  /* virtual addresses for each string */

    for (int i = argc - 1; i >= 0; i--) {
        uint32_t len = 0;
        const char *s = argv[i];
        while (s[len]) len++;
        len++;  /* include NUL */

        str_cursor -= len;
        memcpy(str_cursor, argv[i], len);

        /* Virtual address = USER_STACK_TOP - (page_top - str_cursor) */
        virt_addrs[i] = USER_STACK_TOP - (uint32_t)(page_top - str_cursor);
    }

    /* ── Step 3: Align down to 4 bytes ─────────────────────────────── */
    str_cursor = (uint8_t *)((uint32_t)str_cursor & ~0x3);

    /* ── Step 4: Write pointer array + argc below the strings ──────── */
    uint32_t *slot = (uint32_t *)str_cursor;

    *(--slot) = 0;  /* NULL sentinel: argv[argc] */
    for (int i = argc - 1; i >= 0; i--)
        *(--slot) = virt_addrs[i];  /* argv[i] pointer */

    /* Virtual address of argv[0] in user space */
    uint32_t argv_virt = USER_STACK_TOP -
                         (uint32_t)(page_top - (uint8_t *)(slot));

    *(--slot) = argv_virt;       /* argv pointer (for [ESP+4]) */
    *(--slot) = (uint32_t)argc;  /* argc         (for [ESP])   */

    /* ── Step 5: Compute and return the virtual ESP ────────────────── */
    uint32_t esp = USER_STACK_TOP - (uint32_t)(page_top - (uint8_t *)slot);
    return esp;
}

/**
 * elf_load_internal — Core loader that works from a memory buffer
 *
 * Validates the ELF, creates a page directory, loads segments,
 * maps the user stack, writes argv/argc, and spawns the task.
 */
static task_t* elf_load_internal(const void *buf, uint32_t size,
                                  const char *task_name,
                                  const char **argv, int argc) {
    /* ── Validate ELF header ───────────────────────────────────────── */
    if (elf_validate(buf, size) != 0) {
        klog_error("ELF: invalid header for '%s'", task_name);
        return 0;
    }

    const elf32_ehdr_t *ehdr = (const elf32_ehdr_t *)buf;

    klog_info("ELF: loading '%s' entry=0x%x phnum=%d argc=%d",
              task_name, ehdr->e_entry, ehdr->e_phnum, argc);

    /* ── Create per-process page directory ─────────────────────────── */
    uint32_t dir_phys = 0;
    page_directory_t *dir = paging_clone_directory(&dir_phys);
    if (!dir) {
        klog_error("ELF: failed to clone page directory");
        return 0;
    }

    /* ── Load PT_LOAD segments ─────────────────────────────────────── */
    if (load_segments(buf, size, ehdr, dir) != 0) {
        klog_error("ELF: failed to load segments for '%s'", task_name);
        paging_free_directory(dir);
        return 0;
    }

    /* ── Allocate & map user stack ─────────────────────────────────── */
    uint32_t ustack_phys = alloc_page();
    if (!ustack_phys) {
        klog_error("ELF: failed to allocate user stack");
        paging_free_directory(dir);
        return 0;
    }
    memset((void *)ustack_phys, 0, PAGE_SIZE);
    paging_map_page_in(dir, USER_STACK_VIRT, ustack_phys,
                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    /* ── Write argv/argc onto the user stack ───────────────────────── */
    uint32_t user_esp = setup_user_stack_args(ustack_phys, argv, argc);

    /* ── Create the user task ──────────────────────────────────────── */
    task_t *t = task_create_user_mapped(ehdr->e_entry, task_name,
                                         dir, dir_phys, user_esp);
    if (!t) {
        klog_error("ELF: failed to create task for '%s'", task_name);
        paging_free_directory(dir);
        return 0;
    }

    klog_info("ELF: spawned task '%s' (tid=%d, entry=0x%x, cr3=0x%x, esp=0x%x)",
              task_name, t->id, ehdr->e_entry, dir_phys, user_esp);

    return t;
}

/* ─── Public API ────────────────────────────────────────────────────────── */

task_t* elf_exec(const char *filename, const char *task_name,
                 const char **argv, int argc) {
    if (!filename || !task_name) return 0;

    /* Get file size */
    fs_entry_t entry;
    if (fs_stat(filename, &entry) != 0) {
        klog_error("ELF: file '%s' not found", filename);
        return 0;
    }

    if (entry.size == 0 || entry.size > ELF_MAX_FILE_SIZE) {
        klog_error("ELF: file '%s' size %u invalid (max %u)",
                   filename, entry.size, ELF_MAX_FILE_SIZE);
        return 0;
    }

    /* Allocate temporary buffer and read file */
    void *buf = kmalloc(entry.size);
    if (!buf) {
        klog_error("ELF: failed to allocate %u bytes for '%s'",
                   entry.size, filename);
        return 0;
    }

    int32_t bytes_read = fs_read_file(filename, buf, entry.size);
    if (bytes_read < 0 || (uint32_t)bytes_read < sizeof(elf32_ehdr_t)) {
        klog_error("ELF: failed to read '%s' (got %d bytes)", filename, bytes_read);
        kfree(buf);
        return 0;
    }

    /* Load and exec */
    task_t *t = elf_load_internal(buf, (uint32_t)bytes_read, task_name,
                                  argv, argc);

    kfree(buf);
    return t;
}

task_t* elf_exec_mem(const void *buf, uint32_t size, const char *task_name,
                     const char **argv, int argc) {
    if (!buf || size == 0 || !task_name) return 0;
    return elf_load_internal(buf, size, task_name, argv, argc);
}
