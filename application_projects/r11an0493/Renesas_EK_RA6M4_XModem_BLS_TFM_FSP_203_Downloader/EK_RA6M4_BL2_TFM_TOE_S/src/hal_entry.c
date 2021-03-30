#include "hal_data.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

/*******************************************************************************************************************//**
 * main() is generated by the RA Configuration editor and is used to generate threads if an RTOS is used.  This function
 * is called by main() when no RTOS is used.
 **********************************************************************************************************************/
void hal_entry(void)
{
    /* TODO: add your own code here */
    extern uint32_t __stack;
    __set_CONTROL_SPSEL (1);
    __set_PSP (&__stack);
    tfm_main ();
#if BSP_TZ_SECURE_BUILD
    /* Enter non-secure code */
    R_BSP_NonSecureEnter ();
#endif
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart(bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
#if BSP_FEATURE_FLASH_LP_VERSION != 0
/* Enable reading from data flash. */
R_FACI_LP->DFLCTL = 1U;
/* Would normally have to wait tDSTOP(6us) for data flash
recovery. Placing the enable here, before clock and
* C runtime initialization, should negate the need for a
delay since the initialization will typically take more than 6us. */
#endif
    }
    if (BSP_WARM_START_POST_CLOCK == event)
    {
        /* C runtime environment and system clocks are setup. */
        RM_TFM_SystemInit ();
    }
    if (BSP_WARM_START_POST_C == event)
    {
        /* Crypto hardware is initialized. */
        mbedtls_platform_setup (NULL);
        /* Configure pins. */
        R_IOPORT_Open (&g_ioport_ctrl, &g_bsp_pin_cfg);
    }
}

#if BSP_TZ_SECURE_BUILD

BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();

/* Trustzone Secure Projects require at least one nonsecure callable function in order to build (Remove this if it is not required to build). */
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{

}
#endif
