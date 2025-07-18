/*
 * (C) Copyright 2018
* SPDX-License-Identifier:	GPL-2.0+
 * wangwei <wangwei@allwinnertech.com>
 */

#include <common.h>
#include <private_boot0.h>
#include <mmc_boot0.h>
#include <arch/clock.h>
#include <arch/uart.h>
#include <arch/dram.h>
#include <arch/gpio.h>

#define BLOCK_SIZE 512
#define DEV_NUM 0

int load_image_from_sdcard_fat(const char *filename, uintptr_t start_addr);
bool compare_arrays(char *arr1, char *arr2, int size);

static int boot0_clear_env(void);

// JDZ: used to confirm this boot program is the one that is running
// blink ACT led and count on uart for a while before start boot sequence
void blink(int nseconds) {
	unsigned *cfg = (void *)0x02000098; // PD cfg 2
	*cfg = (*cfg & ~0xf00) | 0x100;	 // config PD18 as output
	unsigned *data = (void *)0x020000a0; // PD data
	char *uart = (void *)0x02500000; // uart base
	for (int i = 0; i < nseconds; i++) {
		*uart = '0' + (i%10); // output digit
		*data ^= (1 << 18);  // toggle PD18 (blue ACT led)
		mdelay(1000);
	}
}

void main(void)
{
	sunxi_serial_init(BT0_head.prvt_head.uart_port, (void *)BT0_head.prvt_head.uart_ctrl, 6);
	mdelay(5*1000); // pause for terminal to catch up
	// blink(10);

	printf("Welcome!!!!! HELLO! BOOT0 is starting!\n");
	printf("BOOT0 commit : %s\n", BT0_head.hash);
	printf("Booting Mango Pi - loading 'MANGO.BIN' from SD...\n");

	sunxi_set_printf_debug_mode(BT0_head.prvt_head.debug_mode);

	int status = sunxi_board_init();
	if(status)
		goto _BOOT_ERROR;

	int dram_size = init_DRAM(0, (void *)BT0_head.prvt_head.dram_para);
	if(!dram_size)
		goto _BOOT_ERROR;
	else {
		printf("dram size =%d\n", dram_size);
	}
	// JDZ Aug 2024
	// this version of boot0 will attempt to load program image from sdcard
	// looks for file named mango.bin, if found will load and exec
	// otherwise exits to FEL
	const char *kernel_file = "MANGO.BIN";
	uintptr_t start_addr = 0x40000000;
	int error = load_image_from_sdcard_fat(kernel_file, start_addr);
	if (error != 0) {
		printf("unable to read file '%s' from SD card (error code %d), exiting to FEL\n", kernel_file, error);
	} else {
	printf("\n \n TESTING STARTING \n \n");
		// TEST 1: READ AND WRITE FROM BLOCK 1
			char buff_w[BLOCK_SIZE]; // Buffer with data populated for writing
			char buff_r[BLOCK_SIZE]; // Buffer with data populate for reading

			memset(buff_w, 0x00, BLOCK_SIZE); // Data to be written
			memset(buff_r, 0xff, BLOCK_SIZE); // Fill the buf with dirt to be overwritten
			int blocks_written = mmc_bwrite(DEV_NUM, 1024, 1, buff_w);
			if (blocks_written != 1) {
				printf("Test1: Error writing to the disk\n");
			}

			mdelay(100);

			//Check if you read 0x00
			int blocks_read = mmc_bread(DEV_NUM, 1024, 1, buff_r);
			if (blocks_read != 1) {
				printf("Test1: Error reading from disk\n");
			}
			// Test if the data read is correct
			if (compare_arrays(buff_w, buff_r, BLOCK_SIZE)) {
				printf("Test 1: Passed\n");
			}

		// TEST 2: WRITE DATA BIGGER THAN 1 BLOCK
			char buff_w2[BLOCK_SIZE * 2];
			char buff_r2[BLOCK_SIZE * 2];

			mdelay(50);

			memset(buff_w2, 'e', BLOCK_SIZE * 2);
			memset(buff_r2, 'm', BLOCK_SIZE * 2); // Fill it with dirt to be overwritten

			blocks_written = mmc_bwrite(DEV_NUM, 1025, 2, buff_w2);
			mdelay(50);
			if (blocks_written != 2) {
				printf("Test 2: Error writing to disk.\n");
			} else {
				blocks_read = mmc_bread(DEV_NUM, 1025, 2, buff_r2);
				if (blocks_read != 2) {
					printf("Test2: Error reading from disk.\n");
				}
				if (compare_arrays(buff_w2, buff_r2, BLOCK_SIZE * 2)) {
					printf("Test2: Passed\n");
				}
			}
	}

_BOOT_ERROR:
	boot0_clear_env();
	boot0_jmp(FEL_BASE);
}

bool compare_arrays(char *arr1, char *arr2, int size) {
	for (int i = 0; i < size; i++) {
		if (arr1[i] != arr2[i]) {
			printf("Test Failed: buff_w[%d] (%d) != buff_r[%d] (%d)\n", i, arr1[i], i, arr2[i]);
			return false;
		}
	}
	return true;
}

static int boot0_clear_env(void)
{
	sunxi_board_exit();
	sunxi_board_clock_reset();
	mmu_disable();
	mdelay(10);

	return 0;
}
