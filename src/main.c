/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SPI_MASTER NRF_SPIM3
#define SPI_TRIG_TIMER NRF_TIMER1
#define SPI_TRIG_COUNTER NRF_TIMER2

#define LIST_ITEMS 2000
#define END_MARGIN 100
#define TIMER_RELOAD_US 100
#define LIST_TOTAL_ITEMS (LIST_ITEMS + END_MARGIN)

static uint8_t m_tx_buffer[LIST_TOTAL_ITEMS][2], m_rx_buffer[LIST_TOTAL_ITEMS][2];
static uint32_t m_item_num;
static uint32_t m_item_index;
K_SEM_DEFINE(m_sem_samples_available, 0, 1);

ISR_DIRECT_DECLARE(trig_cnt_irq_handler)
{
	// Half way point
	if(SPI_TRIG_COUNTER->EVENTS_COMPARE[0]) {
		SPI_TRIG_COUNTER->EVENTS_COMPARE[0] = 0;
		
		m_item_num = LIST_ITEMS / 2;
		m_item_index = 0;
		k_sem_give(&m_sem_samples_available);
	}	

	// End of buffer (not including margin)
	if(SPI_TRIG_COUNTER->EVENTS_COMPARE[1]) {
		SPI_TRIG_COUNTER->EVENTS_COMPARE[1] = 0;

		// If there is a risk that this interrupt gets delayed we need to handle the case where more items have been sampled than expected
		// trigger a capture on the counter to check how far the count has reached
		// TODO: Possibly we would also need to check the state of the timer to check if is just about to trigger another sample, which could lead to a race condition
		SPI_TRIG_COUNTER->TASKS_CAPTURE[3] = 1;
		m_item_num = SPI_TRIG_COUNTER->CC[3] - (LIST_ITEMS / 2);
		m_item_index = LIST_ITEMS / 2;
		SPI_MASTER->TXD.PTR = (uint32_t)m_tx_buffer[0];
		SPI_MASTER->RXD.PTR = (uint32_t)m_rx_buffer[0];
		SPI_TRIG_COUNTER->TASKS_CLEAR = 1;
		k_sem_give(&m_sem_samples_available);
	}

	ISR_DIRECT_PM();
	return 1;
}

static void trig_timer_init(void)
{
	SPI_TRIG_TIMER->PRESCALER = 4;
	SPI_TRIG_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
	SPI_TRIG_TIMER->CC[0] = TIMER_RELOAD_US;
	SPI_TRIG_TIMER->SHORTS  = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
	SPI_TRIG_TIMER->TASKS_CLEAR = 1;
	SPI_TRIG_TIMER->TASKS_START = 1;
}

static void trig_counter_init(void)
{
	SPI_TRIG_COUNTER->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
	SPI_TRIG_COUNTER->CC[0] = LIST_ITEMS / 2;
	SPI_TRIG_COUNTER->CC[1] = LIST_ITEMS;
	SPI_TRIG_COUNTER->INTENSET = TIMER_INTENSET_COMPARE0_Msk | TIMER_INTENSET_COMPARE1_Msk;
	SPI_TRIG_COUNTER->TASKS_CLEAR = 1;
	SPI_TRIG_COUNTER->TASKS_START = 1;

    IRQ_DIRECT_CONNECT(TIMER2_IRQn, IRQ_PRIO_LOWEST, trig_cnt_irq_handler, 0);
	irq_enable(TIMER2_IRQn);
}

static void trig_ppi_init(void)
{
	NRF_PPI->CH[2].EEP = (uint32_t)&SPI_TRIG_TIMER->EVENTS_COMPARE[0];
	NRF_PPI->CH[2].TEP = (uint32_t)&SPI_MASTER->TASKS_START;
	NRF_PPI->FORK[2].TEP = (uint32_t)&SPI_TRIG_COUNTER->TASKS_COUNT;
	NRF_PPI->CHENSET = (1 << 2);
}

static void spi_init(void)
{
	SPI_MASTER->ENABLE = SPIM_ENABLE_ENABLE_Enabled;
	SPI_MASTER->FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M1 << SPIM_FREQUENCY_FREQUENCY_Pos;
	SPI_MASTER->PSEL.CSN = 33;
	SPI_MASTER->PSEL.SCK = 35;
	SPI_MASTER->PSEL.MOSI = 36;
	SPI_MASTER->PSEL.MISO = 37;
	SPI_MASTER->TXD.PTR = (uint32_t)m_tx_buffer[0];
	SPI_MASTER->TXD.MAXCNT = 2;
	SPI_MASTER->TXD.LIST = SPIM_TXD_LIST_LIST_ArrayList << SPIM_TXD_LIST_LIST_Pos;
	SPI_MASTER->RXD.PTR = (uint32_t)m_rx_buffer[0];
	SPI_MASTER->RXD.MAXCNT = 2;
	SPI_MASTER->RXD.LIST = SPIM_RXD_LIST_LIST_ArrayList << SPIM_RXD_LIST_LIST_Pos;
}

void main(void)
{
    printk("Starting nrfx_spim basic non-blocking example.\n");

	trig_timer_init();

	trig_counter_init();

	trig_ppi_init();

	spi_init();

	// Put some dummy data in the TX buffer
	for(int i = 0; i < LIST_ITEMS; i++) {
		m_tx_buffer[i][0] = 0x5;
		m_tx_buffer[i][1] = i+1;
	}

	while (1) {
		// Wait for the samples available semaphore to be set
		k_sem_take(&m_sem_samples_available, K_FOREVER);

		// For debugging purposes, print the index and length of the buffer, as well as the first two readings
		printk("I: %.4i, S: %.4i ", m_item_index, m_item_num);
		printk("Val 0: h%.2x-%.2x, Val 1: h%.2x-%.2x", m_rx_buffer[m_item_index][0], m_rx_buffer[m_item_index][1], m_rx_buffer[m_item_index+1][0], m_rx_buffer[m_item_index+1][1]);
		printk("\n");
	}
}
