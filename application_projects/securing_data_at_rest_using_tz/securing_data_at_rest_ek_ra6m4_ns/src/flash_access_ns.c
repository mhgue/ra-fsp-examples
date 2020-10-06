/***********************************************************************************************************************
 * File Name    : flash_access_ns.c
 * Description  : routines for code flash access.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
 * SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2020 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/
#include <app_definitions.h>
#include "flash_access_ns.h"
#include "common_utils.h"

static uint8_t readBuffer[FLASH_WRITE_LENGTH];

static void flash_operation_cleanup(void)
{
	g_flash0_close_guard(NULL);
	__enable_fault_irq();

}

/*******************************************************************************************************************//**
 * function: flash_write
 * description:
 * write FLASH_WRITE_LENGTH bytes to certain flash address
 * return: bool
 **********************************************************************************************************************/

bool flash_write_ns(uint32_t Flash_address, uint8_t *writeBuffer)
{

	volatile fsp_err_t  err = 0;
	flash_result_t blank_check_result = 0;
	bool status = true;

	err = g_flash0_open_guard(NULL, NULL);

	if(FSP_SUCCESS == err)
	{

		__disable_fault_irq();
		/*
		 * non-secure callable, flash erase and programming APIs are located in Secure region.
		 * flash erase will fail when trying to erase FLASH_WRITE_TEST_BLOCK2 (test case: TEST_F_NS_WRITE2_LOCKED_FLASH)
		 */
		err = g_flash0_erase_guard(NULL, Flash_address, NUM_OF_FLASH_SECTOR);
		if (FSP_SUCCESS == err)
		{
			err = g_flash0_blank_check_guard(NULL, Flash_address, FLASH_SECTOR_SIZE_256_BYTES, &blank_check_result);
			{
				if (FSP_SUCCESS == err)
				{
					if (FLASH_RESULT_BLANK == blank_check_result)
					{
						err = g_flash0_write_guard(NULL, (uint32_t)writeBuffer, Flash_address, FLASH_WRITE_LENGTH);
						{
							if (FSP_SUCCESS == err)
							{
								/*Read code flash data
								 * tips:
								 * below line will cause secure fault when trying to read from secure flash region: FLASH_WRITE_TEST_BLOCK1
								 * this situation corresponds the test case: TEST_F_NS_WRITE_S_FLASH
								 * no issue when accessing FLASH_WRITE_TEST_BLOCK3 (test case: TEST_F_NS_WRITE3_NS_FLASH)
								 */
								memcpy(readBuffer, (uint8_t *) Flash_address, FLASH_WRITE_LENGTH);
								/*below line will cause secure fault if trying to read from secure flash region */
								if(!memcmp(writeBuffer, readBuffer, FLASH_WRITE_LENGTH))
								{
									err = FSP_SUCCESS;
								}
							}

						}

					}
				}
			}

		}
	}

	if(FSP_SUCCESS != err)
	{
		if (FSP_ERR_ERASE_FAILED == err)
		{
			APP_PRINT(FLASH_ERASE_FAILED);
		}
		else if (FSP_ERR_ASSERTION == err)
		{
			APP_PRINT(FLASH_WRITE_SOURCE_ADDR_CHECK_FAILED);
		}
		status = false;
	}
	else
	{

		status = true;
	}
	flash_operation_cleanup();
	return status;
}
