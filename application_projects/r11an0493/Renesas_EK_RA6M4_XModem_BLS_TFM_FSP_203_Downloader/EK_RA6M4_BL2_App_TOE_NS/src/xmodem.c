// xmodem.c

#include <string.h>
#include "xmodem.h"
#include "./comms/comms.h"
#include "hal_data.h"
#include "downloader_thread.h"
#include "header.h"
#include "tfm_platform_api.h"
#include "tfm_ioctl_api.h"

unsigned char XmodemDownloadAndProgramFlash (unsigned long FlashAddress)
{
/*
XmodemDownloadAndProgramFlash() takes a memory address as the base address to
which data downloaded is programmed.  The data is downloaded using the XModem
protocol developed in 1977 by Ward Christensen.
The routine detects XM_ERRORs due to XM_TIMEOUTs, comms XM_ERRORs or invalid checksums.
The routine reports the following to the caller:
-Success
-Invalid address	(not implemented here as the address is checked prior to this
					function being called)
-Comms XM_ERROR
-XM_TIMEOUT XM_ERROR
-Failure to program flash


Expects:    
--------
FlashAddress:
32-bit address located in Flash memory space starting on a 128-byte boundary

Returns:
--------
XM_OK				-	Download and Flash programming performed ok
XM_ADDRESS_XM_ERROR 	-	Address was either not on a 128-bit boundary or not in valid Flash
XM_COMMS_XM_ERROR	 	-	Comms parity, framing or overrun XM_ERROR
XM_XM_TIMEOUT			-	Transmitter did not respond to this receiver
XM_PROG_FAIL		-	Failed to program one or more bytes of the Flash memory
*/
	unsigned char   xm_ret;
	unsigned char   ExpectedBlkNum;
	unsigned char   RetryCounter;
	unsigned char   RxByteBufferIndex;
	unsigned char   checksum;
	unsigned char   StartCondition;
	unsigned long   Address;
	unsigned char   RxByteBuffer[132] BSP_ALIGN_VARIABLE(4);
	uint8_t         tx_byte BSP_ALIGN_VARIABLE(4);
	uint32_t        rx_len;

	// first xmodem block number is 1
	ExpectedBlkNum = 1;
	
	StartCondition = 0;
	
	Address = FlashAddress;
	
	while(1)
	{
		//	{1}
		//	initialise Rx attempts
		RetryCounter = 10;
	
		//	decrement Rx attempts counter & get Rx byte with a 10 sec TIMEOUT repeat until Rx attempts is 0
		xm_ret = XM_TOUT;
		while ( (RetryCounter > 0) && (xm_ret == XM_TOUT) )
		{
			if (StartCondition == 0)
			{
				//	if this is the start of the xmodem frame
				//	send a NAK to the transmitter
			    tx_byte = NAK;
			    comms_send(&tx_byte, 1);    // Kick off the XModem transfer

				/* Request 132 bytes from host with a delay of 10 seconds */
				fsp_err_t err;
				rx_len = 132;
				memset((void *)&RxByteBuffer[0], 0, 132);
				err = comms_read((uint8_t *)&RxByteBuffer[0], &rx_len, 10000);
				if(FSP_SUCCESS == err)
				{
				    xm_ret = OK;
				}
				else if(FSP_ERR_TIMEOUT == err)
				{
				    xm_ret = XM_TOUT;
				}
				else
				{
				    xm_ret = XM_ERROR;
				}
			}                             
			else
			{
			    /* Request 132 bytes from host with a delay of 1 second */
                fsp_err_t err;
                rx_len = 132;
                memset((void *)&RxByteBuffer[0], 0, 132);
                err = comms_read((uint8_t *)&RxByteBuffer[0], &rx_len, 1000);
                if(FSP_SUCCESS == err)
                {
                    xm_ret = OK;
                }
                else if(FSP_ERR_TIMEOUT == err)
                {
                    xm_ret = XM_TOUT;
                }
                else
                {
                    xm_ret = XM_ERROR;
                }
			}
			RetryCounter--;
		}
			
		StartCondition = 1;

		if ( xm_ret == XM_ERROR )
        {
            return ( XM_ERROR );
        }
		else if ( xm_ret == XM_TOUT )
		{
		    //  if timed out after 10 attempts
		    return ( XM_TIMEOUT );
		}
		else			
		{
			// if first received byte is 'end of frame'
			// return ACK to sender
		    if ( RxByteBuffer[0] == EOT )
			{
		        tx_byte = ACK;
		        comms_send(&tx_byte, 1);
				return( XM_OK );
			}
			else
			{				
                // data Rx ok
                // calculate the checksum of the data bytes only
                checksum = 0;
                int cs = 0;
                for (RxByteBufferIndex=0; RxByteBufferIndex<128; RxByteBufferIndex++)
                {
                    cs += RxByteBuffer[RxByteBufferIndex + 3];
                }

                /* This int to unsigned char conversion needed to eliminate conversion warning with checksum calculation. */
                checksum = (unsigned char)cs;

                //	if SOH, BLK#, 255-BLK# or checksum not valid
                //	(BLK# is valid if the same as expected blk counter or is 1 less
                if ( !( (RxByteBuffer[0] == SOH) && ((RxByteBuffer[1] == ExpectedBlkNum) || (RxByteBuffer[1] == ExpectedBlkNum - 1) ) && (RxByteBuffer[2] + RxByteBuffer[1] == 255 ) && (RxByteBuffer[131] == checksum) ) )
                {
                    //	send NAK and loop back to (1)
                    tx_byte = NAK;
                    comms_send(&tx_byte, 1);
                }
                else
                {
                    //	if blk# is expected blk num
                    if ( RxByteBuffer[1] == ExpectedBlkNum )
                    {
                        //	Program the received data into flash

                        /* Assume flash area is erased */
#if 0
                        fsp_err_t err = FSP_SUCCESS;
                        ThreadsAndInterrupts(DISABLE);
                        err = g_flash0.p_api->write(g_flash0.p_ctrl, (uint32_t)&RxByteBuffer[3], Address, 128);
                        ThreadsAndInterrupts(RE_ENABLE);
#endif
                        int32_t  tfm_err_status;
                        uint32_t flash_test_result = 0;
                        tfm_err_status =  tfm_platform_flash_write((uint32_t*)&RxByteBuffer[3], (uint32_t)Address, 128, &flash_test_result);

                        //if(FSP_SUCCESS == err)
                        if(TFM_PLATFORM_ERR_SUCCESS == tfm_err_status)
                        {
                            //	if prog routine passed ok increment flash address by 128
                            Address += 128;
                            ExpectedBlkNum++;
                            tx_byte = ACK;
                            comms_send(&tx_byte, 1);

                            //	loop back to (1)
                        }
                        else
                        {
                            // prog fail
                            tx_byte = NAK;
                            comms_send(&tx_byte, 1);
                            // cancel xmodem download
                            tx_byte = CAN;
                            comms_send(&tx_byte, 1);

                            return( XM_PROG_FAIL );
                        }
                    }
                    else
                    {
                        // 	block number is valid but this data block has already been received
                        //	send ACK and loop to (1)
                        tx_byte = ACK;
                        comms_send(&tx_byte, 1);
                    }
                }
            }
		}
	}		
}
