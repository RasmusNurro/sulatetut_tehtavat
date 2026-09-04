// *****************************************************
// Liikennevalot RGB2-ledillä ja taskien avulla
//
// Tavoite: 2 pistettä
// Toteutettu:
// - kolme LED-taskia
// - FSM
// - painonapin keskeytys
// - pause-toiminto
//
// Tilat:
// 0 = punainen
// 1 = keltainen
// 2 = vihreä
// 4 = pause
// *****************************************************

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>

// =====================================================
// RGB2 LED -määritykset
// =====================================================

#define RED_LED   DT_ALIAS(led3)
#define GREEN_LED DT_ALIAS(led4)
#define BLUE_LED  DT_ALIAS(led5)

static const struct gpio_dt_spec red_led =
    GPIO_DT_SPEC_GET(RED_LED, gpios);

static const struct gpio_dt_spec green_led =
    GPIO_DT_SPEC_GET(GREEN_LED, gpios);

static const struct gpio_dt_spec blue_led =
    GPIO_DT_SPEC_GET(BLUE_LED, gpios);


// =====================================================
// Painonappi
// =====================================================

#define BUTTON_0 DT_ALIAS(sw2)

static const struct gpio_dt_spec button_0 =
    GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});

static struct gpio_callback button_0_data;


// =====================================================
// Tilakone
// =====================================================
//
// 0 = punainen
// 1 = keltainen
// 2 = vihreä
// 4 = pause

volatile int led_state = 0;

// Tänne tallennetaan tila ennen pausea
volatile int previous_state = 0;


// =====================================================
// Taskien määritykset
// =====================================================

void red_task(void *, void *, void *);
void yellow_task(void *, void *, void *);
void green_task(void *, void *, void *);

int init_button(void);

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

// Painonapin keskeytyskäsittelijä
void button_0_handler(const struct device *dev,
                      struct gpio_callback *cb,
                      uint32_t pins)
{
    printk("Button pressed\n");

    // Jos ei olla paussilla niin tallennetaan nykyinen tila ja mennään pauseen.
    if (led_state != 4) {

        previous_state = led_state;
        led_state = 4;
    }

    // Jos ollaan jo paussilla niin palataan aikaisempaan tilaan.
    else {

        led_state = previous_state;
    }
}


// =====================================================
// Punainen task
// =====================================================

void red_task(void *, void *, void *)
{
    while (true) {

        if (led_state == 0) {

            // Punainen päälle
            gpio_pin_set_dt(&red_led, 1);

            // Muut pois
            gpio_pin_set_dt(&green_led, 0);
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti päällä
            k_msleep(1000);

            // Tarkistetaan että pausea ei ole
            // aktivoitu unen aikana.
            if (led_state == 0) {

                // Punainen pois
                gpio_pin_set_dt(&red_led, 0);

                // Seuraava tila = keltainen
                led_state = 1;
            }
        }

        k_msleep(10);
    }
}


// =====================================================
// Keltainen task
// =====================================================
//
// Keltainen = punainen + vihreä
// =====================================================

void yellow_task(void *, void *, void *)
{
    while (true) {

        if (led_state == 1) {

            // Keltainen päälle
            gpio_pin_set_dt(&red_led, 1);
            gpio_pin_set_dt(&green_led, 1);

            // Sininen pois
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti päällä
            k_msleep(1000);

            // Tarkistetaan että pausea ei ole
            // aktivoitu unen aikana.
            if (led_state == 1) {

                // Keltainen pois
                gpio_pin_set_dt(&red_led, 0);
                gpio_pin_set_dt(&green_led, 0);

                // Seuraava tila = vihreä
                led_state = 2;
            }
        }

        k_msleep(10);
    }
}


// =====================================================
// Vihreä task
// =====================================================

void green_task(void *, void *, void *)
{
    while (true) {

        if (led_state == 2) {

            // Vihreä päälle
            gpio_pin_set_dt(&green_led, 1);

            // Muut pois
            gpio_pin_set_dt(&red_led, 0);
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti päällä
            k_msleep(1000);

            // Tarkistetaan että pausea ei ole
            // aktivoitu unen aikana.
            if (led_state == 2) {

                // Vihreä pois
                gpio_pin_set_dt(&green_led, 0);

                // Seuraava tila = punainen
                led_state = 0;
            }
        }

        k_msleep(10);
    }
}


// =====================================================
// Painonapin alustaminen
// =====================================================

int init_button(void)
{
    int ret;

    if (!gpio_is_ready_dt(&button_0)) {

        printk("Error: button 0 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);

    if (ret != 0) {

        printk("Error: failed to configure pin\n");
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button_0,
        GPIO_INT_EDGE_TO_ACTIVE);

    if (ret != 0) {

        printk("Error: failed to configure interrupt on pin\n");
        return -1;
    }

    gpio_init_callback(
        &button_0_data,
        button_0_handler,
        BIT(button_0.pin));

    gpio_add_callback(
        button_0.port,
        &button_0_data);

    printk("Set up button 0 ok\n");

    return 0;
}


// =====================================================
// Main
// =====================================================

int main(void)
{
    int ret;

    // Tarkistetaan että LEDit ovat käytettävissä
    if (!gpio_is_ready_dt(&red_led) ||
        !gpio_is_ready_dt(&green_led) ||
        !gpio_is_ready_dt(&blue_led)) {

        printk("RGB2 LED is not ready\n");
        return 0;
    }


    // =================================================
    // LEDien konfigurointi
    // =================================================

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


    // Kaikki aluksi pois
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&blue_led, 0);


    // =================================================
    // Painonapin alustaminen
    // =================================================

    ret = init_button();

    if (ret < 0) {
        return 0;
    }


    printk("RGB2 traffic light started\n");


    // Main ei tee varsinaista liikennevalojen työtä.
    // Kolme taskia suorittavat FSM:n.

    while (true) {

        k_msleep(1000);
    }

    return 0;
}