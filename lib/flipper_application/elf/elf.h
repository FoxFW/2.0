#ifndef _ELF_H
#define _ELF_H 1

#include <stdint.h>

typedef uint16_t Elf32_Half;
typedef uint16_t Elf64_Half;

typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;

typedef uint64_t Elf32_Xword;
typedef int64_t Elf32_Sxword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

typedef uint32_t Elf32_Addr;
typedef uint64_t Elf64_Addr;

typedef uint32_t Elf32_Off;
typedef uint64_t Elf64_Off;

typedef uint16_t Elf32_Section;
typedef uint16_t Elf64_Section;

typedef Elf32_Half Elf32_Versym;
typedef Elf64_Half Elf64_Versym;

#define EI_NIDENT (16)

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off e_phoff;
    Elf64_Off e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
} Elf64_Ehdr;

#define EI_MAG0 0
#define ELFMAG0 0x7f

#define EI_MAG1 1
#define ELFMAG1 'E'

#define EI_MAG2 2
#define ELFMAG2 'L'

#define EI_MAG3 3
#define ELFMAG3 'F'

#define ELFMAG "\177ELF"
#define SELFMAG 4

#define EI_CLASS 4
#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFCLASSNUM 3

#define EI_DATA 5
#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2
#define ELFDATANUM 3

#define EI_VERSION 6

#define EI_OSABI 7
#define ELFOSABI_NONE 0
#define ELFOSABI_SYSV 0
#define ELFOSABI_HPUX 1
#define ELFOSABI_NETBSD 2
#define ELFOSABI_LINUX 3
#define ELFOSABI_SOLARIS 6
#define ELFOSABI_AIX 7
#define ELFOSABI_IRIX 8
#define ELFOSABI_FREEBSD 9
#define ELFOSABI_TRU64 10
#define ELFOSABI_MODESTO 11
#define ELFOSABI_OPENBSD 12
#define ELFOSABI_ARM 97
#define ELFOSABI_STANDALONE 255

#define EI_ABIVERSION 8

#define EI_PAD 9

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define ET_NUM 5
#define ET_LOOS 0xfe00
#define ET_HIOS 0xfeff
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

#define EM_NONE 0
#define EM_M32 1
#define EM_SPARC 2
#define EM_386 3
#define EM_68K 4
#define EM_88K 5
#define EM_860 7
#define EM_MIPS 8
#define EM_S370 9
#define EM_MIPS_RS3_LE 10

#define EM_PARISC 15
#define EM_VPP500 17
#define EM_SPARC32PLUS 18
#define EM_960 19
#define EM_PPC 20
#define EM_PPC64 21
#define EM_S390 22

#define EM_V800 36
#define EM_FR20 37
#define EM_RH32 38
#define EM_RCE 39
#define EM_ARM 40
#define EM_FAKE_ALPHA 41
#define EM_SH 42
#define EM_SPARCV9 43
#define EM_TRICORE 44
#define EM_ARC 45
#define EM_H8_300 46
#define EM_H8_300H 47
#define EM_H8S 48
#define EM_H8_500 49
#define EM_IA_64 50
#define EM_MIPS_X 51
#define EM_COLDFIRE 52
#define EM_68HC12 53
#define EM_MMA 54
#define EM_PCP 55
#define EM_NCPU 56
#define EM_NDR1 57
#define EM_STARCORE 58
#define EM_ME16 59
#define EM_ST100 60
#define EM_TINYJ 61
#define EM_X86_64 62
#define EM_PDSP 63

#define EM_FX66 66
#define EM_ST9PLUS 67
#define EM_ST7 68
#define EM_68HC16 69
#define EM_68HC11 70
#define EM_68HC08 71
#define EM_68HC05 72
#define EM_SVX 73
#define EM_ST19 74
#define EM_VAX 75
#define EM_CRIS 76
#define EM_JAVELIN 77
#define EM_FIREPATH 78
#define EM_ZSP 79
#define EM_MMIX 80
#define EM_HUANY 81
#define EM_PRISM 82
#define EM_AVR 83
#define EM_FR30 84
#define EM_D10V 85
#define EM_D30V 86
#define EM_V850 87
#define EM_M32R 88
#define EM_MN10300 89
#define EM_MN10200 90
#define EM_PJ 91
#define EM_OPENRISC 92
#define EM_ARC_A5 93
#define EM_XTENSA 94
#define EM_NUM 95

#define EV_NONE 0
#define EV_CURRENT 1
#define EV_NUM 2

typedef struct {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
} Elf32_Shdr;

typedef struct {
    Elf64_Word sh_name;
    Elf64_Word sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word sh_link;
    Elf64_Word sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

#define SHN_UNDEF 0
#define SHN_LORESERVE 0xff00
#define SHN_LOPROC 0xff00
#define SHN_BEFORE \
    0xff00

#define SHN_AFTER \
    0xff01

#define SHN_HIPROC 0xff1f
#define SHN_LOOS 0xff20
#define SHN_HIOS 0xff3f
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2
#define SHN_XINDEX 0xffff
#define SHN_HIRESERVE 0xffff

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_INIT_ARRAY 14
#define SHT_FINI_ARRAY 15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP 17
#define SHT_SYMTAB_SHNDX 18
#define SHT_NUM 19
#define SHT_LOOS 0x60000000
#define SHT_GNU_ATTRIBUTES 0x6ffffff5
#define SHT_GNU_HASH 0x6ffffff6
#define SHT_GNU_LIBLIST 0x6ffffff7
#define SHT_CHECKSUM 0x6ffffff8
#define SHT_LOSUNW 0x6ffffffa
#define SHT_SUNW_move 0x6ffffffa
#define SHT_SUNW_COMDAT 0x6ffffffb
#define SHT_SUNW_syminfo 0x6ffffffc
#define SHT_GNU_verdef 0x6ffffffd
#define SHT_GNU_verneed 0x6ffffffe
#define SHT_GNU_versym 0x6fffffff
#define SHT_HISUNW 0x6fffffff
#define SHT_HIOS 0x6fffffff
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0x8fffffff

#define SHF_WRITE (1 << 0)
#define SHF_ALLOC (1 << 1)
#define SHF_EXECINSTR (1 << 2)
#define SHF_MERGE (1 << 4)
#define SHF_STRINGS (1 << 5)
#define SHF_INFO_LINK (1 << 6)
#define SHF_LINK_ORDER (1 << 7)
#define SHF_OS_NONCONFORMING \
    (1 << 8)

#define SHF_GROUP (1 << 9)
#define SHF_TLS (1 << 10)
#define SHF_MASKOS 0x0ff00000
#define SHF_MASKPROC 0xf0000000
#define SHF_ORDERED \
    (1 << 30)

#define SHF_EXCLUDE \
    (1 << 31)

#define GRP_COMDAT 0x1

typedef struct {
    Elf32_Word st_name;
    Elf32_Addr st_value;
    Elf32_Word st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Section st_shndx;
} Elf32_Sym;

typedef struct {
    Elf64_Word st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Section st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

typedef struct {
    Elf32_Half si_boundto;
    Elf32_Half si_flags;
} Elf32_Syminfo;

typedef struct {
    Elf64_Half si_boundto;
    Elf64_Half si_flags;
} Elf64_Syminfo;

#define SYMINFO_BT_SELF 0xffff
#define SYMINFO_BT_PARENT 0xfffe
#define SYMINFO_BT_LOWRESERVE 0xff00

#define SYMINFO_FLG_DIRECT 0x0001
#define SYMINFO_FLG_PASSTHRU 0x0002
#define SYMINFO_FLG_COPY 0x0004
#define SYMINFO_FLG_LAZYLOAD \
    0x0008

#define SYMINFO_NONE 0
#define SYMINFO_CURRENT 1
#define SYMINFO_NUM 2

#define ELF32_ST_BIND(val) (((unsigned char)(val)) >> 4)
#define ELF32_ST_TYPE(val) ((val)&0xf)
#define ELF32_ST_INFO(bind, type) (((bind) << 4) + ((type)&0xf))

#define ELF64_ST_BIND(val) ELF32_ST_BIND(val)
#define ELF64_ST_TYPE(val) ELF32_ST_TYPE(val)
#define ELF64_ST_INFO(bind, type) ELF32_ST_INFO((bind), (type))

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_NUM 3
#define STB_LOOS 10
#define STB_GNU_UNIQUE 10
#define STB_HIOS 12
#define STB_LOPROC 13
#define STB_HIPROC 15

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_COMMON 5
#define STT_TLS 6
#define STT_NUM 7
#define STT_LOOS 10
#define STT_GNU_IFUNC 10
#define STT_HIOS 12
#define STT_LOPROC 13
#define STT_HIPROC 15

#define STN_UNDEF 0

#define ELF32_ST_VISIBILITY(o) ((o)&0x03)

#define ELF64_ST_VISIBILITY(o) ELF32_ST_VISIBILITY(o)

#define STV_DEFAULT 0
#define STV_INTERNAL 1
#define STV_HIDDEN 2
#define STV_PROTECTED 3

typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} Elf32_Rel;

typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
} Elf64_Rel;

typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
    Elf32_Sword r_addend;
} Elf32_Rela;

typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

#define ELF32_R_SYM(val) ((val) >> 8)
#define ELF32_R_TYPE(val) ((val)&0xff)
#define ELF32_R_INFO(sym, type) (((sym) << 8) + ((type)&0xff))

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i)&0xffffffff)
#define ELF64_R_INFO(sym, type) ((((Elf64_Xword)(sym)) << 32) + (type))

typedef struct {
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

typedef struct {
    Elf64_Word p_type;
    Elf64_Word p_flags;
    Elf64_Off p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} Elf64_Phdr;

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7
#define PT_NUM 8
#define PT_LOOS 0x60000000
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_GNU_STACK 0x6474e551
#define PT_GNU_RELRO 0x6474e552
#define PT_LOSUNW 0x6ffffffa
#define PT_SUNWBSS 0x6ffffffa
#define PT_SUNWSTACK 0x6ffffffb
#define PT_HISUNW 0x6fffffff
#define PT_HIOS 0x6fffffff
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff

#define PF_X (1 << 0)
#define PF_W (1 << 1)
#define PF_R (1 << 2)
#define PF_MASKOS 0x0ff00000
#define PF_MASKPROC 0xf0000000

#define NT_PRSTATUS 1
#define NT_FPREGSET 2
#define NT_PRPSINFO 3
#define NT_PRXREG 4
#define NT_TASKSTRUCT 4
#define NT_PLATFORM 5
#define NT_AUXV 6
#define NT_GWINDOWS 7
#define NT_ASRS 8
#define NT_PSTATUS 10
#define NT_PSINFO 13
#define NT_PRCRED 14
#define NT_UTSNAME 15
#define NT_LWPSTATUS 16
#define NT_LWPSINFO 17
#define NT_PRFPXREG 20
#define NT_PRXFPREG 0x46e62b7f
#define NT_PPC_VMX 0x100
#define NT_PPC_SPE 0x101
#define NT_PPC_VSX 0x102
#define NT_386_TLS 0x200
#define NT_386_IOPERM 0x201

#define NT_VERSION 1

typedef struct {
    Elf32_Sword d_tag;
    union {
        Elf32_Word d_val;
        Elf32_Addr d_ptr;
    } d_un;
} Elf32_Dyn;

typedef struct {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr d_ptr;
    } d_un;
} Elf64_Dyn;

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_PLTGOT 3
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_SONAME 14
#define DT_RPATH 15
#define DT_SYMBOLIC 16
#define DT_REL 17
#define DT_RELSZ 18
#define DT_RELENT 19
#define DT_PLTREL 20
#define DT_DEBUG 21
#define DT_TEXTREL 22
#define DT_JMPREL 23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH 29
#define DT_FLAGS 30
#define DT_ENCODING 32
#define DT_PREINIT_ARRAY 32
#define DT_PREINIT_ARRAYSZ 33
#define DT_NUM 34
#define DT_LOOS 0x6000000d
#define DT_HIOS 0x6ffff000
#define DT_LOPROC 0x70000000
#define DT_HIPROC 0x7fffffff
#define DT_PROCNUM DT_MIPS_NUM

#define DT_VALRNGLO 0x6ffffd00
#define DT_GNU_PRELINKED 0x6ffffdf5
#define DT_GNU_CONFLICTSZ 0x6ffffdf6
#define DT_GNU_LIBLISTSZ 0x6ffffdf7
#define DT_CHECKSUM 0x6ffffdf8
#define DT_PLTPADSZ 0x6ffffdf9
#define DT_MOVEENT 0x6ffffdfa
#define DT_MOVESZ 0x6ffffdfb
#define DT_FEATURE_1 0x6ffffdfc
#define DT_POSFLAG_1 \
    0x6ffffdfd

#define DT_SYMINSZ 0x6ffffdfe
#define DT_SYMINENT 0x6ffffdff
#define DT_VALRNGHI 0x6ffffdff
#define DT_VALTAGIDX(tag) (DT_VALRNGHI - (tag))
#define DT_VALNUM 12

#define DT_ADDRRNGLO 0x6ffffe00
#define DT_GNU_HASH 0x6ffffef5
#define DT_TLSDESC_PLT 0x6ffffef6
#define DT_TLSDESC_GOT 0x6ffffef7
#define DT_GNU_CONFLICT 0x6ffffef8
#define DT_GNU_LIBLIST 0x6ffffef9
#define DT_CONFIG 0x6ffffefa
#define DT_DEPAUDIT 0x6ffffefb
#define DT_AUDIT 0x6ffffefc
#define DT_PLTPAD 0x6ffffefd
#define DT_MOVETAB 0x6ffffefe
#define DT_SYMINFO 0x6ffffeff
#define DT_ADDRRNGHI 0x6ffffeff
#define DT_ADDRTAGIDX(tag) (DT_ADDRRNGHI - (tag))
#define DT_ADDRNUM 11

#define DT_VERSYM 0x6ffffff0

#define DT_RELACOUNT 0x6ffffff9
#define DT_RELCOUNT 0x6ffffffa

#define DT_FLAGS_1 0x6ffffffb
#define DT_VERDEF \
    0x6ffffffc

#define DT_VERDEFNUM 0x6ffffffd
#define DT_VERNEED \
    0x6ffffffe

#define DT_VERNEEDNUM 0x6fffffff
#define DT_VERSIONTAGIDX(tag) (DT_VERNEEDNUM - (tag))
#define DT_VERSIONTAGNUM 16

#define DT_AUXILIARY 0x7ffffffd
#define DT_FILTER 0x7fffffff
#define DT_EXTRATAGIDX(tag) ((Elf32_Word) - ((Elf32_Sword)(tag) << 1 >> 1) - 1)
#define DT_EXTRANUM 3

#define DF_ORIGIN 0x00000001
#define DF_SYMBOLIC 0x00000002
#define DF_TEXTREL 0x00000004
#define DF_BIND_NOW 0x00000008
#define DF_STATIC_TLS 0x00000010

#define DF_1_NOW 0x00000001
#define DF_1_GLOBAL 0x00000002
#define DF_1_GROUP 0x00000004
#define DF_1_NODELETE 0x00000008
#define DF_1_LOADFLTR 0x00000010
#define DF_1_INITFIRST 0x00000020
#define DF_1_NOOPEN 0x00000040
#define DF_1_ORIGIN 0x00000080
#define DF_1_DIRECT 0x00000100
#define DF_1_TRANS 0x00000200
#define DF_1_INTERPOSE 0x00000400
#define DF_1_NODEFLIB 0x00000800
#define DF_1_NODUMP 0x00001000
#define DF_1_CONFALT 0x00002000
#define DF_1_ENDFILTEE 0x00004000
#define DF_1_DISPRELDNE 0x00008000
#define DF_1_DISPRELPND 0x00010000

#define DTF_1_PARINIT 0x00000001
#define DTF_1_CONFEXP 0x00000002

#define DF_P1_LAZYLOAD 0x00000001
#define DF_P1_GROUPPERM \
    0x00000002

typedef struct {
    Elf32_Half vd_version;
    Elf32_Half vd_flags;
    Elf32_Half vd_ndx;
    Elf32_Half vd_cnt;
    Elf32_Word vd_hash;
    Elf32_Word vd_aux;
    Elf32_Word vd_next;

} Elf32_Verdef;

typedef struct {
    Elf64_Half vd_version;
    Elf64_Half vd_flags;
    Elf64_Half vd_ndx;
    Elf64_Half vd_cnt;
    Elf64_Word vd_hash;
    Elf64_Word vd_aux;
    Elf64_Word vd_next;

} Elf64_Verdef;

#define VER_DEF_NONE 0
#define VER_DEF_CURRENT 1
#define VER_DEF_NUM 2

#define VER_FLG_BASE 0x1
#define VER_FLG_WEAK 0x2

#define VER_NDX_LOCAL 0
#define VER_NDX_GLOBAL 1
#define VER_NDX_LORESERVE 0xff00
#define VER_NDX_ELIMINATE 0xff01

typedef struct {
    Elf32_Word vda_name;
    Elf32_Word vda_next;

} Elf32_Verdaux;

typedef struct {
    Elf64_Word vda_name;
    Elf64_Word vda_next;

} Elf64_Verdaux;

typedef struct {
    Elf32_Half vn_version;
    Elf32_Half vn_cnt;
    Elf32_Word vn_file;

    Elf32_Word vn_aux;
    Elf32_Word vn_next;

} Elf32_Verneed;

typedef struct {
    Elf64_Half vn_version;
    Elf64_Half vn_cnt;
    Elf64_Word vn_file;

    Elf64_Word vn_aux;
    Elf64_Word vn_next;

} Elf64_Verneed;

#define VER_NEED_NONE 0
#define VER_NEED_CURRENT 1
#define VER_NEED_NUM 2

typedef struct {
    Elf32_Word vna_hash;
    Elf32_Half vna_flags;
    Elf32_Half vna_other;
    Elf32_Word vna_name;
    Elf32_Word vna_next;

} Elf32_Vernaux;

typedef struct {
    Elf64_Word vna_hash;
    Elf64_Half vna_flags;
    Elf64_Half vna_other;
    Elf64_Word vna_name;
    Elf64_Word vna_next;

} Elf64_Vernaux;

#define VER_FLG_WEAK 0x2

typedef struct {
    uint32_t a_type;
    union {
        uint32_t a_val;

    } a_un;
} Elf32_auxv_t;

typedef struct {
    uint64_t a_type;
    union {
        uint64_t a_val;

    } a_un;
} Elf64_auxv_t;

#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_NOTELF 10
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_CLKTCK 17

#define AT_PLATFORM 15
#define AT_HWCAP \
    16

#define AT_FPUCW 18

#define AT_DCACHEBSIZE 19
#define AT_ICACHEBSIZE 20
#define AT_UCACHEBSIZE 21

#define AT_IGNOREPPC 22

#define AT_SECURE 23

#define AT_BASE_PLATFORM 24

#define AT_RANDOM 25

#define AT_EXECFN 31

#define AT_SYSINFO 32
#define AT_SYSINFO_EHDR 33

#define AT_L1I_CACHESHAPE 34
#define AT_L1D_CACHESHAPE 35
#define AT_L2_CACHESHAPE 36
#define AT_L3_CACHESHAPE 37

typedef struct {
    Elf32_Word n_namesz;
    Elf32_Word n_descsz;
    Elf32_Word n_type;
} Elf32_Nhdr;

typedef struct {
    Elf64_Word n_namesz;
    Elf64_Word n_descsz;
    Elf64_Word n_type;
} Elf64_Nhdr;

#define ELF_NOTE_SOLARIS "SUNW Solaris"

#define ELF_NOTE_GNU "GNU"

#define ELF_NOTE_PAGESIZE_HINT 1

#define NT_GNU_ABI_TAG 1
#define ELF_NOTE_ABI NT_GNU_ABI_TAG

#define ELF_NOTE_OS_LINUX 0
#define ELF_NOTE_OS_GNU 1
#define ELF_NOTE_OS_SOLARIS2 2
#define ELF_NOTE_OS_FREEBSD 3

#define NT_GNU_HWCAP 2

#define NT_GNU_BUILD_ID 3

#define NT_GNU_GOLD_VERSION 4

typedef struct {
    Elf32_Xword m_value;
    Elf32_Word m_info;
    Elf32_Word m_poffset;
    Elf32_Half m_repeat;
    Elf32_Half m_stride;
} Elf32_Move;

typedef struct {
    Elf64_Xword m_value;
    Elf64_Xword m_info;
    Elf64_Xword m_poffset;
    Elf64_Half m_repeat;
    Elf64_Half m_stride;
} Elf64_Move;

#define ELF32_M_SYM(info) ((info) >> 8)
#define ELF32_M_SIZE(info) ((unsigned char)(info))
#define ELF32_M_INFO(sym, size) (((sym) << 8) + (unsigned char)(size))

#define ELF64_M_SYM(info) ELF32_M_SYM(info)
#define ELF64_M_SIZE(info) ELF32_M_SIZE(info)
#define ELF64_M_INFO(sym, size) ELF32_M_INFO(sym, size)

#define EF_ARM_RELEXEC 0x01
#define EF_ARM_HASENTRY 0x02
#define EF_ARM_INTERWORK 0x04
#define EF_ARM_APCS_26 0x08
#define EF_ARM_APCS_FLOAT 0x10
#define EF_ARM_PIC 0x20
#define EF_ARM_ALIGN8 0x40
#define EF_ARM_NEW_ABI 0x80
#define EF_ARM_OLD_ABI 0x100
#define EF_ARM_SOFT_FLOAT 0x200
#define EF_ARM_VFP_FLOAT 0x400
#define EF_ARM_MAVERICK_FLOAT 0x800

#define EF_ARM_SYMSARESORTED 0x04
#define EF_ARM_DYNSYMSUSESEGIDX 0x08
#define EF_ARM_MAPSYMSFIRST 0x10
#define EF_ARM_EABIMASK 0XFF000000

#define EF_ARM_BE8 0x00800000
#define EF_ARM_LE8 0x00400000

#define EF_ARM_EABI_VERSION(flags) ((flags)&EF_ARM_EABIMASK)
#define EF_ARM_EABI_UNKNOWN 0x00000000
#define EF_ARM_EABI_VER1 0x01000000
#define EF_ARM_EABI_VER2 0x02000000
#define EF_ARM_EABI_VER3 0x03000000
#define EF_ARM_EABI_VER4 0x04000000
#define EF_ARM_EABI_VER5 0x05000000

#define STT_ARM_TFUNC STT_LOPROC
#define STT_ARM_16BIT STT_HIPROC

#define SHF_ARM_ENTRYSECT 0x10000000
#define SHF_ARM_COMDEF \
    0x80000000

#define PF_ARM_SB \
    0x10000000

#define PF_ARM_PI 0x20000000
#define PF_ARM_ABS 0x40000000

#define PT_ARM_EXIDX (PT_LOPROC + 1)

#define SHT_ARM_EXIDX (SHT_LOPROC + 1)
#define SHT_ARM_PREEMPTMAP (SHT_LOPROC + 2)
#define SHT_ARM_ATTRIBUTES (SHT_LOPROC + 3)

#define R_ARM_NONE 0
#define R_ARM_PC24 1
#define R_ARM_ABS32 2
#define R_ARM_REL32 3
#define R_ARM_PC13 4
#define R_ARM_ABS16 5
#define R_ARM_ABS12 6
#define R_ARM_THM_ABS5 7
#define R_ARM_ABS8 8
#define R_ARM_SBREL32 9
#define R_ARM_THM_PC22 10
#define R_ARM_THM_PC8 11
#define R_ARM_AMP_VCALL9 12
#define R_ARM_SWI24 13
#define R_ARM_THM_SWI8 14
#define R_ARM_XPC25 15
#define R_ARM_THM_XPC22 16
#define R_ARM_TLS_DTPMOD32 17
#define R_ARM_TLS_DTPOFF32 18
#define R_ARM_TLS_TPOFF32 19
#define R_ARM_COPY 20
#define R_ARM_GLOB_DAT 21
#define R_ARM_JUMP_SLOT 22
#define R_ARM_RELATIVE 23
#define R_ARM_GOTOFF 24
#define R_ARM_GOTPC 25
#define R_ARM_GOT32 26
#define R_ARM_PLT32 27
#define R_ARM_THM_JUMP24 30
#define R_ARM_ALU_PCREL_7_0 32
#define R_ARM_ALU_PCREL_15_8 33
#define R_ARM_ALU_PCREL_23_15 34
#define R_ARM_LDR_SBREL_11_0 35
#define R_ARM_ALU_SBREL_19_12 36
#define R_ARM_ALU_SBREL_27_20 37
#define R_ARM_THM_MOVW_ABS_NC 47
#define R_ARM_THM_MOVT_ABS 48
#define R_ARM_GNU_VTENTRY 100
#define R_ARM_GNU_VTINHERIT 101
#define R_ARM_THM_PC11 102
#define R_ARM_THM_PC9 103
#define R_ARM_TLS_GD32 \
    104

#define R_ARM_TLS_LDM32 \
    105

#define R_ARM_TLS_LDO32 \
    106

#define R_ARM_TLS_IE32 \
    107

#define R_ARM_TLS_LE32 \
    108

#define R_ARM_RXPC25 249
#define R_ARM_RSBREL32 250
#define R_ARM_THM_RPC22 251
#define R_ARM_RREL32 252
#define R_ARM_RABS22 253
#define R_ARM_RPC24 254
#define R_ARM_RBASE 255

#define R_ARM_NUM 256

#endif
