/* Liikennevalot RGB2-ledillä ja taskien avulla

   Tavoite: 4 pistettä
   Saavutettu kaikki tarvittavat tavoitteet 4 pistettä varten 
   Sekä kaikki mahdolliset ylimääräiset kohdat.
   Toteutettu: UART-sekvenssin vastaanotto,
   FIFO UART, Dispatcher-task, FIFO eri väreille,
   Condition variablet taskien ohjaukseen, Release signaali disp-
   atcherille, sekvenssin ajastus, sekvenssin toisto, valotaskit 
   eivät käytä polling-superlooppeja
   Nappien oletus:
   sw0 = nappi 1 = Play/Pause
   sw1 = nappi 2 = punainen
   sw2 = nappi 3 = keltainen
   sw3 = nappi 4 = vihreä
   sw4 = nappi 5 = vilkkuva keltainen
*/
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define RED_LED   DT_ALIAS(led3)
#define GREEN_LED DT_ALIAS(led4)
#define BLUE_LED  DT_ALIAS(led5)

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED, gpios);

static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN_LED, gpios);

static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BLUE_LED, gpios);

#define BUTTON_0 DT_ALIAS(sw0)
#define BUTTON_1 DT_ALIAS(sw1)
#define BUTTON_2 DT_ALIAS(sw2)
#define BUTTON_3 DT_ALIAS(sw3)
#define BUTTON_4 DT_ALIAS(sw4)

static const struct gpio_dt_spec button_0 =
    GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});

static const struct gpio_dt_spec button_1 =
    GPIO_DT_SPEC_GET_OR(BUTTON_1, gpios, {0});

static const struct gpio_dt_spec button_2 =
    GPIO_DT_SPEC_GET_OR(BUTTON_2, gpios, {0});

static const struct gpio_dt_spec button_3 =
    GPIO_DT_SPEC_GET_OR(BUTTON_3, gpios, {0});

static const struct gpio_dt_spec button_4 =
    GPIO_DT_SPEC_GET_OR(BUTTON_4, gpios, {0});


static struct gpio_callback button_0_data;
static struct gpio_callback button_1_data;
static struct gpio_callback button_2_data;
static struct gpio_callback button_3_data;
static struct gpio_callback button_4_data;


#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define UART_MSG_SIZE 32

struct uart_data {
    void *fifo_reserved;
    char msg[UART_MSG_SIZE];
};

struct light_command {
    void *fifo_reserved;
    char color;
    uint32_t duration;
};

K_FIFO_DEFINE(dispatcher_fifo);
K_FIFO_DEFINE(red_fifo);
K_FIFO_DEFINE(yellow_fifo);
K_FIFO_DEFINE(green_fifo);

K_MUTEX_DEFINE(red_mutex);
K_CONDVAR_DEFINE(red_signal);

K_MUTEX_DEFINE(yellow_mutex);
K_CONDVAR_DEFINE(yellow_signal);

K_MUTEX_DEFINE(green_mutex);
K_CONDVAR_DEFINE(green_signal);

K_SEM_DEFINE(release_sem, 0, 1);

#define MAX_SEQUENCE 256

static struct light_command sequence[MAX_SEQUENCE];
static int sequence_length = 0;
volatile bool manual_red = false;
volatile bool manual_yellow = false;
volatile bool manual_green = false;

#define STACKSIZE 1000
#define PRIORITY 5

void red_task(void *, void *, void *);
void yellow_task(void *, void *, void *);
void green_task(void *, void *, void *);
static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);
int init_buttons(void);

K_THREAD_DEFINE(red_tid,STACKSIZE,red_task,NULL,NULL,NULL,PRIORITY,0,0);

K_THREAD_DEFINE(yellow_tid,STACKSIZE,yellow_task,NULL,NULL,NULL,PRIORITY,0,0);

K_THREAD_DEFINE(green_tid,STACKSIZE,green_task,NULL,NULL,NULL,PRIORITY,0,0);

K_THREAD_DEFINE(uart_tid,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);

K_THREAD_DEFINE(dispatcher_tid, STACKSIZE, dispatcher_task, NULL, NULL, NULL,
        PRIORITY, 0, 0);

static void all_leds_off(void)
{
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&blue_led, 0);
}

static void red_on(void)
{
    gpio_pin_set_dt(&red_led, 1);
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&blue_led, 0);
}

static void yellow_on(void)
{
    gpio_pin_set_dt(&red_led, 1);
    gpio_pin_set_dt(&green_led, 1);
    gpio_pin_set_dt(&blue_led, 0);
}

static void green_on(void)
{
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 1);
    gpio_pin_set_dt(&blue_led, 0);
}


void button_0_handler(const struct device *dev,
    struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    printk("Button 1 - Play/Pause\n");
    static bool paused = false;
    paused = !paused;
    if (paused) {
        printk("Traffic lights paused\n");
        all_leds_off();
    } else {
        printk("Traffic lights resumed\n");
    }
}


void button_1_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    printk("Button 2 - Red\n");
    manual_red = !manual_red;
    manual_yellow = false;
    manual_green = false;
    if (manual_red) {
        red_on();
    } else {
        all_leds_off();
    }
}

void button_2_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    printk("Button 3 - Yellow\n");
    manual_yellow = !manual_yellow;
    manual_red = false;
    manual_green = false;
    if (manual_yellow) {
        yellow_on();
    } else {
        all_leds_off();
    }
}

void button_3_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    printk("Button 4 - Green\n");
    manual_green = !manual_green;
    manual_red = false;
    manual_yellow = false;
    if (manual_green) {
        green_on();
    } else {
        all_leds_off();
    }
}

void button_4_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    printk("Button 5 - Flashing yellow\n");
    manual_red = false;
    manual_yellow = false;
    manual_green = false;
    yellow_on();
}

int init_uart(void)
{
    if (!device_is_ready(uart_dev)) {
        printk("UART device is not ready\n");
        return -1;
    }
    printk("UART initialized\n");
    return 0;
}

static void uart_task(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);
    char rc = 0;
    char uart_msg[UART_MSG_SIZE];
    int uart_msg_cnt = 0;
    memset(uart_msg, 0, sizeof(uart_msg));
    while (true) {
        if (uart_poll_in(uart_dev, &rc) == 0) {
            if (rc == '\r' || rc == '\n') {
                if (uart_msg_cnt > 0) {
                    uart_msg[uart_msg_cnt] = '\0';
                    printk("UART msg: %s\n", uart_msg);
                    struct uart_data *buf =
                        k_malloc(sizeof(struct uart_data));
                    if (buf == NULL) {
                        printk("UART FIFO malloc failed\n");
                        uart_msg_cnt = 0;
                        memset(uart_msg, 0, sizeof(uart_msg));
                        continue;
                    }
                    strncpy(buf->msg, uart_msg,UART_MSG_SIZE - 1);
                    buf->msg[UART_MSG_SIZE - 1] = '\0';

                    k_fifo_put(&dispatcher_fifo, buf);
                    uart_msg_cnt = 0;

                    memset(uart_msg, 0, sizeof(uart_msg));
                }
            }
            else {
                if (uart_msg_cnt < UART_MSG_SIZE - 1) {
                    uart_msg[uart_msg_cnt] = rc;
                    uart_msg_cnt++;
                }
                else {
                    printk("UART message too long\n");
                    uart_msg_cnt = 0;
                    memset(uart_msg, 0, sizeof(uart_msg));
                }
            }
        }
        k_msleep(10);
    }
}

static bool parse_command(
    const char *msg,
    char *color,
    uint32_t *duration)
{
    if (msg[0] == 'T' && msg[1] == '\0') {
        return false;
    }
    if (sscanf(msg, "%c,%u", color, duration) == 2) {
        if (*color == 'R' ||
            *color == 'Y' ||
            *color == 'G') {
            return true;
        }
    }
    if ((msg[0] == 'R' ||
         msg[0] == 'Y' ||
         msg[0] == 'G') &&
        msg[1] == '\0') {
        *color = msg[0];
        *duration = 1000;
        return true;
    }
    return false;
}

static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);
    while (true) {
        struct uart_data *rec_item =
            k_fifo_get(&dispatcher_fifo, K_FOREVER);
        if (rec_item == NULL) {
            continue;
        }
        printk("Dispatcher received: %s\n", rec_item->msg);
        if (strcmp(rec_item->msg, "T") == 0) {
            printk("Dispatcher: repeat sequence\n");
            for (int i = 0;
                 i < sequence_length;
                 i++) {
                struct light_command *cmd =
                    k_malloc(sizeof(struct light_command));
                if (cmd == NULL) {
                    printk("Repeat command malloc failed");
                    break;
                }
                cmd->color = sequence[i].color;
                cmd->duration = sequence[i].duration;
                switch (cmd->color) {
                case 'R':
                    k_fifo_put(&red_fifo, cmd);
                    k_condvar_broadcast(&red_signal);
                    break;
                case 'Y':
                    k_fifo_put(&yellow_fifo, cmd);
                    k_condvar_broadcast(&yellow_signal);
                    break;
                case 'G':
                    k_fifo_put(&green_fifo, cmd);
                    k_condvar_broadcast(&green_signal);
                    break;
                }
                k_sem_take(&release_sem,K_FOREVER);
            }
            k_free(rec_item);
            continue;
        }
        char color = 0;
        uint32_t duration = 1000;
        if (!parse_command(rec_item->msg, &color,&duration)) {
            printk( "Invalid command: %s\n", rec_item->msg);
            k_free(rec_item);
            continue;
        }
        printk("Dispatcher: color=%c duration=%u ms\n", color, duration);
        if (sequence_length < MAX_SEQUENCE) {
            sequence[sequence_length].color = color;
            sequence[sequence_length].duration = duration;
            sequence_length++;
        }
        else {
            printk("Sequence buffer full\n");
        }
        struct light_command *cmd = k_malloc(
                sizeof(struct light_command));
        if (cmd == NULL) {
            printk("Light command malloc failed\n");
            k_free(rec_item);
            continue;
        }
        cmd->color = color;
        cmd->duration = duration;
        switch (color) {
        case 'R':
            k_fifo_put(&red_fifo, cmd);
            k_condvar_broadcast(&red_signal);
            break;
        case 'Y':
            k_fifo_put(&yellow_fifo, cmd);
            k_condvar_broadcast(&yellow_signal);
            break;
        case 'G':
            k_fifo_put(&green_fifo, cmd);
            k_condvar_broadcast(&green_signal);
            break;
        default:
            k_free(cmd);
            break;
        }
        if (color == 'R' ||
            color == 'Y' ||
            color == 'G') {
            k_sem_take(&release_sem,K_FOREVER);
        }
        k_free(rec_item);
    }
}

void red_task(
    void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);
    while (true) {
        k_mutex_lock(&red_mutex, K_FOREVER);
        struct light_command *cmd =
            k_fifo_get(&red_fifo, K_NO_WAIT);
        if (cmd == NULL) {
            k_condvar_wait(&red_signal, &red_mutex, K_FOREVER);
            cmd = k_fifo_get(&red_fifo, K_NO_WAIT);
        }
        k_mutex_unlock(&red_mutex);
        if (cmd == NULL) {
            continue;
        }
        printk("RED ON (%u ms)\n",
        cmd->duration);

        red_on();
        k_msleep(cmd->duration);
        all_leds_off();
        printk("RED OFF\n");
        k_sem_give(&release_sem);
        k_free(cmd);
    }
}

void yellow_task(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);
    while (true) {
        k_mutex_lock(&yellow_mutex, K_FOREVER);

        struct light_command *cmd =
            k_fifo_get(&yellow_fifo, K_NO_WAIT);

        if (cmd == NULL) {
            k_condvar_wait(&yellow_signal, &yellow_mutex, K_FOREVER);

            cmd = k_fifo_get(&yellow_fifo, K_NO_WAIT);
        }
        k_mutex_unlock(
            &yellow_mutex);
        if (cmd == NULL) {
            continue;
        }

        printk("YELLOW ON (%u ms)\n", cmd->duration);

        yellow_on();
        k_msleep(cmd->duration);
        all_leds_off();
        printk("YELLOW OFF\n");
        k_sem_give(&release_sem);
        k_free(cmd);
    }
}



void green_task(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);
    while (true) {
        k_mutex_lock(&green_mutex, K_FOREVER);

        struct light_command *cmd =
            k_fifo_get(&green_fifo,K_NO_WAIT);

        if (cmd == NULL) {
            k_condvar_wait(&green_signal, &green_mutex, K_FOREVER);
            cmd = k_fifo_get(&green_fifo, K_NO_WAIT);
        }
        k_mutex_unlock(&green_mutex);
        if (cmd == NULL) {
            continue;
        }

        printk("GREEN ON (%u ms)\n",
        cmd->duration);
        green_on();
        k_msleep(cmd->duration);
        all_leds_off();
        printk("GREEN OFF\n");
        k_sem_give(&release_sem);
        k_free(cmd);
    }
}


int init_buttons(void)
{
    int ret;
    if (!gpio_is_ready_dt(&button_0)) {
        printk("Error: button 0 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&button_0,GPIO_INPUT);
    if (ret != 0) {
        printk("Error: button 0 configure failed\n");
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
    gpio_add_callback(button_0.port, &button_0_data);
    if (!gpio_is_ready_dt(&button_1)) {
        printk("Error: button 1 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&button_1, GPIO_INPUT);
    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(&button_1, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(&button_1_data, button_1_handler, BIT(button_1.pin));
    gpio_add_callback(button_1.port,&button_1_data);
    if (!gpio_is_ready_dt(&button_2)) {
        printk("Error: button 2 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&button_2, GPIO_INPUT);
    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(&button_2, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(&button_2_data,button_2_handler, BIT(button_2.pin));
    gpio_add_callback(button_2.port,&button_2_data);
    if (!gpio_is_ready_dt(&button_3)) {
        printk("Error: button 3 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&button_3,GPIO_INPUT);

    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(&button_3, GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(&button_3_data, button_3_handler, BIT(button_3.pin));

    gpio_add_callback(button_3.port, &button_3_data);



    if (!gpio_is_ready_dt(&button_4)) {
        printk("Error: button 4 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&button_4, GPIO_INPUT);
    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(&button_4, GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(&button_4_data, button_4_handler, BIT(button_4.pin));
    gpio_add_callback(button_4.port, &button_4_data);
    printk("All buttons initialized\n");
    return 0;
}

int main(void)
{
    int ret;
    if (!gpio_is_ready_dt(&red_led) ||
        !gpio_is_ready_dt(&green_led) ||
        !gpio_is_ready_dt(&blue_led)) {
        printk("RGB2 LED is not ready\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("Red LED configuration failed\n");
        return 0;
    }
    ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("Green LED configuration failed\n");
        return 0;
    }
    ret = gpio_pin_configure_dt(&blue_led,
        GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("Blue LED configuration failed\n");
        return 0;
    }
    all_leds_off();
    ret = init_uart();
    if (ret != 0) {
        printk("UART initialization failed\n");
        return 0;
    }
    ret = init_buttons();
    if (ret < 0) {
        printk("Button initialization failed\n");
        return 0;
    }
    printk("\n");
    printk("==============================\n");
    printk("RGB2 traffic light started\n");
    printk("==============================\n");
    printk("Send commands through UART:\n");
    printk("R,1000\n");
    printk("Y,500\n");
    printk("G,1000\n");
    printk("T\n");
    printk("==============================\n");

    return 0;
}