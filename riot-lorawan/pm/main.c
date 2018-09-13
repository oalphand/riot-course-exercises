/*
 * Copyright (C) 2018 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <string.h>

#include "net/loramac.h"
#include "semtech_loramac.h"

#include "hts221.h"
#include "hts221_params.h"

#include "cayenne_lpp.h"

#include "board.h"

/* Messages are sent every 20s to respect the duty cycle on each channel */
#define PERIOD              (20U)

/* Declare globally the loramac descriptor */
static semtech_loramac_t loramac;

/* Declare globally the sensor device descriptor */
static hts221_t hts221;

/* Declare globally Cayenne LPP descriptor */
static cayenne_lpp_t lpp;

/* Device and application informations required for OTAA activation */
static const uint8_t deveui[LORAMAC_DEVEUI_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t appeui[LORAMAC_APPEUI_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t appkey[LORAMAC_APPKEY_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static void sender(void)
{
    msg_t msg;
    msg_t msg_queue[8];
    msg_init_queue(msg_queue, 8);

    while (1) {
        /* do some measurements */
        uint16_t humidity = 0;
        int16_t temperature = 0;
        if (hts221_read_humidity(&hts221, &humidity) != HTS221_OK) {
            puts(" -- failed to read humidity!");
        }
        if (hts221_read_temperature(&hts221, &temperature) != HTS221_OK) {
            puts(" -- failed to read temperature!");
        }

        /* prepare cayenne lpp payload */
        cayenne_lpp_add_temperature(&lpp, 0, (float)temperature / 10);
        cayenne_lpp_add_relative_humidity(&lpp, 1, (float)humidity / 10);

        printf("Sending LPP data\n");

        /* send the LoRaWAN message */
        semtech_loramac_send(&loramac, lpp.buffer, lpp.cursor);

        /* wait for any potential received data */
        semtech_loramac_recv(&loramac);

        /* clear lpp buffer once done */
        cayenne_lpp_reset(&lpp);

        /* schedule the next wake-up alarm */

        /* switch to low-power mode */

        /* waiting for IPC message from wake-up alarm */
        msg_receive(&msg);
    }

    /* this should never be reached */
    return;
}

int main(void)
{
    if (hts221_init(&hts221, &hts221_params[0]) != HTS221_OK) {
        puts("Sensor initialization failed");
        return 1;
    }
    if (hts221_power_on(&hts221) != HTS221_OK) {
        puts("Sensor initialization power on failed");
        return 1;
    }
    if (hts221_set_rate(&hts221, hts221.p.rate) != HTS221_OK) {
        puts("Sensor continuous mode setup failed");
        return 1;
    }

    /* initialize the loramac stack */
    semtech_loramac_init(&loramac);

    /* use a fast datarate so we don't use the physical layer too much */
    semtech_loramac_set_dr(&loramac, 5);

    /* set the LoRaWAN keys */
    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);

    /* start the OTAA join procedure */
    puts("Starting join procedure");
    if (semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA) != SEMTECH_LORAMAC_JOIN_SUCCEEDED) {
        puts("Join procedure failed");
        return 1;
    }

    puts("Join procedure succeeded");

    /* call the sender */
    sender();

    return 0;
}
