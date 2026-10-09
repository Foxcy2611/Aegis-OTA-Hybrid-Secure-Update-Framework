#ifndef AEGIS_PINOUT_HW_H
#define AEGIS_PINOUT_HW_H

/* ==================== Giao tiếp AS32 phía node ==================== */

/* USART1: kết nối LoRa P2P chế độ truyền trong suốt với AS32 #2. */
#define AS32_UART_INSTANCE              USART1
#define AS32_UART_IRQn                  USART1_IRQn
#define AS32_UART_PORT                  GPIOA
#define AS32_UART_TX_PIN                GPIO_Pin_9
#define AS32_UART_RX_PIN                GPIO_Pin_10
#define AS32_UART_BAUDRATE              9600U

/* Các chân chọn chế độ hoạt động và báo sẵn sàng của AS32. */
#define AS32_MD0_PORT                   GPIOB
#define AS32_MD0_PIN                    GPIO_Pin_0
#define AS32_MD1_PORT                   GPIOB
#define AS32_MD1_PIN                    GPIO_Pin_1
#define AS32_AUX_PORT                   GPIOB
#define AS32_AUX_PIN                    GPIO_Pin_10

/* ==================== Giao tiếp debug cục bộ tùy chọn ==================== */

/* Mặc định tắt; không thuộc đường truyền OTA hoặc Recovery. */
#define LOCAL_DEBUG_UART_ENABLED        0U
#define LOCAL_DEBUG_UART_PORT           GPIOA
#define LOCAL_DEBUG_UART_TX_PIN         GPIO_Pin_2
#define LOCAL_DEBUG_UART_RX_PIN         GPIO_Pin_3
#define LOCAL_DEBUG_UART_BAUDRATE       115200U

/* ==================== Giao tiếp W25Q32 qua SPI1 ==================== */

#define W25Q32_PORT                     GPIOA
#define W25Q32_CS_PIN                   GPIO_Pin_4
#define W25Q32_CLK_PIN                  GPIO_Pin_5
#define W25Q32_MISO_PIN                 GPIO_Pin_6
#define W25Q32_MOSI_PIN                 GPIO_Pin_7

/* ==================== Trạng thái board ==================== */

#define STATUS_LED_PORT                 GPIOC
#define STATUS_LED_PIN                  GPIO_Pin_13

#endif /* AEGIS_PINOUT_HW_H */
