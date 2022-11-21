// SPDX-License-Identifier: BSD-2-Clause
/*
 * openpiton copyright (c) 2020 Western Digital Corporation or its affiliates.
 * Rewrite for emufun is (C) 2022 Charles Lohr.
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/uart8250.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define EMUFUN_DEFAULT_UART_ADDR		0x10000000
#define EMUFUN_DEFAULT_UART_FREQ		1000000
#define EMUFUN_DEFAULT_UART_BAUDRATE    115200
#define EMUFUN_DEFAULT_UART_REG_SHIFT	0
#define EMUFUN_DEFAULT_UART_REG_WIDTH	1
#define EMUFUN_DEFAULT_UART_REG_OFFSET	0
#define EMUFUN_DEFAULT_HART_COUNT       1

/*
 * emufun platform early initialization.
 */
static int emufun_early_init(bool cold_boot)
{
	int * k = (int*)0xffffffff;
	*k = 0xaaaaaaaa;
	return 0;
}


/*
 * emufun platform final initialization.
 */
static int emufun_final_init(bool cold_boot)
{
	return 0;
}

/*
 * Initialize the emufun console.
 */
static int emufun_console_init(void)
{
	return uart8250_init( EMUFUN_DEFAULT_UART_ADDR, EMUFUN_DEFAULT_UART_FREQ, EMUFUN_DEFAULT_UART_BAUDRATE, 0, 1, 0);
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.early_init = emufun_early_init,
	.final_init = emufun_final_init,
	.console_init = emufun_console_init,
	.irqchip_init = 0,
	.ipi_init = 0,
	.timer_init = 0,
};

const struct sbi_platform platform = {
	.opensbi_version = OPENSBI_VERSION,
	.platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name = "emufun RISC-V",
	.features = SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count = EMUFUN_DEFAULT_HART_COUNT,
	.hart_stack_size = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr = (unsigned long)&platform_ops
};
