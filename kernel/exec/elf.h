/**
 * elf.h — ELF32 format definitions and loader API
 *
 * Provides the type definitions for parsing 32-bit ELF headers and
 * program headers, plus the public API for loading and executing
 * ELF binaries from the filesystem or from a memory buffer.
 *
 * Reference: Tool Interface Standard (TIS) ELF Specification v1.2
 */

#ifndef ELF_H
#define ELF_H

#include "../../klibc/stdint.h"
#include "../task/task.h"

/* ─── ELF Identification (e_ident) ──────────────────────────────────────── */

#define EI_NIDENT       16      /* Size of e_ident[] array               */

/* Byte indices within e_ident[] */
#define EI_MAG0         0       /* 0x7f                                  */
#define EI_MAG1         1       /* 'E'                                   */
#define EI_MAG2         2       /* 'L'                                   */
#define EI_MAG3         3       /* 'F'                                   */
#define EI_CLASS        4       /* File class (32/64-bit)                */
#define EI_DATA         5       /* Data encoding (endianness)            */
#define EI_VERSION      6       /* ELF version                           */

/* Magic bytes */
#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

/* EI_CLASS values */
#define ELFCLASSNONE    0       /* Invalid                               */
#define ELFCLASS32      1       /* 32-bit objects                        */
#define ELFCLASS64      2       /* 64-bit objects                        */

/* EI_DATA values */
#define ELFDATA2LSB     1       /* Little-endian                         */
#define ELFDATA2MSB     2       /* Big-endian                            */

/* ─── ELF Header Types (e_type) ─────────────────────────────────────────── */

#define ET_NONE         0       /* No file type                          */
#define ET_REL          1       /* Relocatable file                      */
#define ET_EXEC         2       /* Executable file                       */
#define ET_DYN          3       /* Shared object file                    */
#define ET_CORE         4       /* Core file                             */

/* ─── Machine Types (e_machine) ─────────────────────────────────────────── */

#define EM_386          3       /* Intel 80386                           */

/* ─── ELF Version ───────────────────────────────────────────────────────── */

#define EV_CURRENT      1       /* Current version                       */

/* ─── Program Header Types (p_type) ─────────────────────────────────────── */

#define PT_NULL         0       /* Unused entry                          */
#define PT_LOAD         1       /* Loadable segment                      */
#define PT_DYNAMIC      2       /* Dynamic linking info                  */
#define PT_INTERP       3       /* Path to interpreter                   */
#define PT_NOTE         4       /* Auxiliary information                  */
#define PT_PHDR         6       /* Program header table itself           */

/* ─── Program Header Flags (p_flags) ────────────────────────────────────── */

#define PF_X            0x1     /* Execute                               */
#define PF_W            0x2     /* Write                                 */
#define PF_R            0x4     /* Read                                  */

/* ─── ELF32 Header ──────────────────────────────────────────────────────── */

typedef struct {
    uint8_t     e_ident[EI_NIDENT]; /* Magic number and other info       */
    uint16_t    e_type;             /* Object file type                  */
    uint16_t    e_machine;          /* Architecture                      */
    uint32_t    e_version;          /* Object file version               */
    uint32_t    e_entry;            /* Entry point virtual address       */
    uint32_t    e_phoff;            /* Program header table file offset  */
    uint32_t    e_shoff;            /* Section header table file offset  */
    uint32_t    e_flags;            /* Processor-specific flags          */
    uint16_t    e_ehsize;           /* ELF header size in bytes          */
    uint16_t    e_phentsize;        /* Program header table entry size   */
    uint16_t    e_phnum;            /* Program header table entry count  */
    uint16_t    e_shentsize;        /* Section header table entry size   */
    uint16_t    e_shnum;            /* Section header table entry count  */
    uint16_t    e_shstrndx;         /* Section header string table index */
} __attribute__((packed)) elf32_ehdr_t;

/* ─── ELF32 Program Header ──────────────────────────────────────────────── */

typedef struct {
    uint32_t    p_type;             /* Segment type                      */
    uint32_t    p_offset;           /* Segment file offset               */
    uint32_t    p_vaddr;            /* Segment virtual address           */
    uint32_t    p_paddr;            /* Segment physical address (unused) */
    uint32_t    p_filesz;           /* Segment size in file              */
    uint32_t    p_memsz;            /* Segment size in memory            */
    uint32_t    p_flags;            /* Segment flags (PF_R|PF_W|PF_X)   */
    uint32_t    p_align;            /* Segment alignment                 */
} __attribute__((packed)) elf32_phdr_t;

/* ─── Default User Virtual Address ──────────────────────────────────────── */

/** Traditional i386 base address for user executables */
#define ELF_USER_BASE   0x08048000

/* ─── Maximum supported PT_LOAD segments ────────────────────────────────── */

#define ELF_MAX_SEGMENTS    8

/* ─── Maximum ELF file size we'll load (256 KB) ─────────────────────────── */

#define ELF_MAX_FILE_SIZE   (256 * 1024)

/* ─── API ───────────────────────────────────────────────────────────────── */

/**
 * elf_exec — Load an ELF binary from SimpleFS and spawn a user process
 * @filename:  SimpleFS file name (max 23 chars)
 * @task_name: Human-readable name for the task
 *
 * Reads the file, validates the ELF header, maps PT_LOAD segments into
 * a new per-process address space, sets up a user stack, and creates
 * a Ring 3 task starting at the ELF entry point.
 *
 * Returns: Pointer to the new task, or NULL on failure.
 */
task_t* elf_exec(const char *filename, const char *task_name);

/**
 * elf_exec_mem — Load an ELF binary from a memory buffer
 * @buf:       Pointer to the ELF file contents
 * @size:      Size of the buffer in bytes
 * @task_name: Human-readable name for the task
 *
 * Same as elf_exec() but reads from an in-memory buffer instead of
 * the filesystem.  Useful for embedded test binaries.
 *
 * Returns: Pointer to the new task, or NULL on failure.
 */
task_t* elf_exec_mem(const void *buf, uint32_t size, const char *task_name);

/**
 * elf_validate — Check whether a buffer contains a valid ELF32 i386 executable
 * @buf:  Pointer to the ELF file contents
 * @size: Size of the buffer in bytes
 *
 * Returns: 0 on success, -1 if the header is invalid.
 */
int elf_validate(const void *buf, uint32_t size);

#endif /* ELF_H */
