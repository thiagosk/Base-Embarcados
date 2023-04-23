#include <asf.h>
#include "conf_board.h"

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/* Botao da placa */
#define BUT_PIO     PIOA
#define BUT_PIO_ID  ID_PIOA
#define BUT_PIO_PIN 11
#define BUT_PIO_PIN_MASK (1 << BUT_PIO_PIN)

// LED PLACA
#define LED_PIO           PIOC                 
#define LED_PIO_ID        ID_PIOC                  
#define LED_IDX       8                    
#define LED_IDX_MASK  (1 << LED_IDX)   

// LED 1 OLED
#define LED_1_PIO PIOA
#define LED_1_PIO_ID ID_PIOA
#define LED_1_IDX 0
#define LED_1_IDX_MASK (1 << LED_1_IDX)

// LED 2 OLED
#define LED_2_PIO PIOC
#define LED_2_PIO_ID ID_PIOC
#define LED_2_IDX 30
#define LED_2_IDX_MASK (1 << LED_2_IDX)

// LED 3 OLED
#define LED_3_PIO PIOB
#define LED_3_PIO_ID ID_PIOB
#define LED_3_IDX 2
#define LED_3_IDX_MASK (1 << LED_3_IDX)

// BUT 1 OLED
#define BUT_1_PIO PIOD
#define BUT_1_PIO_ID ID_PIOD
#define BUT_1_IDX 28
#define BUT_1_IDX_MASK (1u << BUT_1_IDX)

// BUT 2 OLED
#define BUT_2_PIO PIOC
#define BUT_2_PIO_ID ID_PIOC
#define BUT_2_IDX 31
#define BUT_2_IDX_MASK (1u << BUT_2_IDX)

// BUT 3 OLED
#define BUT_3_PIO PIOA
#define BUT_3_PIO_ID ID_PIOA
#define BUT_3_IDX 19
#define BUT_3_IDX_MASK (1u << BUT_3_IDX)

// AFEC
#define AFEC_POT AFEC0
#define AFEC_POT_ID ID_AFEC0
#define AFEC_POT_CHANNEL 0 // Canal do pino PD30

/** RTOS  */
#define TASK_OLED_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_OLED_STACK_PRIORITY            (tskIDLE_PRIORITY)

#define TASK_PRINTCONSOLE_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_PRINTCONSOLE_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/** prototypes */
void but_callback(void);
void but1_callback(void);
void but2_callback(void);
void but3_callback(void);

static void io_init(void);

void pisca_LED(int n, int t);
void pisca_LED1(int n, int t);
void pisca_LED2(int n, int t);
void pisca_LED3(int n, int t);

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);

static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel, afec_callback_t callback);

QueueHandle_t xQueueBut;
QueueHandle_t xQueueBut1;
QueueHandle_t xQueueBut2;
QueueHandle_t xQueueBut3;

SemaphoreHandle_t xSemaphoreRTT;

TimerHandle_t xTimer; // pro AFEC

QueueHandle_t xQueueAFEC;

QueueHandle_t xQueueAFECtratado;

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/

void but_callback(void) {
	printf("but_callback\n");
	int but_id = 0;
	xQueueSendFromISR(xQueueBut, &but_id, 0);
}

void but1_callback(void) {
	printf("but1_callback\n");
	int but1_id = 1;
	xQueueSendFromISR(xQueueBut1, &but1_id, 0);
}

void but2_callback(void) {
	printf("but2_callback\n");
	int but2_id = 2;
	xQueueSendFromISR(xQueueBut2, &but2_id, 0);
}

void but3_callback(void) {
	printf("but3_callback\n");
	int but3_id = 3;
	xQueueSendFromISR(xQueueBut3, &but3_id, 0);
}

void RTT_Handler(void) {
    uint32_t ul_status;
    ul_status = rtt_get_status(RTT);
    /* IRQ due to Alarm */
    if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
        xSemaphoreGiveFromISR(xSemaphoreRTT, 0);
    }
}

static void AFEC_pot_Callback(void){
	int valor = afec_channel_get_value(AFEC_POT, AFEC_POT_CHANNEL);
	printf("valor AFEC: %d\n", valor);
	xQueueSendFromISR(xQueueAFEC, &valor, 0);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

void vTimerCallback(TimerHandle_t xTimer) {
  /* Selecina canal e inicializa conversão */
  afec_channel_enable(AFEC_POT, AFEC_POT_CHANNEL);
  afec_start_software_conversion(AFEC_POT);
}

static void task_AFEC(void *pvParameters){ // Aqui se trata o dado do AFEC
  for (;;) {
	int valor;
	if( xQueueReceive( xQueueAFEC, &valor, 10 )){
		printf("valor AFEC na task: %d\n", valor);
		xQueueSend(xQueueAFECtratado, &valor, 0);
	}
  }
}

static void task_oled(void *pvParameters) {
  gfx_mono_ssd1306_init();
  io_init();

  config_AFEC_pot(AFEC_POT, AFEC_POT_ID, AFEC_POT_CHANNEL, AFEC_pot_Callback);
  xTimer = xTimerCreate("Timer", 100, pdTRUE, (void *)0, vTimerCallback); // pro AFEC
  xTimerStart(xTimer, 0); // pro AFEC

  gfx_mono_draw_string("Exemplo RTOS", 0, 0, &sysfont);
  gfx_mono_draw_string("oii", 0, 20, &sysfont);

	int but_id;
	int but1_id;
	int but2_id;
	int but3_id;

	int valor;

	for (;;)
	{

		if( xQueueReceive( xQueueBut, &but_id, 10 )){
			gfx_mono_draw_filled_rect(0, 0, 160, 30, GFX_PIXEL_CLR);
			char buf[1];
			sprintf(buf,"BOTA0: %d",but_id);
			gfx_mono_draw_string(buf, 0, 20, &sysfont);

			pisca_LED(3, 100);
		}

		if( xQueueReceive( xQueueBut1, &but1_id, 10 )){

			if (!pio_get(BUT_1_PIO, PIO_INPUT, BUT_1_IDX_MASK)) {
				RTT_init(10000, 0, 0);
			} else {
				uint32_t time_read = rtt_read_timer_value(RTT);
				printf("time_read BUT1: %d\n", time_read);
				if (time_read > 15000) { // +- 3s
					gfx_mono_draw_filled_rect(0, 0, 160, 30, GFX_PIXEL_CLR);
					char buf[1];
					sprintf(buf,"BOTA0: %d",but1_id);
					gfx_mono_draw_string(buf, 0, 20, &sysfont);

					pisca_LED1(3, 100);

				} else {
					gfx_mono_draw_filled_rect(0, 0, 160, 30, GFX_PIXEL_CLR);
					gfx_mono_draw_string("Aperto rapido", 0, 10, &sysfont);
					char buf[1];
					sprintf(buf,"BOTA0: %d",but1_id);
					gfx_mono_draw_string(buf, 0, 20, &sysfont);
				}
			}
		}

		if( xQueueReceive( xQueueBut2, &but2_id, 10 )){
			gfx_mono_draw_filled_rect(0, 0, 160, 30, GFX_PIXEL_CLR);
			char buf[1];
			sprintf(buf,"BOTA0: %d",but2_id);
			gfx_mono_draw_string(buf, 0, 20, &sysfont);

			pisca_LED2(3, 100);
		}

		if( xQueueReceive( xQueueBut3, &but3_id, 10 )){
			gfx_mono_draw_filled_rect(0, 0, 160, 30, GFX_PIXEL_CLR);
			gfx_mono_draw_string("COMECO ALARME", 0, 10, &sysfont);
			char buf[1];
			sprintf(buf,"BOTA0: %d",but3_id);
			gfx_mono_draw_string(buf, 0, 20, &sysfont);

			RTT_init(1, 3, RTT_MR_ALMIEN); // 3s
		}

		if (xSemaphoreTake(xSemaphoreRTT, 0)) {
			gfx_mono_draw_filled_rect(0, 0, 160, 30, GFX_PIXEL_CLR);
			gfx_mono_draw_string("ACABO ALARME", 0, 10, &sysfont);
			gfx_mono_draw_string("BOTAO: 3", 0, 20, &sysfont);

			pisca_LED3(3, 100);
		}	

		if( xQueueReceive( xQueueAFECtratado, &valor, 10 )){
			printf("valor AFEC tratado: %d\n", valor);
		}
	}
}


static void task_printConsole(void *pvParameters) {
	uint32_t cont=0;
	for (;;)
	{		
		cont++;
		// printf("%03d\n",cont);
		// vTaskDelay(1000);
	}
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

void pisca_LED(int n, int t) {
	for (int i=0;i<n;i++) {
		pio_clear(LED_PIO, LED_IDX_MASK);
		vTaskDelay(t);
		pio_set(LED_PIO, LED_IDX_MASK);
		vTaskDelay(t);
	}
}

void pisca_LED1(int n, int t) {
	for (int i=0;i<n;i++) {
		pio_clear(LED_1_PIO, LED_1_IDX_MASK);
		vTaskDelay(t);
		pio_set(LED_1_PIO, LED_1_IDX_MASK);
		vTaskDelay(t);
	}
}

void pisca_LED2(int n, int t) {
	for (int i=0;i<n;i++) {
		pio_clear(LED_2_PIO, LED_2_IDX_MASK);
		vTaskDelay(t);
		pio_set(LED_2_PIO, LED_2_IDX_MASK);
		vTaskDelay(t);
	}
}

void pisca_LED3(int n, int t) {
	for (int i=0;i<n;i++) {
		pio_clear(LED_3_PIO, LED_3_IDX_MASK);
		vTaskDelay(t);
		pio_set(LED_3_PIO, LED_3_IDX_MASK);
		vTaskDelay(t);
	}
}

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.charlength = CONF_UART_CHAR_LENGTH,
		.paritytype = CONF_UART_PARITY,
		.stopbits = CONF_UART_STOP_BITS,
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

static void io_init(void) {

	/* conf botão como entrada */
	pio_configure(BUT_PIO, PIO_INPUT, BUT_PIO_PIN_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_PIO, BUT_PIO_PIN_MASK, 60);
	pio_handler_set(BUT_PIO,
					BUT_PIO_ID,
					BUT_PIO_PIN_MASK,
					PIO_IT_FALL_EDGE,
					but_callback);
	pio_enable_interrupt(BUT_PIO, BUT_PIO_PIN_MASK);
	pio_get_interrupt_status(BUT_PIO);
	/* configura prioridae */
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4);

	// LED PLACA
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_set_output(LED_PIO, LED_IDX_MASK, 1, 0, 0);

	// LED 1 OLED
	pmc_enable_periph_clk(LED_1_PIO_ID);
	pio_set_output(LED_1_PIO, LED_1_IDX_MASK, 1, 0, 0);

	// LED 2 OLED
	pmc_enable_periph_clk(LED_2_PIO_ID);
	pio_set_output(LED_2_PIO, LED_2_IDX_MASK, 1, 0, 0);

	// LED 3 OLED
	pmc_enable_periph_clk(LED_3_PIO_ID);
	pio_set_output(LED_3_PIO, LED_3_IDX_MASK, 1, 0, 0);

	// BUT 1 OLED
	pio_configure(BUT_1_PIO, PIO_INPUT, BUT_1_IDX_MASK, PIO_PULLUP| PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_1_PIO, BUT_1_IDX_MASK, 60);
	pio_handler_set(BUT_1_PIO, BUT_1_PIO_ID, BUT_1_IDX_MASK, PIO_IT_EDGE, but1_callback);
	pio_enable_interrupt(BUT_1_PIO, BUT_1_IDX_MASK);
	pio_get_interrupt_status(BUT_1_PIO);
	NVIC_EnableIRQ(BUT_1_PIO_ID);
	NVIC_SetPriority(BUT_1_PIO_ID, 4);

	// BUT 2 OLED
	pio_configure(BUT_2_PIO, PIO_INPUT, BUT_2_IDX_MASK, PIO_PULLUP| PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_2_PIO, BUT_2_IDX_MASK, 60);
	pio_handler_set(BUT_2_PIO, BUT_2_PIO_ID, BUT_2_IDX_MASK, PIO_IT_FALL_EDGE, but2_callback);
	pio_enable_interrupt(BUT_2_PIO, BUT_2_IDX_MASK);
	pio_get_interrupt_status(BUT_2_PIO);
	NVIC_EnableIRQ(BUT_2_PIO_ID);
	NVIC_SetPriority(BUT_2_PIO_ID, 4);

	// BUT 3 OLED
	pio_configure(BUT_3_PIO, PIO_INPUT, BUT_3_IDX_MASK, PIO_PULLUP| PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_3_PIO, BUT_3_IDX_MASK, 60);
	pio_handler_set(BUT_3_PIO, BUT_3_PIO_ID, BUT_3_IDX_MASK, PIO_IT_FALL_EDGE, but3_callback);
	pio_enable_interrupt(BUT_3_PIO, BUT_3_IDX_MASK);
	pio_get_interrupt_status(BUT_3_PIO);
	NVIC_EnableIRQ(BUT_3_PIO_ID);
	NVIC_SetPriority(BUT_3_PIO_ID, 4);

}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int)(((float)32768) / freqPrescale);

	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);

	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT))
		;
		rtt_write_alarm_time(RTT, IrqNPulses + ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
}

static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel,
afec_callback_t callback) {
	/*************************************
	* Ativa e configura AFEC
	*************************************/
	/* Ativa AFEC - 0 */
	afec_enable(afec);

	/* struct de configuracao do AFEC */
	struct afec_config afec_cfg;

	/* Carrega parametros padrao */
	afec_get_config_defaults(&afec_cfg);

	/* Configura AFEC */
	afec_init(afec, &afec_cfg);

	/* Configura trigger por software */
	afec_set_trigger(afec, AFEC_TRIG_SW);

	/*** Configuracao espec�fica do canal AFEC ***/
	struct afec_ch_config afec_ch_cfg;
	afec_ch_get_config_defaults(&afec_ch_cfg);
	afec_ch_cfg.gain = AFEC_GAINVALUE_0;
	afec_ch_set_config(afec, afec_channel, &afec_ch_cfg);

	/*
	* Calibracao:
	* Because the internal ADC offset is 0x200, it should cancel it and shift
	down to 0.
	*/
	afec_channel_set_analog_offset(afec, afec_channel, 0x200);

	/***  Configura sensor de temperatura ***/
	struct afec_temp_sensor_config afec_temp_sensor_cfg;

	afec_temp_sensor_get_config_defaults(&afec_temp_sensor_cfg);
	afec_temp_sensor_set_config(afec, &afec_temp_sensor_cfg);

	/* configura IRQ */
	afec_set_callback(afec, afec_channel, callback, 1);
	NVIC_SetPriority(afec_id, 4);
	NVIC_EnableIRQ(afec_id);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/

int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();

	/* Initialize the console uart */
	configure_console();

	/* Create task to control oled */
	if (xTaskCreate(task_oled, "oled", TASK_OLED_STACK_SIZE, NULL, TASK_OLED_STACK_PRIORITY, NULL) != pdPASS) {
	  printf("Failed to create oled task\r\n");
	}
	
	if (xTaskCreate(task_printConsole, "task_printConsole", TASK_PRINTCONSOLE_STACK_SIZE, NULL, TASK_PRINTCONSOLE_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create printConsole task\r\n");
	}

	xQueueBut = xQueueCreate(32, sizeof(int) );
	xQueueBut1 = xQueueCreate(32, sizeof(int) );
	xQueueBut2 = xQueueCreate(32, sizeof(int) );
	xQueueBut3 = xQueueCreate(32, sizeof(int) );

	xSemaphoreRTT = xSemaphoreCreateBinary();

	xQueueAFEC = xQueueCreate(32, sizeof(int) );

	if (xTaskCreate(task_AFEC, "Led", 1024, NULL, 0, NULL) != pdPASS) {
	printf("Failed to create test led task\r\n");
	}

	xQueueAFECtratado = xQueueCreate(32, sizeof(int) );

	/* Start the scheduler. */
	vTaskStartScheduler();

  /* RTOS não deve chegar aqui !! */
	while(1){}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}
