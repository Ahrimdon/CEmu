#ifdef DEBUG_SUPPORT

#ifndef DEBUG_H
#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../defines.h"
#include "../port.h"

extern volatile bool inDebugger;

eZ80portrange_t init_debugger_ports(void);

/* For use in the debugger */
enum {
    DBG_USER,
    DBG_STEP,
    HIT_EXEC_BREAKPOINT,
    HIT_READ_WATCHPOINT,
    HIT_WRITE_WATCHPOINT,
    HIT_RUN_BREAKPOINT,
    HIT_PORT_WRITE_WATCHPOINT,
    HIT_PORT_READ_WATCHPOINT,
    DBG_NMI_TRIGGERED,
    DBG_WATCHDOG_TIMEOUT,
    DBG_MISC_RESET,
    NUM_DBG_COMMANDS,
};

/* For Port Monitoring */
#define DBG_NO_HANDLE             0
#define DBG_PORT_READ             1
#define DBG_PORT_WRITE            2
#define DBG_PORT_FREEZE           4

/* For Memory Brakpoints */
#define DBG_READ_WATCHPOINT       (1 << 0)
#define DBG_WRITE_WATCHPOINT      (1 << 1)
#define DBG_EXEC_BREAKPOINT       (1 << 2)
#define DBG_TEMP_EXEC_BREAKPOINT  (1 << 3)

#define DBG_RW_WATCHPOINT         ((DBG_WRITE_WATCHPOINT) | (DBG_READ_WATCHPOINT))

/* For other things */
#define DBG_INST_START_MARKER     (1 << 4)
#define DBG_INST_MARKER           (1 << 5)

#define DBG_PORT_RANGE            0xFFFF00
#define DBGOUT_PORT_RANGE         0xFB0000
#define DBGERR_PORT_RANGE         0xFC0000
#define SIZEOF_DBG_BUFFER         0x1000

typedef struct {
    uint8_t *block;
    uint8_t *ports;
} debug_data_t;

typedef struct {        /* For debugging */
    uint32_t cpuCycles, cpuNext;
    uint64_t cpuBaseCycles, cpuHaltCycles, cpuDmaCycles;
    char *buffer;
    char *bufferErr;
    bool resetOpensDebugger;
    uint32_t stepOverInstrSize;
    uint8_t stepOverMode;
    uint32_t stepOutSPL;
    uint32_t stoAddress;
    uint16_t stepOutSPS;
    uint32_t stepOverInstrEnd;
    uint32_t runUntilAddress;
    int8_t stepOutWait;
    bool stepOverFirstStep;
    bool ignore;
    debug_data_t data;
    volatile uint32_t bufferPos;
    volatile uint32_t bufferErrPos;
    bool ignoreDmaCycles;
    int64_t cycleCount;
    bool commands;
} debug_state_t;

/* Debugging */
extern debug_state_t debugger;

void debugger_init(void);
void debugger_free(void);

/* Main interface functions */
void open_debugger(int reason, uint32_t address);
void close_debugger(void);

uint8_t debug_peek_byte(uint32_t address);
void debug_switch_step_mode(void);

void debug_init_run_until(uint32_t address);

void debug_breakwatch(uint32_t address, unsigned int type, bool set);
void debug_breakpoint_remove(uint32_t address);

void debug_pmonitor_set(uint16_t address, unsigned int type, bool set);
void debug_pmonitor_remove(uint16_t address);

void debug_set_pc_address(uint32_t address);

void debug_clear_temp_break(void);

#ifdef __cplusplus
}
#endif

#endif

#endif
