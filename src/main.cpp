#include <Arduino.h>
#include <U8g2lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include <stdio.h>
#include <string.h>

#define TEXT_LINE1_YY    (16 + 4 + 18)
#define TEXT_LINE2_YY    (TEXT_LINE1_YY + 4 + 18)

// U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, 12, 14, /* reset=*/U8X8_PIN_NONE);

bool g_auto_mode = true;
uint32_t g_lcd_show_index = 2;

// 文本表，每个索引对应一个字符串
const char* textTable[3] = {
    "道 路 畅 通,谨 慎 驾 驶",
    "车 流 量 大,禁 止 左 转",
    // "道 路 千 万 条,安 全 第 一 条",
    "请插入设备,等待信号输入"
};

void setup(void)
{
    Serial.begin(9600);
    Serial.println();

    Serial.println(F("SSD1306 start init \r\n"));
    u8g2.begin();
    u8g2.enableUTF8Print(); // enable UTF8 support for the Arduino print() function
    u8g2.setFontDirection(0);
    u8g2.clearBuffer();
    Serial.println(F("SSD1306 init done \r\n"));

}

void lcd_show_static_string(uint32_t index)
{
    uint32_t dx1, dy1, dx2, dy2;
    char *string_line_1 = NULL, *string_line_2 = NULL;
    char *commaPos = NULL;

    u8g2.setFont(u8g2_font_wqy16_t_gb2312);
    commaPos = strchr(textTable[index], ',');
    if (commaPos != NULL) {
        int offset = commaPos - textTable[index];
        string_line_1 = (char *)malloc(offset + 1);
        if (string_line_1 == NULL) return;
        memcpy(string_line_1, textTable[index], offset);
        string_line_1[offset] = '\0';
        string_line_2 = commaPos + 1;
    } else {
        string_line_2 = NULL;
    }

    if (string_line_1 != NULL) {
        dx1 = (u8g2.getWidth()- u8g2.getUTF8Width(string_line_1)) / 2;
        dy1 = TEXT_LINE1_YY;
        u8g2.drawUTF8(dx1, dy1, string_line_1);
    }

    if (string_line_2 != NULL) {
        dx2 = (u8g2.getWidth()- u8g2.getUTF8Width(string_line_2)) / 2;
        dy2 = TEXT_LINE2_YY;
        u8g2.drawUTF8(dx2, dy2, string_line_2);
    }

    free(string_line_1);
}

// AA BB 02 [00] [00] [checksum]    自动模式
// AA BB 02 [01] [checksum]         手动模式
// AA BB  n [02] xx xx xx xx xx xx xx [checksum]     手动模式,输入文本
// AA BB  n [03] xx xx xx xx xx xx xx [checksum]     手动模式,输入文本

uint32_t pack_lenght;
uint8_t rx_index = 0;
uint8_t uart_receive_buffer[128];
unsigned long uart_rx_timeout = 0;

void loop(void)
{
    uint8_t uart_ch;
    static char *input_str_1 = NULL;
    static char *input_str_2 = NULL;
    static bool refress_display = true;
    static bool pack_ready_flag = false;

    if (Serial.available() > 0) {
        // Serial.printf("tinmstamp = %ld. \r\n", millis());
        uart_rx_timeout = millis();
        uart_ch = Serial.read();
        if (rx_index == 3) {
            if ((uart_receive_buffer[0] == 0xAA) && (uart_receive_buffer[1] == 0xBB) && (uart_receive_buffer[2] > 3)) {
                pack_ready_flag = true;
                pack_lenght = (uart_receive_buffer[2]);
                uart_rx_timeout = millis();
            } else {
                pack_ready_flag = false;
                pack_lenght = 0;
                rx_index = 0;
                memset(uart_receive_buffer, 0, sizeof(uart_receive_buffer));
                Serial.printf("serial pack receive header error. \r\n");
                return;
            }
        }
        uart_receive_buffer[rx_index++] = uart_ch;
        if ((pack_ready_flag == true) && (rx_index == pack_lenght)) {
            Serial.printf("serial pack receive done, start paring. \r\n");
            for (uint32_t i = 0; i < pack_lenght; i++) {
                Serial.printf("0x%02x ", uart_receive_buffer[i]);
            }
            Serial.printf("\r\n");

            /* 校验数据包 */
            uint8_t check_sum = 0;
            for (uint32_t i = 0; i < (pack_lenght - 1); i++) {
                check_sum += uart_receive_buffer[i];
            }
            Serial.printf("pack_lenght:%d checksum:0x%02x \r\n", pack_lenght, check_sum);

            /* 解析数据包 */
            if (check_sum == uart_receive_buffer[pack_lenght - 1]) {
                Serial.printf("serial pack check sum check ok. \r\n");
                switch (uart_receive_buffer[3]) {
                    case 0:
                        // auto mode
                        g_auto_mode = true;
                        refress_display = true;
                        g_lcd_show_index = uart_receive_buffer[4];
                    break;

                    case 1:
                        g_auto_mode = false;
                    break;

                    case 2:
                        Serial.printf("serial pack receive text1:%s ", &uart_receive_buffer[4]);
                        g_auto_mode = false;
                        // refress_display = true;
                        input_str_1 = (char*)malloc(pack_lenght - 4);
                        memcpy(input_str_1, &uart_receive_buffer[4], pack_lenght - 5);
                        input_str_1[pack_lenght - 5] = '\0';
                    break;

                    case 3:
                        Serial.printf("serial pack receive text2:%s ", &uart_receive_buffer[4]);
                        g_auto_mode = false;
                        refress_display = true;
                        input_str_2 = (char*)malloc(pack_lenght - 4);
                        memcpy(input_str_2, &uart_receive_buffer[4], pack_lenght - 5);
                        input_str_2[pack_lenght - 5] = '\0';
                    break;

                    default:
                        refress_display = false;
                    break;
                }
            } else {
                Serial.printf("serial pack check sum error. \r\n");
            }
            pack_ready_flag = false;
            pack_lenght = 0;
            rx_index = 0;
        }
    }

    if (((millis() - uart_rx_timeout) > 1500) && (pack_ready_flag == true)) {
        Serial.printf("uart receive timeout, clear fifo. \r\n");
        memset(uart_receive_buffer, 0, sizeof(uart_receive_buffer));
        pack_ready_flag = false;
        pack_lenght = 0;
        rx_index = 0;
    }

    if (refress_display == false) {
        return;
    }

    Serial.printf("refresh page, auto_mode:%d page_index:%d. \r\n", g_auto_mode, g_lcd_show_index);

    /* 刷新页面*/
    u8g2.firstPage();
    do {
        /* 显示标题 */
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.drawUTF8((u8g2.getWidth()- u8g2.getUTF8Width("智 能 交 通"))/2, 15, "智 能 交 通");
        /* 显示提示信息 */
        if (g_auto_mode == true) {
            if (g_lcd_show_index <= 2) {
                lcd_show_static_string(g_lcd_show_index);
            }
        } else {
            u8g2.drawUTF8((u8g2.getWidth()- u8g2.getUTF8Width(input_str_1))/2, TEXT_LINE1_YY, input_str_1);
            u8g2.drawUTF8((u8g2.getWidth()- u8g2.getUTF8Width(input_str_2))/2, TEXT_LINE2_YY, input_str_2);
        }
    } while (u8g2.nextPage());
    free(input_str_1);
    free(input_str_2);
    input_str_1 = NULL;
    input_str_2 = NULL;
    refress_display = false;
}
