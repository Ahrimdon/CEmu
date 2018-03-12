#include "link.h"
#include "asic.h"
#include "cpu.h"
#include "mem.h"
#include "control.h"
#include "interrupt.h"
#include "emu.h"
#include "os/os.h"

#include <stdio.h>
#include <string.h>

#define SAFE_RAM 0xD052C6

static const uint8_t jforcegraph[9] = {
    0xF3,                         /* di                            */
    0xFD, 0xCB, 0x03, 0x86,       /* res graphdraw,(iy+graphflags) */
    0xC3, 0x7C, 0x14, 0x02        /* jp _jforcegraphnokey          */
};

static const uint8_t jforcehome[7] = {
    0xF3,                         /* di            */
    0x3E, 0x09,                   /* ld a,kclear   */
    0xC3, 0x64, 0x01, 0x02        /* jp _jforcecmd */
};

static const uint8_t archivevar[15] = {
    0xF3,                         /* di               */
    0xCD, 0xC8, 0x02, 0x02,       /* call _op4toop1   */
    0xCD, 0x0C, 0x05, 0x02,       /* call _chkfindsym */
    0xCD, 0x4C, 0x14, 0x02,       /* call _archivevar */
    0x18, 0xFE                    /* _sink: jr _sink  */
};

static const uint8_t header_data[10] = {
    0x2A, 0x2A, 0x54, 0x49, 0x38, 0x33, 0x46, 0x2A, 0x1A, 0x0A
};

static const uint8_t pgrm_loader[34] = {
    0xF3,                         /* di                   */
    0xE5,                         /* push hl              */
    0xCD, 0x0C, 0x05, 0x02,       /* call _chkfindsym     */
    0xD4, 0x34, 0x14, 0x02,       /* call nc,_delvararc   */
    0xE1,                         /* pop hl               */
    0x3A, 0xF8, 0x05, 0xD0,       /* ld a,(OP1)           */
    0xCD, 0x38, 0x13, 0x02,       /* call _createvar      */
    0xED, 0x53,
    (SAFE_RAM>>0)&255,
    (SAFE_RAM>>8)&255,
    (SAFE_RAM>>16)&255,           /* ld (SAFE_RAM),de     */
    0x18, 0xFE                    /* _sink: jr _sink      */
};

bool listVariablesLink(void) {
    calc_var_t var;
    vat_search_init(&var);
    puts("VAT:");
    while (vat_search_next(&var)) {
        printf("type: %s, name: %s, size: %hu\n",
               calc_var_type_names[var.type],
               calc_var_name_to_utf8(var.name),
               var.size);
    }
    return true;
}

static void run_asm(const uint8_t *data, const size_t data_size, const uint32_t cycles) {
    cpu.halted = cpu.IEF_wait = cpu.IEF1 = cpu.IEF2 = cpu.NMI = 0;
    memcpy(phys_mem_ptr(SAFE_RAM, 8400), data, data_size);
    cpu.cycles = 0;
    cpu.next = sched.event.cycle = cycles;
    cpu_flush(SAFE_RAM, 1);
    cpu_execute();
}

/*
 * Really hackish way to send a variable -- Like, on a scale of 1 to hackish, it's like really hackish
 * Proper USB emulation should really be a thing
 * See GitHub issue #25
 */
int EMSCRIPTEN_KEEPALIVE sendVariableLink(const char *file_name, unsigned int location) {
    const size_t h_size = sizeof header_data;
    const uint8_t tVarLst = 0x5D, tAns = 0x72;
    unsigned int i;
    int ret = LINK_GOOD;

    FILE *file;
    uint8_t tmp_buf[0x80];

    uint32_t save_cycles;

    uint8_t var_ver,
            var_arc;

    uint8_t *cxCurApp     = phys_mem_ptr(0xD007E0, 1),
            *op1          = phys_mem_ptr(0xD005F8, 9),
            *var_ptr;

    uint16_t var_size,
             var_size2,
             data_size,
             checksum,
             cchecksum,
             header_size;

    size_t   temp_size;
    long     lSize;

    /* Return if we are at an error menu */
    if (*cxCurApp == 0x52 || !(file = fopen_utf8(file_name, "rb"))) {
        gui_console_printf("[CEmu] Transfer Error: OS in error screen.\n");
        return LINK_ERR;
    }

    save_cycles = cpu.cycles;

    if (fread(tmp_buf, 1, h_size, file) != h_size)         goto r_err;
    if (memcmp(tmp_buf, header_data, h_size))              goto r_err;

    if (fseek(file, FILE_DATA, SEEK_SET))                  goto r_err;
    if (fread(&data_size, 2, 1, file) != 1)                goto r_err;


    if (fseek(file, 0L, SEEK_END))                         goto r_err;
    if ((lSize = ftell(file)) <= 0)                        goto r_err;

    temp_size = (size_t)data_size + FILE_DATA + 4;

    if ((size_t)lSize != temp_size) {
        gui_console_printf("[CEmu] Transfer Warning: File data section size incorrect.\n");
        ret = LINK_WARN;
    }

    if (fseek(file, FILE_DATA_START, SEEK_SET))            goto r_err;

    /* make sure the checksum is correct */
    checksum = 0;
    for (i = 0x37; i<(unsigned int)lSize-2; i++) {
        checksum = (checksum + fgetc(file)) & 0xffff;
    }

    if (fread(&cchecksum, 2, 1, file) != 1)                goto r_err;

    if (cchecksum != checksum) {
        gui_console_printf("[CEmu] Transfer Warning: File checksum invalid.\n");
        ret = LINK_WARN;
    }

    if (fseek(file, FILE_DATA_START, SEEK_SET))            goto r_err;

    if (control.off) {
        intrpt_set(INT_ON, true);
        control.readBatteryStatus = ~1;
        intrpt_pulse(INT_WAKE);
        cpu.cycles = cpu.IEF_wait = 0;
        cpu.next = 100000000;
        cpu_execute();
        intrpt_set(INT_ON, false);
        goto r_err;
    }

    /* parse each varaible individually until the entire file is compelete. */

    run_asm(jforcegraph, sizeof jforcegraph, 2500000);

    while (ftell(file) < lSize-2) {

        if (fread(&header_size, 2, 1, file) != 1)          goto r_err;
        if (fread(&var_size, 2, 1, file) != 1)             goto r_err;
        if (fread(op1, 1, 9, file) != 9)                   goto r_err;
        if (header_size == 11) {
            var_ver = var_arc = 0;
        } else if (header_size == 13) {
            if (fread(&var_ver, 1, 1, file) != 1)          goto r_err;
            if (fread(&var_arc, 1, 1, file) != 1)          goto r_err;
        } else                                             goto r_err;
        if (fread(&var_size2, 2, 1, file) != 1)            goto r_err;
        if (var_size != var_size2)                         goto r_err;

        /* Hack for TI Connect CE bug */
        if ((*op1 == CALC_VAR_TYPE_REAL_LIST || *op1 == CALC_VAR_TYPE_CPLX_LIST) &&
            !(op1[1] == tVarLst || op1[1] == tAns)) {
            memmove(op1 + 2, op1 + 1, 7);
            op1[1] = tVarLst;
        }

        mem_poke_byte(0xD008DF, 0);
        cpu.registers.HL = var_size - 2;

        /* copy the program into the emulator */

        run_asm(pgrm_loader, sizeof pgrm_loader, 23000000);

        if (mem_peek_byte(0xD008DF)) {
            gui_console_printf("[CEmu] Transfer Error: OS Error encountered\n");
            ret = LINK_ERR;
            goto r_err;
        }

        if (mem_peek_word(0xD0118C, true)) {
            gui_console_printf("[CEmu] Transfer Warning: Running assembly program; RAM leak possible\n");
            ret = LINK_WARN;
        }

        var_ptr = phys_mem_ptr(mem_peek_long(SAFE_RAM), var_size);

        if (fread(var_ptr, 1, var_size, file) != var_size) goto r_err;

        switch (location) {
            case LINK_FILE:
                if (var_arc != 0x80) break;
                /* fallthrough */
            case LINK_ARCH:
                run_asm(archivevar, sizeof archivevar, 23000000);
                break;
            case LINK_RAM:
                break;
        }
    }

    run_asm(jforcehome, sizeof jforcehome, 23000000);
r_err:
    cpu.cycles = save_cycles;
    sched.event.cycle = 0;
    cpu_restore_next();
    fclose(file);
    return ret;
}

static const char header[] = "**TI83F*\x1A\x0A\0Exported via CEmu ";
bool receiveVariableLink(int count, const calc_var_t *vars, const char *file_name) {
    FILE *file;
    calc_var_t var;
    uint16_t header_size = 13, size = 0, checksum = 0;
    int byte;

    file = fopen_utf8(file_name, "w+b");
    if (!file) {
        return false;
    }
    setbuf(file, NULL);
    if (fwrite(header, sizeof header - 1, 1, file) != 1) goto w_err;
    if (fseek(file, 0x37, SEEK_SET))                     goto w_err;
    while (count--) {
        if (!vat_search_find(vars++, &var))              goto w_err;
        if (fwrite(&header_size,       2, 1, file) != 1) goto w_err;
        if (fwrite(&var.size,          2, 1, file) != 1) goto w_err;
        if (fwrite(&var.type,          1, 1, file) != 1) goto w_err;
        if (fwrite(&var.name,          8, 1, file) != 1) goto w_err;
        if (fwrite(&var.version,       1, 1, file) != 1) goto w_err;
        if (fputc(var.archived << 7, file) == EOF)       goto w_err;
        if (fwrite(&var.size,          2, 1, file) != 1) goto w_err;
        if (fwrite(var.data,    var.size, 1, file) != 1) goto w_err;
        size += 17 + var.size;
    }
    if (fseek(file, 0x35, SEEK_SET))                     goto w_err;
    if (fwrite(&size,                  2, 1, file) != 1) goto w_err;
    if (fflush(file))                                    goto w_err;
    while (size--) {
        if ((byte = fgetc(file)) == EOF)                 goto w_err;
        checksum += byte;
    }
    if (fwrite(&checksum,              2, 1, file) != 1) goto w_err;

    return !fclose(file);

w_err:
    fclose(file);
    if(remove(file_name)) {
        gui_console_printf("[CEmu] Transfer Error: Please contact the developers\n");
    }
    return false;
}
