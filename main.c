/*******************************************************************************
 * File Name:   main.c
 *
 * Description: This is the source code for the P-DMA 2D Transfer
 *              for ModusToolbox.
 *
 * Related Document: See README.md
 *
 *
*******************************************************************************
* (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "mtb_hal.h"

/******************************************************************************
* Macros
*******************************************************************************/
/* The priority for interrupt */
#define GPIO_INTERRUPT_PRIORITY (7u)

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* The status of USER_BTN1 interrupt */
volatile bool g_isInterrupt_SW1 = false;

/* P-DMA interrupt configuration */
const cy_stc_sysint_t TX_DMA_2D_INT_CFG =
{
    .intrSrc      = (IRQn_Type)(((NvicMux4_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | (IRQn_Type)TxDma2D_IRQ )),
    .intrPriority = 6u,
};

const cy_stc_sysint_t BTN_IRQ_CFG =
{
    .intrSrc = (IRQn_Type)(((NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | CYBSP_USER_BTN_IRQ)),
    .intrPriority = GPIO_INTERRUPT_PRIORITY
};

/* Source data to be transferred */
const uint8_t SOURCE_DATA[] =
{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '!', '@',
};

/* For the Retarget -IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    UART_context;           /* UART context */
static mtb_hal_uart_t               UART_hal_obj;           /* Debug UART HAL object  */

/*******************************************************************************
* Function Definitions
*******************************************************************************/
void handle_GPIO_Interrupt_SW1(void);

/*******************************************************************************
* Function Name: handle_GPIO_Interrupt_SW1
********************************************************************************
* Summary:
* GPIO interrupt handler for USER_BTN1.
*
* Parameters:
*  handlerArg (unused)
*  event (unused)
*
* Return:
*  void
*
*******************************************************************************/
void handle_GPIO_Interrupt_SW1(void)
{
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM);

    g_isInterrupt_SW1 = true;
}

/*******************************************************************************
* Function Name: handle_PDMA_Interrupt
********************************************************************************
* Summary:
* Handles PDMA interrupt.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/
static void handle_PDMA_Interrupt(void)
{
    cy_en_dma_intr_cause_t chStatus;

    Cy_DMA_Channel_ClearInterrupt(TxDma2D_HW, TxDma2D_CHANNEL);

    /* Check interrupt cause to capture errors. */
    chStatus = Cy_DMA_Channel_GetStatus(TxDma2D_HW, TxDma2D_CHANNEL);
    if (chStatus == CY_DMA_INTR_CAUSE_COMPLETION)
    {
        Cy_SCB_UART_PutString(UART_HW, "\r\nX-loop done!\r\n\n");
    }
    else
    {
        char buf[16];
        sprintf(buf, "\r\nerror:%d\r\n\n", chStatus);
        Cy_SCB_UART_PutString(UART_HW, buf);

        CY_ASSERT(0);
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* The main function.
*   1. Initializes the board, retarget-io and led
*   2. Configures the SPI Master to send command packet to the slave
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    if (cybsp_init() != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    #if defined (CY_DEVICE_TVIIBH8M) || defined (CY_DEVICE_TVIIBH4M) || defined (CY_DEVICE_TVIIBH16M) || defined (CY_DEVICE_TVIIC2D6M) || defined (CY_DEVICE_TVIIC2D4M)
    /* Disable caches for DMA coherency */   
    SCB_DisableICache();
    SCB_DisableDCache();
    #endif

    /* Debug UART init */
    result = (cy_rslt_t)Cy_SCB_UART_Init(UART_HW, &UART_config, &UART_context);

    /* UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&UART_hal_obj, &UART_hal_config, &UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cy_retarget_io_init(&UART_hal_obj);

    /* HAL retarget_io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    Cy_SCB_UART_PutString(UART_HW, "\x1b[2J\x1b[;H");
    
    Cy_SCB_UART_PutString(UART_HW, "************************************************************\r\n");
    Cy_SCB_UART_PutString(UART_HW, "PDMA 2D Transfer code example\r\n");
    Cy_SCB_UART_PutString(UART_HW, "************************************************************\r\n\n");
    Cy_SCB_UART_PutString(UART_HW, ">> push USER_BTN1 to transfer 4-bytes of X(row) \r\n\n");

    /* Configure GPIO interrupt for SW1 */
    Cy_SysInt_Init(&BTN_IRQ_CFG, &handle_GPIO_Interrupt_SW1);
    NVIC_ClearPendingIRQ((IRQn_Type)BTN_IRQ_CFG.intrSrc);
    NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);

    /* Initialize PDMA */
    if (Cy_DMA_Descriptor_Init(&TxDma2D_Descriptor_0, &TxDma2D_Descriptor_0_config) != CY_DMA_SUCCESS)
    {
        CY_ASSERT(0);
    }
    if (Cy_DMA_Channel_Init(TxDma2D_HW, TxDma2D_CHANNEL, &TxDma2D_channelConfig) != CY_DMA_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Set source and destination address for PDMA descriptor */
    Cy_DMA_Descriptor_SetSrcAddress(&TxDma2D_Descriptor_0, (uint32_t *) SOURCE_DATA);
    Cy_DMA_Descriptor_SetDstAddress(&TxDma2D_Descriptor_0, (uint32_t *) &UART_HW->TX_FIFO_WR);

    /* Set descriptor for PDMA channel */
    Cy_DMA_Channel_SetDescriptor(TxDma2D_HW, TxDma2D_CHANNEL, &TxDma2D_Descriptor_0);

    /* Initialize and enable interrupt from PDMA */
    Cy_SysInt_Init(&TX_DMA_2D_INT_CFG, &handle_PDMA_Interrupt);
    NVIC_EnableIRQ((IRQn_Type) NvicMux4_IRQn);

    /* Enable PDMA interrupt source. */
    Cy_DMA_Channel_SetInterruptMask(TxDma2D_HW, TxDma2D_CHANNEL, CY_DMA_INTR_MASK);

    /* Enable channel and PDMA block to start descriptor execution process */
    Cy_DMA_Channel_Enable(TxDma2D_HW, TxDma2D_CHANNEL);
    Cy_DMA_Enable(TxDma2D_HW);

    for (;;)
    {
        /* Check if USER_BTN1 is pressed */
        if (g_isInterrupt_SW1 == true)
        {
            /* Clear the flag */
            g_isInterrupt_SW1 = false;

            /* Trigger P-DMA Transfer */
            Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_0_PDMA0_TR_IN0, CY_TRIGGER_TWO_CYCLES);
        }
    }
}

/* [] END OF FILE */
