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

QueueHandle_t xQueueBut;
QueueHandle_t xQueueBut1;
QueueHandle_t xQueueBut2;
QueueHandle_t xQueueBut3;

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

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_oled(void *pvParameters) {
  io_init();

  gfx_mono_ssd1306_init();
  gfx_mono_draw_string("Exemplo RTOS", 0, 0, &sysfont);
  gfx_mono_draw_string("oii", 0, 20, &sysfont);

	int but_id;
	int but1_id;
	int but2_id;
	int but3_id;

	for (;;)
	{

		if( xQueueReceive( xQueueBut, &but_id, 10 )){
			gfx_mono_draw_filled_rect(0, 0, 120, 30, GFX_PIXEL_CLR);
			char buf[1];
			sprintf(buf,"BOTA0: %d",but_id);
			gfx_mono_draw_string(buf, 0, 20, &sysfont);

			pisca_LED(3, 100);
		}

		if( xQueueReceive( xQueueBut1, &but1_id, 10 )){
			gfx_mono_draw_filled_rect(0, 0, 120, 30, GFX_PIXEL_CLR);
			char buf[1];
			sprintf(buf,"BOTA0: %d",but1_id);
			gfx_mono_draw_string(buf, 0, 20, &sysfont);

			pisca_LED1(3, 100);
		}

		if( xQueueReceive( xQueueBut2, &but2_id, 10 )){
			gfx_mono_draw_filled_rect(0, 0, 120, 30, GFX_PIXEL_CLR);
			char buf[1];
			sprintf(buf,"BOTA0: %d",but2_id);
			gfx_mono_draw_string(buf, 0, 20, &sysfont);

			pisca_LED2(3, 100);
		}

		if( xQueueReceive( xQueueBut3, &but3_id, 10 )){
			gfx_mono_draw_filled_rect(0, 0, 120, 30, GFX_PIXEL_CLR);
			char buf[1];
			sprintf(buf,"BOTA0: %d",but3_id);
			gfx_mono_draw_string(buf, 0, 20, &sysfont);

			pisca_LED3(3, 100);
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
	pio_handler_set(BUT_1_PIO, BUT_1_PIO_ID, BUT_1_IDX_MASK, PIO_IT_FALL_EDGE, but1_callback);
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

	/* Start the scheduler. */
	vTaskStartScheduler();

  /* RTOS não deve chegar aqui !! */
	while(1){}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}
