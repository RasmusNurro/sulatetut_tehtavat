// *****************************************************
// Liikennevalot RGB2-ledillä ja taskien avulla
//
// Tavoite: 3 pistettä
// Tilat:
// 0 = punainen
// 1 = keltainen
// 2 = vihreä
// 4 = pause
// 5 = vilkkuva keltainen
//
// Nappien oletus:
// sw0 = nappi 1 = Play/Pause
// sw1 = nappi 2 = punainen
// sw2 = nappi 3 = keltainen
// sw3 = nappi 4 = vihreä
// sw4 = nappi 5 = vilkkuva keltainen

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>


// RGB2 LED -määritykset

#define RED_LED   DT_ALIAS(led3)
#define GREEN_LED DT_ALIAS(led4)
#define BLUE_LED  DT_ALIAS(led5)

static const struct gpio_dt_spec red_led =
    GPIO_DT_SPEC_GET(RED_LED, gpios);

static const struct gpio_dt_spec green_led =
    GPIO_DT_SPEC_GET(GREEN_LED, gpios);

static const struct gpio_dt_spec blue_led =
    GPIO_DT_SPEC_GET(BLUE_LED, gpios);


// Painikkeiden määritykset

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


// Callback-rakenteet
static struct gpio_callback button_0_data;
static struct gpio_callback button_1_data;
static struct gpio_callback button_2_data;
static struct gpio_callback button_3_data;
static struct gpio_callback button_4_data;


// Tilakone
// 0 = punainen
// 1 = keltainen
// 2 = vihreä
// 4 = pause
// 5 = vilkkuva keltainen

volatile int led_state = 0;

// Tallennetaan tila ennen pausea
volatile int previous_state = 0;

// Käsiohjauksen tilat
volatile bool manual_red = false;
volatile bool manual_yellow = false;
volatile bool manual_green = false;


// Taskien määritykset

void red_task(void *, void *, void *);
void yellow_task(void *, void *, void *);
void green_task(void *, void *, void *);

int init_buttons(void);


// Taskien asetukset

#define STACKSIZE 500
#define PRIORITY 5


K_THREAD_DEFINE(red_tid,
                STACKSIZE,
                red_task,
                NULL, NULL, NULL,
                PRIORITY, 0, 0);

K_THREAD_DEFINE(yellow_tid,
                STACKSIZE,
                yellow_task,
                NULL, NULL, NULL,
                PRIORITY, 0, 0);

K_THREAD_DEFINE(green_tid,
                STACKSIZE,
                green_task,
                NULL, NULL, NULL,
                PRIORITY, 0, 0);


// Nappi 1 - Play/Pause

void button_0_handler(const struct device *dev,
                      struct gpio_callback *cb,
                      uint32_t pins)
{
    printk("Button 1 - Play/Pause\n");

    if (led_state != 4) {

        // Tallennetaan nykyinen tila
        previous_state = led_state;

        // Siirrytään pauseen
        led_state = 4;

        // Sammutetaan automaattinen valo
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
        gpio_pin_set_dt(&blue_led, 0);
    }

    else {

        // Palataan aikaisempaan tilaan
        led_state = previous_state;
    }
}


// Nappi 2 - punainen käsiohjaus

void button_1_handler(const struct device *dev,
                      struct gpio_callback *cb,
                      uint32_t pins)
{
    printk("Button 2 - Red\n");

    led_state = 4;

    manual_red = !manual_red;

    // Muut käsiohjaukset pois
    manual_yellow = false;
    manual_green = false;

    if (manual_red) {

        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 0);
        gpio_pin_set_dt(&blue_led, 0);

    } else {

        gpio_pin_set_dt(&red_led, 0);
    }
}

// Nappi 3 - keltainen käsiohjaus

void button_2_handler(const struct device *dev,
                      struct gpio_callback *cb,
                      uint32_t pins)
{
    printk("Button 3 - Yellow\n");

    led_state = 4;

    manual_yellow = !manual_yellow;

    // Muut käsiohjaukset pois
    manual_red = false;
    manual_green = false;

    if (manual_yellow) {

        // Keltainen = punainen + vihreä
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 1);
        gpio_pin_set_dt(&blue_led, 0);

    } else {

        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
    }
}


// Nappi 4 - vihreä käsiohjaus

void button_3_handler(const struct device *dev,
                      struct gpio_callback *cb,
                      uint32_t pins)
{
    printk("Button 4 - Green\n");

    led_state = 4;

    manual_green = !manual_green;

    // Muut käsiohjaukset pois
    manual_red = false;
    manual_yellow = false;

    if (manual_green) {

        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 1);
        gpio_pin_set_dt(&blue_led, 0);

    } else {

        gpio_pin_set_dt(&green_led, 0);
    }
}

void button_4_handler(const struct device *dev,
                      struct gpio_callback *cb,
                      uint32_t pins)
{
    printk("Button 5 - Flashing yellow\n");

    // Jos vilkkuva keltainen ei ole päällä,
    // käynnistetään se.
    if (led_state != 6) {

        previous_state = led_state;
        led_state = 6;

        // Sammutetaan muut valot
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
        gpio_pin_set_dt(&blue_led, 0);
    }

    // Jos vilkkuva keltainen on päällä,
    // palataan aikaisempaan tilaan.
    else {

        led_state = previous_state;

        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
        gpio_pin_set_dt(&blue_led, 0);
    }
}


// Punainen task

void red_task(void *, void *, void *)
{
    while (true) {

        if (led_state == 0) {

            // Punainen päälle
            gpio_pin_set_dt(&red_led, 1);

            // Muut pois
            gpio_pin_set_dt(&green_led, 0);
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti
            k_msleep(1000);

            // Jos pausea ei ole painettu
            if (led_state == 0) {

                gpio_pin_set_dt(&red_led, 0);

                // Seuraava tila
                led_state = 1;
            }
        }

        k_msleep(10);
    }
}

void yellow_task(void *, void *, void *)
{
    while (true) {

        // Normaali keltainen
        if (led_state == 1) {

            // Keltainen = punainen + vihreä
            gpio_pin_set_dt(&red_led, 1);
            gpio_pin_set_dt(&green_led, 1);
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti
            k_msleep(1000);

            // Jos pausea ei ole painettu
            if (led_state == 1) {

                gpio_pin_set_dt(&red_led, 0);
                gpio_pin_set_dt(&green_led, 0);

                // Seuraava tila
                led_state = 2;
            }
        }

        // Vilkkuva keltainen

        if (led_state == 6) {

            // Keltainen päälle
            gpio_pin_set_dt(&red_led, 1);
            gpio_pin_set_dt(&green_led, 1);
            gpio_pin_set_dt(&blue_led, 0);

            k_msleep(500);

            // Tarkistetaan että tila on edelleen 6
            if (led_state == 6) {

                // Keltainen pois
                gpio_pin_set_dt(&red_led, 0);
                gpio_pin_set_dt(&green_led, 0);

                k_msleep(500);
            }
        }

        k_msleep(10);
    }
}

void green_task(void *, void *, void *)
{
    while (true) {

        if (led_state == 2) {

            // Vihreä päälle
            gpio_pin_set_dt(&green_led, 1);

            // Muut pois
            gpio_pin_set_dt(&red_led, 0);
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti
            k_msleep(1000);

            // Jos pausea ei ole painettu
            if (led_state == 2) {

                gpio_pin_set_dt(&green_led, 0);

                // Seuraava tila
                led_state = 0;
            }
        }

        k_msleep(10);
    }
}


// Painikkeiden alustaminen

int init_buttons(void)
{
    int ret;

    // Button 0
    if (!gpio_is_ready_dt(&button_0)) {

        printk("Error: button 0 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(
        &button_0,
        GPIO_INPUT);

    if (ret != 0) {

        printk("Error: button 0 configure failed\n");
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button_0,
        GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {

        printk("Error: button 0 interrupt failed\n");
        return -1;
    }

    gpio_init_callback(
        &button_0_data,
        button_0_handler,
        BIT(button_0.pin));

    gpio_add_callback(
        button_0.port,
        &button_0_data);


    // Button 1

    if (!gpio_is_ready_dt(&button_1)) {

        printk("Error: button 1 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(
        &button_1,
        GPIO_INPUT);

    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button_1,
        GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(
        &button_1_data,
        button_1_handler,
        BIT(button_1.pin));

    gpio_add_callback(
        button_1.port,
        &button_1_data);


    // Button 2

    if (!gpio_is_ready_dt(&button_2)) {

        printk("Error: button 2 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(
        &button_2,
        GPIO_INPUT);

    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button_2,
        GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(
        &button_2_data,
        button_2_handler,
        BIT(button_2.pin));

    gpio_add_callback(
        button_2.port,
        &button_2_data);


    // Button 3

    if (!gpio_is_ready_dt(&button_3)) {

        printk("Error: button 3 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(
        &button_3,
        GPIO_INPUT);

    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button_3,
        GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(
        &button_3_data,
        button_3_handler,
        BIT(button_3.pin));

    gpio_add_callback(
        button_3.port,
        &button_3_data);


    // Button 4

    if (!gpio_is_ready_dt(&button_4)) {

        printk("Error: button 4 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(
        &button_4,
        GPIO_INPUT);

    if (ret != 0) {
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button_4,
        GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {
        return -1;
    }

    gpio_init_callback(
        &button_4_data,
        button_4_handler,
        BIT(button_4.pin));

    gpio_add_callback(
        button_4.port,
        &button_4_data);


    printk("All buttons initialized\n");

    return 0;
}


// Main

int main(void)
{
    int ret;


    // Tarkistetaan LEDit

    if (!gpio_is_ready_dt(&red_led) ||
        !gpio_is_ready_dt(&green_led) ||
        !gpio_is_ready_dt(&blue_led)) {

        printk("RGB2 LED is not ready\n");
        return 0;
    }


    // LEDien konfigurointi

    ret = gpio_pin_configure_dt(
        &red_led,
        GPIO_OUTPUT_INACTIVE);

    if (ret != 0) {

        printk("Red LED configuration failed\n");
        return 0;
    }


    ret = gpio_pin_configure_dt(
        &green_led,
        GPIO_OUTPUT_INACTIVE);

    if (ret != 0) {

        printk("Green LED configuration failed\n");
        return 0;
    }


    ret = gpio_pin_configure_dt(
        &blue_led,
        GPIO_OUTPUT_INACTIVE);

    if (ret != 0) {

        printk("Blue LED configuration failed\n");
        return 0;
    }


    // Kaikki LEDit pois
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&blue_led, 0);


    // Painikkeet

    ret = init_buttons();

    if (ret < 0) {

        printk("Button initialization failed\n");
        return 0;
    }


    printk("RGB2 traffic light started\n");


    // Main loop

    while (true) {

        k_msleep(1000);
    }

    return 0;
}