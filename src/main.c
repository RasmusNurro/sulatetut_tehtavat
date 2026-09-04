// *****************************************************
// Liikennevalot RGB2-ledillä ja taskien avulla
//
// Tavoite: 1 piste
// Toteutettu: kolme LED-taskia ja FSM
//
// Tilat:
// 0 = punainen
// 1 = keltainen
// 2 = vihreä
// *****************************************************

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

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
// Tilakone
// =====================================================
//
// 0 = punainen
// 1 = keltainen
// 2 = vihreä

volatile int led_state = 0;


// =====================================================
// Taskien määritykset
// =====================================================

void red_task(void *, void *, void *);
void yellow_task(void *, void *, void *);
void green_task(void *, void *, void *);

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


// =====================================================
// Punainen task
// =====================================================

void red_task(void *, void *, void *)
{
    while (true) {

        if (led_state == 0) {

            // Punainen päälle
            gpio_pin_set_dt(&red_led, 1);

            // Muut poies
            
            gpio_pin_set_dt(&green_led, 0);
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti päällä
            k_msleep(1000);

            // Punainen pois
            gpio_pin_set_dt(&red_led, 0);

            // Seuraava tila = keltainen
            led_state = 1;
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

            // Sininen pois päälle
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti päällä
            k_msleep(1000);

            // Keltainen pois
            gpio_pin_set_dt(&red_led, 0);
            gpio_pin_set_dt(&green_led, 0);

            // Seuraava tila = vihreä
            led_state = 2;
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

            // Muut värit pois
            gpio_pin_set_dt(&red_led, 0);
            gpio_pin_set_dt(&blue_led, 0);

            // Sekunti päällä
            k_msleep(1000);

            // Vihreä pois
            gpio_pin_set_dt(&green_led, 0);

            // Seuraava tila = punainen
            led_state = 0;
        }

        k_msleep(10);
    }
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

    // Konfiguroidaan LEDit ulostuloiksi
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

    ret = gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        printk("Blue LED configuration failed\n");
        return 0;
    }

    // Kaikki aluksi pois
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&blue_led, 0);

    printk("RGB2 traffic light started\n");

    // Main ei tee varsinaista liikennevalojen työtä.
    // Kolme taskia suorittavat FSM:n.

    while (true) {
        k_msleep(1000);
    }

    return 0;
}