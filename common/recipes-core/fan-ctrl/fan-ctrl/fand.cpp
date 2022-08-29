/*
 * fand
 *
 * Copyright 2014-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Daemon to manage the fan speed to ensure that we stay within a reasonable
 * temperature range.  We're using a simplistic algorithm to get started:
 *
 * If the fan is already on high, we'll move it to medium if we fall below
 * a top temperature.  If we're on medium, we'll move it to high
 * if the temperature goes over the top value, and to low if the
 * temperature falls to a bottom level.  If the fan is on low,
 * we'll increase the speed if the temperature rises to the top level.
 *
 * To ensure that we're not just turning the fans up, then back down again,
 * we'll require an extra few degrees of temperature drop before we lower
 * the fan speed.
 *
 * We check the RPM of the fans against the requested RPMs to determine
 * whether the fans are failing, in which case we'll turn up all of
 * the other fans and report the problem..
 *
 * TODO:  Implement a PID algorithm to closely track the ideal temperature.
 * TODO:  Determine if the daemon is already started.
 */

/* Yeah, the file ends in .cpp, but it's a C program.  Deal. */

/* XXX:  Both CONFIG_WEDGE and CONFIG_WEDGE100 are defined for Wedge100 */
/* XXX:  Both CONFIG_WEDGE and CONFIG_MAVERICKS are defined for MAVERICKS */

#if !defined(CONFIG_YOSEMITE) && !defined(CONFIG_WEDGE) && \
    !defined(CONFIG_WEDGE100) && !defined(CONFIG_MAVERICKS) && \
    !defined(CONFIG_LIGHTNING)
#error "No hardware platform defined!"
#endif
#if defined(CONFIG_YOSEMITE) && defined(CONFIG_WEDGE) && defined(CONFIG_LIGHTNING)
#error "Two hardware platforms defined!"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#if defined(CONFIG_YOSEMITE)
#include <openbmc/ipmi.h>
#include <facebook/bic.h>
#include <facebook/yosemite_sensor.h>
#endif
#if defined(CONFIG_WEDGE) && !defined(CONFIG_WEDGE100)
#include <facebook/wedge_eeprom.h>
#endif

#if defined(CONFIG_MAVERICKS)
#include <fcntl.h>
#include <facebook/i2c-dev.h>
#endif

#include "watchdog.h"

#define USERVER_ERROR_THRESHOLD INTERNAL_TEMPS(120)

#if !defined(CONFIG_LIGHTNING)
/* Sensor definitions */

#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
#define INTERNAL_TEMPS(x) ((x) * 1000) // stored as C * 1000
#define EXTERNAL_TEMPS(x) ((x) / 1000)
#define WEDGE100_COME_DIMM 100000 // 100C
#elif defined(CONFIG_YOSEMITE)
#define INTERNAL_TEMPS(x) (x)
#define EXTERNAL_TEMPS(x) (x)
#define TOTAL_1S_SERVERS 4
#endif

/*
 * The sensor for the uServer CPU is not on the CPU itself, so it reads
 * a little low.  We are special casing this, but we should obviously
 * be thinking about a way to generalize these tweaks, and perhaps
 * the entire configuration.  JSON file?
 */

#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
#define USERVER_TEMP_FUDGE INTERNAL_TEMPS(10)
#else
#define USERVER_TEMP_FUDGE INTERNAL_TEMPS(0)
#endif

#define BAD_TEMP INTERNAL_TEMPS(-60)

#define OVER_NORMAL_TEMP_CHANGE INTERNAL_TEMPS(50)

#define BAD_READ_THRESHOLD 4    /* How many times can reads fail */
#define FAN_FAILURE_THRESHOLD 4 /* How many times can a fan fail */
#define FAN_SHUTDOWN_THRESHOLD 20 /* How long fans can be failed before */
                                  /* we just shut down the whole thing. */

#if defined(CONFIG_WEDGE100)
#define PWM_DIR "/sys/bus/i2c/drivers/fancpld/8-0033/"

#define PWM_UNIT_MAX 31

#define LM75_DIR "/sys/bus/i2c/drivers/lm75/"
#define COM_E_DIR "/sys/bus/i2c/drivers/com_e_driver/"

#define INTAKE_TEMP_DEVICE LM75_DIR "3-0048"
#define CHIP_TEMP_DEVICE LM75_DIR "3-004b"
#define EXHAUST_TEMP_DEVICE LM75_DIR "3-0048"
#define USERVER_TEMP_DEVICE COM_E_DIR "4-0033"

#define FAN_READ_RPM_FORMAT "fan%d_input"

#define FAN0_LED PWM_DIR "fantray1_led_ctrl"
#define FAN1_LED PWM_DIR "fantray2_led_ctrl"
#define FAN2_LED PWM_DIR "fantray3_led_ctrl"
#define FAN3_LED PWM_DIR "fantray4_led_ctrl"
#define FAN4_LED PWM_DIR "fantray5_led_ctrl"

#define FAN_LED_BLUE "0x1"
#define FAN_LED_RED "0x2"

#define MAIN_POWER "/sys/bus/i2c/drivers/syscpld/12-0031/pwr_main_n"
#define USERVER_POWER "/sys/bus/i2c/drivers/syscpld/12-0031/pwr_usrv_en"

#elif defined(CONFIG_MAVERICKS)
#define PWM_DIR "/sys/bus/i2c/drivers/fancpld/8-0033/"
#define PWM_UPPER_DIR "/sys/bus/i2c/drivers/fancpld/9-0033/"
/*For Newport*/
#define PWM_DIR_NEWPORT "/sys/bus/i2c/drivers/fancpld/8-0066/"

/* Convert the percentage to our 1/32th unit (0-31). */
#define PWM_UNIT_MAX 31
/*For Newport, Convert the percentage to our 1/16th unit (0-15). */
#define PWM_UNIT_MAX_NEWPORT 16

#define LM75_DIR "/sys/bus/i2c/drivers/lm75/"
#define COM_E_DIR "/sys/bus/i2c/drivers/com_e_driver/"

#define INTAKE_TEMP_DEVICE LM75_DIR "3-004b"
#define CHIP_TEMP_DEVICE LM75_DIR "3-0048"
#define EXHAUST_TEMP_DEVICE LM75_DIR "3-004a"
#define USERVER_TEMP_DEVICE COM_E_DIR "4-0033"

#define FAN_READ_RPM_FORMAT "fan%d_input"

#define FAN0_LED PWM_DIR "fantray1_led_ctrl"
#define FAN1_LED PWM_DIR "fantray2_led_ctrl"
#define FAN2_LED PWM_DIR "fantray3_led_ctrl"
#define FAN3_LED PWM_DIR "fantray4_led_ctrl"
#define FAN4_LED PWM_DIR "fantray5_led_ctrl"
#define FAN5_LED PWM_UPPER_DIR "fantray1_led_ctrl"
#define FAN6_LED PWM_UPPER_DIR "fantray2_led_ctrl"
#define FAN7_LED PWM_UPPER_DIR "fantray3_led_ctrl"
#define FAN8_LED PWM_UPPER_DIR "fantray4_led_ctrl"
#define FAN9_LED PWM_UPPER_DIR "fantray5_led_ctrl"
/*For Newport*/
#define FAN_LED_DEBUG_MODE PWM_DIR_NEWPORT "led_debug_mode"
#define FAN0_LED_R PWM_DIR_NEWPORT "fantray1_led_r"
#define FAN0_LED_G PWM_DIR_NEWPORT "fantray1_led_g"
#define FAN1_LED_R PWM_DIR_NEWPORT "fantray2_led_r"
#define FAN1_LED_G PWM_DIR_NEWPORT "fantray2_led_g"
#define FAN2_LED_R PWM_DIR_NEWPORT "fantray3_led_r"
#define FAN2_LED_G PWM_DIR_NEWPORT "fantray3_led_g"
#define FAN3_LED_R PWM_DIR_NEWPORT "fantray4_led_r"
#define FAN3_LED_G PWM_DIR_NEWPORT "fantray4_led_g"
#define FAN4_LED_R PWM_DIR_NEWPORT "fantray5_led_r"
#define FAN4_LED_G PWM_DIR_NEWPORT "fantray5_led_g"
#define FAN5_LED_R PWM_DIR_NEWPORT "fantray6_led_r"
#define FAN5_LED_G PWM_DIR_NEWPORT "fantray6_led_g"
/* additional (only) Stinson specific */
#define FAN6_LED_R PWM_DIR_NEWPORT "fantray7_led_r"
#define FAN6_LED_G PWM_DIR_NEWPORT "fantray7_led_g"

#define FAN_LED_BLUE "0x1"
#define FAN_LED_RED "0x2"

/*For Newport*/
#define FAN_LED_ON "0x0"
#define FAN_LED_OFF "0x1"

#define MAIN_POWER "/sys/bus/i2c/drivers/syscpld/12-0031/pwr_main_n"
#define USERVER_POWER "/sys/bus/i2c/drivers/syscpld/12-0031/pwr_usrv_en"

#elif defined(CONFIG_WEDGE)
#define I2C_BUS_3_DIR "/sys/class/i2c-adapter/i2c-3/"
#define I2C_BUS_4_DIR "/sys/class/i2c-adapter/i2c-4/"

#define INTAKE_TEMP_DEVICE I2C_BUS_3_DIR "3-0048"
#define CHIP_TEMP_DEVICE I2C_BUS_3_DIR "3-0049"
#define EXHAUST_TEMP_DEVICE I2C_BUS_3_DIR "3-004a"
#define USERVER_TEMP_DEVICE I2C_BUS_4_DIR "4-0040"


#define FAN0_LED "/sys/class/gpio/gpio53/value"
#define FAN1_LED "/sys/class/gpio/gpio54/value"
#define FAN2_LED "/sys/class/gpio/gpio55/value"
#define FAN3_LED "/sys/class/gpio/gpio72/value"

#define FAN_LED_RED "0"
#define FAN_LED_BLUE "1"

#define GPIO_PIN_ID "/sys/class/gpio/gpio%d/value"
#define REV_IDS 3
#define GPIO_REV_ID_START 192

#define BOARD_IDS 4
#define GPIO_BOARD_ID_START 160

/*
 * With hardware revisions after 3, we use a different set of pins for
 * the BOARD_ID.
 */

#define REV_ID_NEW_BOARD_ID 3
#define GPIO_BOARD_ID_START_NEW 166

#elif defined(CONFIG_YOSEMITE)
#define FAN_LED_RED "0"
#define FAN_LED_BLUE "1"
#endif

#if (defined(CONFIG_YOSEMITE) || defined(CONFIG_WEDGE)) && \
    !defined(CONFIG_WEDGE100) && !defined(CONFIG_MAVERICKS)
#define PWM_DIR "/sys/devices/platform/ast_pwm_tacho.0"

#define PWM_UNIT_MAX 96
#define FAN_READ_RPM_FORMAT "tacho%d_rpm"

#define GPIO_USERVER_POWER_DIRECTION "/sys/class/gpio/gpio25/direction"
#define GPIO_USERVER_POWER "/sys/class/gpio/gpio25/value"
#define GPIO_T2_POWER_DIRECTION "/tmp/gpionames/T2_POWER_UP/direction"
#define GPIO_T2_POWER "/tmp/gpionames/T2_POWER_UP/value"
#endif

#define LARGEST_DEVICE_NAME 120

#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
const char *fan_led[] = {FAN0_LED, FAN1_LED, FAN2_LED, FAN3_LED,
#if defined(CONFIG_WEDGE100) || defined(CONFIG_MAVERICKS)
                         FAN4_LED,
#endif
#if defined(CONFIG_MAVERICKS)
                         FAN5_LED, FAN6_LED, FAN7_LED, FAN8_LED, FAN9_LED,
#endif
};
/*For Newports 0-5. additional 6 is for Stinson */
const char *fan_led_r[] = {FAN0_LED_R, FAN1_LED_R, FAN2_LED_R, FAN3_LED_R,
                           FAN4_LED_R, FAN5_LED_R, FAN6_LED_R
};
const char *fan_led_g[] = {FAN0_LED_G, FAN1_LED_G, FAN2_LED_G, FAN3_LED_G,
                           FAN4_LED_G, FAN5_LED_G, FAN6_LED_G
};
#endif

#define REPORT_TEMP 720  /* Report temp every so many cycles */

/* Sensor limits and tuning parameters */

#define INTAKE_LIMIT INTERNAL_TEMPS(60)
#define SWITCH_LIMIT INTERNAL_TEMPS(80)
#if defined(CONFIG_YOSEMITE)
#define USERVER_LIMIT INTERNAL_TEMPS(110)
#else
#define USERVER_LIMIT INTERNAL_TEMPS(90)
#endif

#if defined(CONFIG_MAVERICKS)
#define TOFINO_LIMIT  (105)
#define TOFINO_THRESH (85)
#define TOFINO_HIGH   (80)
#define TOFINO_MED    (70)
#define TOFINO_LOW    (65)
#define TOFINO_DIFF   (50)
#endif

#define TEMP_TOP INTERNAL_TEMPS(70)
#define TEMP_BOTTOM INTERNAL_TEMPS(40)

/*
 * Toggling the fan constantly will wear it out (and annoy anyone who
 * can hear it), so we'll only turn down the fan after the temperature
 * has dipped a bit below the point at which we'd otherwise switch
 * things up.
 */

#define COOLDOWN_SLOP INTERNAL_TEMPS(6)

#define WEDGE_FAN_LOW 35
#define WEDGE_FAN_MEDIUM 50
#define WEDGE_FAN_HIGH 70
#if defined(CONFIG_WEDGE100) || defined(CONFIG_MAVERICKS)
#define WEDGE_FAN_MAX 100
#else
#define WEDGE_FAN_MAX 99
#endif

#define SIXPACK_FAN_LOW 35
#define SIXPACK_FAN_MEDIUM 55
#define SIXPACK_FAN_HIGH 75
#define SIXPACK_FAN_MAX 99

/*
 * Mapping physical to hardware addresses for fans;  it's different for
 * RPM measuring and PWM setting, naturally.  Doh.
 */

#if defined(CONFIG_WEDGE100)
int fan_to_rpm_map[] = {1, 3, 5, 7, 9};
int fan_to_pwm_map[] = {1, 2, 3, 4, 5};
#define FANS 5
// Tacho offset between front and rear fans:
#define REAR_FAN_OFFSET 1
#define BACK_TO_BACK_FANS

#elif defined(CONFIG_MAVERICKS)
#define MAV_FAN_LOW 40
#define MAV_FAN_MEDIUM 60
#define MAV_FAN_HIGH 70
#define MAV_FAN_MAX 100
#define NP_FAN_FIX 80 // FIXME; newport-temporary

/* make upper  fan tray numbers arbitrarily off by 100, more than the largest
 * value
 **/
int fan_to_rpm_map[] = {1, 3, 5, 7, 9, 101, 103, 105, 107, 109};
int fan_to_pwm_map[] = {1, 2, 3, 4, 5, 101, 102, 103, 104, 105};
/*For Newport*/
int fan_to_rpm_map_newport[] = {1, 3, 5, 7, 9, 11};
int fan_to_pwm_map_newport[] = {1, 2, 3, 4, 5, 6};
/*For Stinson */
int fan_to_rpm_map_stinson[] = {1, 3, 5, 7, 9, 11, 13};
int fan_to_pwm_map_stinson[] = {1, 2, 3, 4, 5, 6, 7};

#define FANS 5
// Tacho offset between front and rear fans:
#define REAR_FAN_OFFSET 1
#define BACK_TO_BACK_FANS

#elif defined(CONFIG_WEDGE)
int fan_to_rpm_map[] = {3, 2, 0, 1};
int fan_to_pwm_map[] = {7, 6, 0, 1};
#define FANS 4
// Tacho offset between front and rear fans:
#define REAR_FAN_OFFSET 4
#define BACK_TO_BACK_FANS
#elif defined(CONFIG_YOSEMITE)
int fan_to_rpm_map[] = {0, 1};
int fan_to_pwm_map[] = {0, 1};
#define FANS 2
// Tacho offset between front and rear fans:
#define REAR_FAN_OFFSET 1
#endif


/*
 * The measured RPM of the fans doesn't match linearly to the requested
 * rate.  In addition, there are coaxially mounted fans, so the rear fans
 * feed into the front fans.  The rear fans will run slower since they're
 * grabbing still air, and the front fants are getting an extra boost.
 *
 * We'd like to measure the fan RPM and compare it to the expected RPM
 * so that we can detect failed fans, so we have a table (derived from
 * hardware testing):
 */

struct rpm_to_pct_map {
  uint pct;
  uint rpm;
};

#if defined(CONFIG_WEDGE100) || defined(CONFIG_MAVERICKS)
struct rpm_to_pct_map rpm_front_map[] = {{20, 4200},
                                         {25, 5550},
                                         {30, 6180},
                                         {35, 7440},
                                         {40, 8100},
                                         {45, 9300},
                                         {50, 10410},
                                         {55, 10920},
                                         {60, 11910},
                                         {65, 12360},
                                         {70, 13260},
                                         {75, 14010},
                                         {80, 14340},
                                         {85, 15090},
                                         {90, 15420},
                                         {95, 15960},
                                         {100, 16200}};
#define FRONT_MAP_SIZE (sizeof(rpm_front_map) / sizeof(struct rpm_to_pct_map))

struct rpm_to_pct_map rpm_rear_map[] = {{20, 2130},
                                        {25, 3180},
                                        {30, 3690},
                                        {35, 4620},
                                        {40, 5130},
                                        {45, 6120},
                                        {50, 7050},
                                        {55, 7560},
                                        {60, 8580},
                                        {65, 9180},
                                        {70, 10230},
                                        {75, 11280},
                                        {80, 11820},
                                        {85, 12870},
                                        {90, 13350},
                                        {95, 14370},
                                        {100, 14850}};
#define REAR_MAP_SIZE (sizeof(rpm_rear_map) / sizeof(struct rpm_to_pct_map))

struct rpm_to_pct_map rpm_front_map_newport[] = {{6, 2100},
                                         {13, 3600},
                                         {19, 5000},
                                         {25, 6400},
                                         {31, 7600},
                                         {38, 8800},
                                         {44, 10000},
                                         {50, 11200},
                                         {56, 12400},
                                         {63, 13700},
                                         {69, 14900},
                                         {75, 16200},
                                         {81, 17400},
                                         {88, 18600},
                                         {94, 20000},
                                         {100, 21500}};
#define FRONT_MAP_SIZE_NEWPORT (sizeof(rpm_front_map_newport) / sizeof(struct rpm_to_pct_map))

struct rpm_to_pct_map rpm_rear_map_newport[] = {{6, 1800},
                                         {13, 3200},
                                         {19, 4300},
                                         {25, 5300},
                                         {31, 6500},
                                         {38, 7500},
                                         {44, 8600},
                                         {50, 9500},
                                         {56, 10500},
                                         {63, 11600},
                                         {69, 12500},
                                         {75, 13600},
                                         {81, 14500},
                                         {88, 15600},
                                         {94, 16500},
                                         {100, 18000}};
#define REAR_MAP_SIZE_NEWPORT (sizeof(rpm_rear_map_newport) / sizeof(struct rpm_to_pct_map))
struct rpm_to_pct_map rpm_front_map_stinson[] = {{20, 4200},
                                         {25, 5550},
                                         {30, 6180},
                                         {35, 7440},
                                         {40, 8100},
                                         {45, 9300},
                                         {50, 10410},
                                         {55, 10920},
                                         {60, 11910},
                                         {65, 12360},
                                         {70, 13260},
                                         {75, 14010},
                                         {80, 14340},
                                         {85, 15090},
                                         {90, 15420},
                                         {95, 15960},
                                         {100, 16200}};
#define FRONT_MAP_SIZE_STINSON (sizeof(rpm_front_map_stinson) / sizeof(struct rpm_to_pct_map))

struct rpm_to_pct_map rpm_rear_map_stinson[] = {{6, 1800},
                                        {20, 2130},
                                        {25, 3180},
                                        {30, 3690},
                                        {35, 4620},
                                        {40, 5130},
                                        {45, 6120},
                                        {50, 7050},
                                        {55, 7560},
                                        {60, 8580},
                                        {65, 9180},
                                        {70, 10230},
                                        {75, 11280},
                                        {80, 11820},
                                        {85, 12870},
                                        {90, 13350},
                                        {95, 14370},
                                        {100, 14850}};
#define REAR_MAP_SIZE_STINSON (sizeof(rpm_rear_map_stinson) / sizeof(struct rpm_to_pct_map))
struct rpm_to_pct_map rpm_front_map_davenport[] = {{20, 4200},
                                         {25, 5550},
                                         {30, 6180},
                                         {35, 7440},
                                         {40, 8100},
                                         {45, 9300},
                                         {50, 10410},
                                         {55, 10920},
                                         {60, 11910},
                                         {65, 12360},
                                         {70, 13260},
                                         {75, 14010},
                                         {80, 14340},
                                         {85, 15090},
                                         {90, 15420},
                                         {95, 15960},
                                         {100, 16200}};
#define FRONT_MAP_SIZE_DAVENPORT (sizeof(rpm_front_map_davenport) / sizeof(struct rpm_to_pct_map))
struct rpm_to_pct_map rpm_rear_map_davenport[] = {{6, 1800},
                                        {20, 2130},
                                        {25, 3180},
                                        {30, 3690},
                                        {35, 4620},
                                        {40, 5130},
                                        {45, 6120},
                                        {50, 7050},
                                        {55, 7560},
                                        {60, 8580},
                                        {65, 9180},
                                        {70, 10230},
                                        {75, 11280},
                                        {80, 11820},
                                        {85, 12870},
                                        {90, 13350},
                                        {95, 14370},
                                        {100, 14850}};
#define REAR_MAP_SIZE_DAVENPORT (sizeof(rpm_rear_map_davenport) / sizeof(struct rpm_to_pct_map))
#elif defined(CONFIG_WEDGE)
struct rpm_to_pct_map rpm_front_map[] = {{30, 6150},
                                         {35, 7208},
                                         {40, 8195},
                                         {45, 9133},
                                         {50, 10017},
                                         {55, 10847},
                                         {60, 11612},
                                         {65, 12342},
                                         {70, 13057},
                                         {75, 13717},
                                         {80, 14305},
                                         {85, 14869},
                                         {90, 15384},
                                         {95, 15871},
                                         {100, 16095}};
#define FRONT_MAP_SIZE (sizeof(rpm_front_map) / sizeof(struct rpm_to_pct_map))

struct rpm_to_pct_map rpm_rear_map[] = {{30, 3911},
                                        {35, 4760},
                                        {40, 5587},
                                        {45, 6434},
                                        {50, 7295},
                                        {55, 8187},
                                        {60, 9093},
                                        {65, 10008},
                                        {70, 10949},
                                        {75, 11883},
                                        {80, 12822},
                                        {85, 13726},
                                        {90, 14690},
                                        {95, 15516},
                                        {100, 15897}};
#define REAR_MAP_SIZE (sizeof(rpm_rear_map) / sizeof(struct rpm_to_pct_map))
#elif defined(CONFIG_YOSEMITE)

/* XXX:  Note that 0% is far from 0 RPM. */
struct rpm_to_pct_map rpm_map[] = {{0, 989},
                                   {10, 1654},
                                   {20, 2650},
                                   {30, 3434},
                                   {40, 4318},
                                   {50, 5202},
                                   {60, 5969},
                                   {70, 6869},
                                   {80, 7604},
                                   {90, 8525},
                                   {100, 9325}};

struct rpm_to_pct_map *rpm_front_map = rpm_map;
struct rpm_to_pct_map *rpm_rear_map = rpm_map;
#define MAP_SIZE (sizeof(rpm_map) / sizeof(struct rpm_to_pct_map))
#define FRONT_MAP_SIZE MAP_SIZE
#define REAR_MAP_SIZE MAP_SIZE

#endif

/*
 * Mappings from temperatures recorded from sensors to fan speeds;
 * note that in some cases, we want to be able to look at offsets
 * from the CPU temperature margin rather than an absolute temperature,
 * so we use ints.
 */

struct temp_to_pct_map {
  int temp;
  unsigned speed;
};

#if defined(CONFIG_YOSEMITE)
struct temp_to_pct_map intake_map[] = {{25, 15},
                                       {27, 16},
                                       {29, 17},
                                       {31, 18},
                                       {33, 19},
                                       {35, 20},
                                       {37, 21},
                                       {39, 22},
                                       {41, 23},
                                       {43, 24},
                                       {45, 25}};
#define INTAKE_MAP_SIZE (sizeof(intake_map) / sizeof(struct temp_to_pct_map))

struct temp_to_pct_map cpu_map[] = {{-28, 10},
                                    {-26, 20},
                                    {-24, 25},
                                    {-22, 30},
                                    {-20, 35},
                                    {-18, 40},
                                    {-16, 45},
                                    {-14, 50},
                                    {-12, 55},
                                    {-10, 60},
                                    {-8, 65},
                                    {-6, 70},
                                    {-4, 80},
                                    {-2, 100}};
#define CPU_MAP_SIZE (sizeof(cpu_map) / sizeof(struct temp_to_pct_map))
#endif


#define FAN_FAILURE_OFFSET 30

int fan_low = WEDGE_FAN_LOW;
int fan_medium = WEDGE_FAN_MEDIUM;
int fan_high = WEDGE_FAN_HIGH;
int fan_max = WEDGE_FAN_MAX;
int total_fans = FANS;
int fan_offset = 0;

int temp_bottom = TEMP_BOTTOM;
int temp_top = TEMP_TOP;

int report_temp = REPORT_TEMP;
bool verbose = false;

#if defined(CONFIG_MAVERICKS)
/* board types */
#define BF_BOARD_MAV 1
#define BF_BOARD_MON 2
#define BF_BOARD_NEW 3
#define BF_BOARD_STN 4
#define BF_BOARD_DVN 5
#define BF_BOARD_PSC 6
static int total_fans_upper = 0;
/*Follow bf_board_type_get(), defaulting to Montara*/
static int mav_board_type = BF_BOARD_MON;
#endif

void usage() {
  fprintf(stderr,
          "fand [-v] [-l <low-pct>] [-m <medium-pct>] "
          "[-h <high-pct>]\n"
          "\t[-b <temp-bottom>] [-t <temp-top>] [-r <report-temp>]\n\n"
          "\tlow-pct defaults to %d%% fan\n"
          "\tmedium-pct defaults to %d%% fan\n"
          "\thigh-pct defaults to %d%% fan\n"
          "\ttemp-bottom defaults to %dC\n"
          "\ttemp-top defaults to %dC\n"
          "\treport-temp defaults to every %d measurements\n\n"
          "fand compensates for uServer temperature reading %d degrees low\n"
          "kill with SIGUSR1 to stop watchdog\n",
          fan_low,
          fan_medium,
          fan_high,
          EXTERNAL_TEMPS(temp_bottom),
          EXTERNAL_TEMPS(temp_top),
          report_temp,
          EXTERNAL_TEMPS(USERVER_TEMP_FUDGE));
  exit(1);
}

/* We need to open the device each time to read a value */
int read_device(const char *device, int *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "r");
  if (!fp) {
    int err = errno;
    syslog(LOG_INFO, "failed to open device %s", device);
    return err;
  }

  rc = fscanf(fp, "%d", value);
  fclose(fp);

  if (rc != 1) {
    syslog(LOG_INFO, "failed to read device %s", device);
    return ENOENT;
  } else {
    return 0;
  }
}

/* We need to open the device again each time to write a value */

int write_device(const char *device, const char *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "w");
  if (!fp) {
    int err = errno;

    syslog(LOG_INFO, "failed to open device for write %s", device);
    return err;
  }

  rc = fputs(value, fp);
  fclose(fp);

  if (rc < 0) {
    syslog(LOG_INFO, "failed to write device %s", device);
    return ENOENT;
  } else {
    return 0;
  }
}

#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
int read_temp(const char *device, int *value) {
  char full_name[LARGEST_DEVICE_NAME + 1];
  int rc = 1;
  int retry = 0;

  /* We set an impossible value to check for errors */
  *value = BAD_TEMP;
  if (((mav_board_type == BF_BOARD_MON)||(mav_board_type == BF_BOARD_MAV)) && (strstr(device, USERVER_TEMP_DEVICE))) {
    /* On Wedge100BF-series, the DIMM temp register is still treated as com-e temperature. */
    snprintf(
        full_name, LARGEST_DEVICE_NAME, "%s/temp2_input", device);
  } else {
    snprintf(
        full_name, LARGEST_DEVICE_NAME, "%s/temp1_input", device);
  }

  //int rc = read_device(full_name, value);
#if defined(CONFIG_WEDGE100)
  if ((rc || *value > WEDGE100_COME_DIMM) && (strstr(device, COM_E_DIR))) {
    *value = BAD_TEMP;
  }
#endif
  while(rc && (retry++ <= 3)) {
	rc = read_device(full_name, value);
	//syslog(LOG_INFO, "full_name %s  retry is %d", full_name, retry);
	usleep(10000);
  }

    if (strstr(device, USERVER_TEMP_DEVICE)) {
        retry = 0;
        if (*value > USERVER_ERROR_THRESHOLD) {
            while(retry++ <= 3) {
			    *value = 0;
                rc = read_device(full_name, value);
			    usleep(10000);
            }
            if (*value > USERVER_ERROR_THRESHOLD) {
                syslog(LOG_CRIT, "Userver(COMe) temperature read error, value:%d. \n", (*value)/1000);
		        *value = BAD_TEMP;
            }
	    }
    }
    
  return rc;
}
#endif

#if defined(CONFIG_WEDGE) && !defined(CONFIG_WEDGE100) \
                          && !defined(CONFIG_MAVERICKS)
int read_gpio_value(const int id, const char *device, int *value) {
  char full_name[LARGEST_DEVICE_NAME];

  snprintf(full_name, LARGEST_DEVICE_NAME, device, id);
  return read_device(full_name, value);
}

int read_gpio_values(const int start, const int count,
                     const char *device, int *result) {
  int status = 0;
  int value;

  *result = 0;
  for (int i = 0; i < count; i++) {
      status |= read_gpio_value(start + i, GPIO_PIN_ID, &value);
      *result |= value << i;
  }
  return status;
}

int read_ids(int *rev_id, int *board_id) {
  int status = 0;
  int value;

  status = read_gpio_values(GPIO_REV_ID_START, REV_IDS, GPIO_PIN_ID, rev_id);
  if (status != 0) {
    syslog(LOG_INFO, "failed to read rev_id");
    return status;
  }

  int board_id_start;
  if (*rev_id >= REV_ID_NEW_BOARD_ID) {
    board_id_start = GPIO_BOARD_ID_START_NEW;
  } else {
    board_id_start = GPIO_BOARD_ID_START;
  }

  status = read_gpio_values(board_id_start, BOARD_IDS, GPIO_PIN_ID, board_id);
  if (status != 0) {
    syslog(LOG_INFO, "failed to read board_id");
  }
  return status;
}

bool is_two_fan_board(bool verbose) {
  struct wedge_eeprom_st eeprom;
  /* Retrieve the board type from EEPROM */
  if (wedge_eeprom_parse(NULL, &eeprom) == 0) {
    /* able to parse EEPROM */
    if (verbose) {
      syslog(LOG_INFO, "board type is %s", eeprom.fbw_location);
    }
    /* only WEDGE is NOT two-fan board */
    return strncasecmp(eeprom.fbw_location, "wedge",
                       sizeof(eeprom.fbw_location));
  } else {
    int status;
    int board_id = 0;
    int rev_id = 0;
    /*
     * Could not parse EEPROM. Most likely, it is an old HW without EEPROM.
     * In this case, use board ID to distinguish if it is wedge or 6-pack.
     */
    status = read_ids(&rev_id, &board_id);
    if (verbose) {
      syslog(LOG_INFO, "rev ID %d, board id %d", rev_id, board_id);
    }
    if (status == 0 && board_id != 0xf) {
      return true;
    } else {
      return false;
    }
  }
}
#endif

#if defined(CONFIG_MAVERICKS)
static int tofino_ext_pvt_en = 0;

/* returns different BF board types */
static int bf_board_type_get() {
  struct wedge_eeprom_st eeprom;
  /* Retrieve the board type from EEPROM . We support Rev2 upward EEPROM map */
  if (wedge_eeprom_parse(NULL, &eeprom) == 0) {
    /* able to parse EEPROM */
    if (verbose) {
      syslog(LOG_INFO, "BF board type is %s", eeprom.fbw_location);
    }
    if ((!strncasecmp(eeprom.fbw_location, "Maverick", strlen("Maverick")))
          || (!strncasecmp(eeprom.fbw_location, "Mavericks", strlen("Mavericks")))){
      syslog(LOG_INFO, "BF board type is %s", eeprom.fbw_location);
      return BF_BOARD_MAV;
    } else if (!strncasecmp(eeprom.fbw_location, "Montara", strlen("Montara"))) {
      syslog(LOG_INFO, "BF board type is %s", eeprom.fbw_location);
      return BF_BOARD_MON;
    } else if ((!strncasecmp(eeprom.fbw_location, "Newport", strlen("Newport")))
                 || (!strncasecmp(eeprom.fbw_location, "Newports", strlen("Newports")))){
      syslog(LOG_INFO, "BF board type is %s", eeprom.fbw_location);
      /* Restore thermal sensor access from Newport R0B
       * System Assembly Part Number xxx-000004-02 is for R0A
       * System Assembly Part Number xxx-000004-03 is for R0B. - Aug. 14, 2020
       * Warning: If update this eeprom field, MUST restart the fand daemon.
       */
      char *cmp_pn=&eeprom.fbw_assembly_number[4];
      if (strcmp(cmp_pn, "000004-02")==0) {
          syslog(LOG_DEBUG, "Turn On for PVT reading (%s)", eeprom.fbw_assembly_number);
          tofino_ext_pvt_en=1;
      } else {
          syslog(LOG_DEBUG, "Turn Off for PVT reading (%s)", eeprom.fbw_assembly_number);
          tofino_ext_pvt_en=0;
      }
      return BF_BOARD_NEW;
    } else if (!strncasecmp(eeprom.fbw_location, "Stinson", strlen("Stinson"))){
      syslog(LOG_INFO, "BF board type is %s", eeprom.fbw_location);
      tofino_ext_pvt_en=0;
      return BF_BOARD_STN;
    } else if (!strncasecmp(eeprom.fbw_location, "Davenpor", strlen("Davenpor"))){
      syslog(LOG_INFO, "BF board type is %s", eeprom.fbw_location);
      tofino_ext_pvt_en=0;
      return BF_BOARD_DVN;
    } else if (!strncasecmp(eeprom.fbw_location, "Pescader", strlen("Pescader"))){
      syslog(LOG_INFO, "BF board type is %s", eeprom.fbw_location);
      tofino_ext_pvt_en=0;
      return BF_BOARD_PSC;
    } else {
      syslog(LOG_WARNING, "BF invalid board type. defaulting to Montara");
      return BF_BOARD_MON;
    }
  } else {
    syslog(LOG_WARNING, "BF error reading eeprom. defaulting to Montara");
    return BF_BOARD_MON;
  }
}

static bool file_exists(const char *file_name) {

  struct stat buffer;
  return (stat(file_name, &buffer) == 0);
}

static void close_and_remove_file(FILE *fp, const char *file_name) {

  fclose(fp);
  remove(file_name);
}

static int fd_tofino_ext_tmp = -1;
#if 1
static int tofino_open_i2c_dev(int i2cbus, char *filename, size_t size, int quiet)
{
  int file;

  snprintf(filename, size, "/dev/i2c/%d", i2cbus);
  filename[size - 1] = '\0';
  file = open(filename, O_RDWR);

  if (file < 0 && (errno == ENOENT || errno == ENOTDIR)) {
    sprintf(filename, "/dev/i2c-%d", i2cbus);
    file = open(filename, O_RDWR);
  }

  if (file < 0 && !quiet) {
    if (errno == ENOENT) {
      fprintf(stderr, "Error: Could not open file "
                      "`/dev/i2c-%d' or `/dev/i2c/%d': %s\n",
                      i2cbus, i2cbus, strerror(ENOENT));
    } else {
      fprintf(stderr, "Error: Could not open file "
                      "`%s': %s\n", filename, strerror(errno));
      if (errno == EACCES) {
        fprintf(stderr, "Run as root?\n");
      }
    }
  }
  return file;
}

static int set_slave_addr(int file, int address, int force)
{
  /* With force, let the user read from/write to the registers
     even when a driver is also running */
  if (ioctl(file, force ? I2C_SLAVE_FORCE : I2C_SLAVE, address) < 0) {
    fprintf(stderr,
            "Error: Could not set address to 0x%02x: %s\n",
            address, strerror(errno));
    return -errno;
  }

  return 0;
}

/* write pvt_ctrl register to perform continuous temp monitoring */
static void np_write_tofino_pvt_ctrl(void)
{
  struct i2c_rdwr_ioctl_data i2c_rdwr_data;
  struct i2c_msg i2c_msg_data[1];
  unsigned char wr_buf[12];
  unsigned short i2c_addr = 0x58;

  memset(i2c_msg_data, 0 , sizeof(i2c_msg_data));

  wr_buf[0] = 0x80; /* cmd */
  wr_buf[1] = 0xf4; /* reg address 0x000801fc */
  wr_buf[2] = 0x01;
  wr_buf[3] = 0x08;
  wr_buf[4] = 0x00;
  wr_buf[5] = 0xe2;
  wr_buf[6] = 0x01;
  wr_buf[7] = 0x81;
  wr_buf[8] = 0x00;

  i2c_msg_data[0].addr = i2c_addr;
  i2c_msg_data[0].flags = 0;
  i2c_msg_data[0].len = 9;
  i2c_msg_data[0].buf = (char *)wr_buf;
  i2c_rdwr_data.msgs = i2c_msg_data;
  i2c_rdwr_data.nmsgs = 1;

  if (ioctl(fd_tofino_ext_tmp, I2C_RDWR, &i2c_rdwr_data) == -1) {
    syslog(LOG_CRIT, "tofino pvt sensor disable ioctl error");
  }

  wr_buf[5] = 0xe3;
  if (ioctl(fd_tofino_ext_tmp, I2C_RDWR, &i2c_rdwr_data) == -1) {
    syslog(LOG_CRIT, "tofino pvt sensor enable ioctl error");
  }

  return;
}

/* read pvt_status register that has temperature value */
static void np_read_tofino_temp_pvt(int *temp)
{
  const char *lock_file_name = "/tmp/mav_9548_10_lock";
  FILE *lock_file_fp;
  struct i2c_rdwr_ioctl_data i2c_rdwr_data;
  struct i2c_msg i2c_msg_data[2];
  unsigned char wr_buf[12];
  unsigned char rd_buf[8];
  unsigned short i2c_addr = 0x58;
  int timeout_counter = 0;
  double tmpr, temp2, temperature;

  *temp = BAD_TEMP;
  /* Acquire the file lock */
  while (file_exists(lock_file_name) == true) {
    timeout_counter++;
    if (timeout_counter >= 2) {
      /* It's possible that the other process using the lock might have
         malfunctioned. Hence explicitly delete the file and proceed*/
      syslog(LOG_CRIT, "Some process didn't clean up the lock file. Hence explicitly cleaning it up and proceeding");
      remove(lock_file_name);
      break;
    }
    sleep(1);
    /* Periodically kick the watch dog while trying to acquire the file lock */
    kick_watchdog();
  }

  kick_watchdog();

  lock_file_fp = fopen(lock_file_name, "w+");

  memset(i2c_msg_data, 0 , sizeof(i2c_msg_data));

  wr_buf[0] = 0xa0; /* cmd */
  wr_buf[1] = 0xfc; /* reg address 0x000801fc */
  wr_buf[2] = 0x01;
  wr_buf[3] = 0x08;
  wr_buf[4] = 0x00;

  i2c_msg_data[0].addr = i2c_addr;
  i2c_msg_data[0].flags = 0;
  i2c_msg_data[0].len = 5;
  i2c_msg_data[0].buf = (char *)wr_buf;
  i2c_msg_data[1].addr = i2c_addr;
  i2c_msg_data[1].flags = I2C_M_RD;
  i2c_msg_data[1].len = 4;
  i2c_msg_data[1].buf = (char *)rd_buf;

  i2c_rdwr_data.msgs = i2c_msg_data;
  i2c_rdwr_data.nmsgs = 2;

  if (ioctl(fd_tofino_ext_tmp, I2C_RDWR, &i2c_rdwr_data) == -1) {
    syslog(LOG_CRIT, "i2c_rdwrd ioctl error ");
    close_and_remove_file(lock_file_fp, lock_file_name);
    return;
  }
  close_and_remove_file(lock_file_fp, lock_file_name);

  if ((rd_buf[1] & 0x10) == 0) {
    syslog(LOG_CRIT, "error Tofino Temp meaurement not ready 0x%x", *(unsigned long *)rd_buf);
    np_write_tofino_pvt_ctrl(); /* restart temp monitoring just in case */
    return;
  }

  rd_buf[1] &= 0x3; /* strip off other bits */
  tmpr = (double)(rd_buf[0] | (rd_buf[1] << 8));
  temp2 = tmpr * tmpr;
  temperature = (temp2 * -0.000011677) + (tmpr * 0.28031) - 66.599;
  *temp = (int)temperature;
  return;
}
#endif

/* temp1 and temp2 return thermal diade 1 and 2 respectively */
static void mav_read_tofino_temp(int *temp1, int *temp2) {
  uint8_t val;
  char full_name[LARGEST_DEVICE_NAME];
  int fd = fd_tofino_ext_tmp;
  int res;
  const char *lock_file_name = "/tmp/mav_9548_10_lock";
  FILE *lock_file_fp;
  int timeout_counter = 0;

  *temp1 = BAD_TEMP;
  if (temp2) {
    *temp2 = BAD_TEMP;
  }

  if (fd == -1) {
    return;
  }

  if (mav_board_type == BF_BOARD_MAV) {
    /* Acquire the file lock */
    while (file_exists(lock_file_name) == true) {
      timeout_counter++;
      if (timeout_counter >= 5) {
        /* It's possible that the other process using the lock might have
           malfunctioned. Hence explicitly delete the file and proceed*/
        syslog(LOG_CRIT, "Some process didn't clean up the lock file. Hence explicitly cleaning it up and proceeding");
        remove(lock_file_name);
        break;
      }
      sleep(1);
      /* Periodically kick the watch dog while trying to acquire the file lock */
      kick_watchdog();
    }

    lock_file_fp = fopen(lock_file_name, "w+");
  }

  /* file_exists() and fopen() can sometimes take a very long time. Hence 
     kick the watch dog so that the delay introduced by us is nullified
     and the timer doesn't go off */
  kick_watchdog();

  if (mav_board_type == BF_BOARD_MAV) {
    /* open PCA9548 channel */
    res = ioctl(fd, I2C_SLAVE_FORCE, 0x70);
    if (res < 0) {
      syslog(LOG_CRIT, "Failed to open slave @ address 0x70 error %d", res);
      close_and_remove_file(lock_file_fp, lock_file_name);
      return;
    }
    res = i2c_smbus_write_byte(fd, 0x10);
    if (res < 0) {
      syslog(LOG_CRIT, "Failed to write slave @ address 0x70 error %d", res);
      close_and_remove_file(lock_file_fp, lock_file_name);
      return;
    }
  }

  res = ioctl(fd, I2C_SLAVE_FORCE, 0x4c);
  if (res < 0) {
    syslog(LOG_CRIT, "Failed to open slave @ address 0x4c error %d", res);
    if (mav_board_type == BF_BOARD_MAV) {
      close_and_remove_file(lock_file_fp, lock_file_name);
    }
    return;
  }

  res = i2c_smbus_read_byte_data(fd, 0x1);
  if (res < 0) {
    syslog(LOG_CRIT, "Failed to read slave @ address 0x4c reg 0x01 error %d", res);
    if (mav_board_type == BF_BOARD_MAV) {
      close_and_remove_file(lock_file_fp, lock_file_name);
    }
    return;
  }

  *temp1 = res;
  if ((mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_PSC) && temp2) {
    res = i2c_smbus_read_byte_data(fd, 0x23);
    if (res < 0) {
      syslog(LOG_CRIT, "Failed to read slave @ address 0x4c reg 0x23 error %d", res);
      return;
    }
    *temp2 = res;
  }

  if ((mav_board_type == BF_BOARD_DVN) && temp2) {
    *temp2 = 0;
  }

  if (mav_board_type == BF_BOARD_MAV) {
    /* close PCA9548 channel */
	/*
    res = ioctl(fd, I2C_SLAVE_FORCE, 0x70);
    if (res < 0) {
      syslog(LOG_CRIT, "Failed to open slave @ address 0x70 error %d", res);
      close_and_remove_file(lock_file_fp, lock_file_name);
      return;
    }
    res = i2c_smbus_write_byte(fd, 0x00);
    if (res < 0) {
      syslog(LOG_CRIT, "Failed to write slave @ address 0x70 error %d", res);
      close_and_remove_file(lock_file_fp, lock_file_name);
      return;
    }
     */
    close_and_remove_file(lock_file_fp, lock_file_name);
 }
}

static int mav_open_i2c_dev(int i2c_bus) {
  char fn[32];
  int fd;
  int rc;

  snprintf(fn, sizeof(fn), "/dev/i2c-%d", i2c_bus);
  fd = open(fn, O_RDWR);
  if (fd == -1) {
    syslog(LOG_CRIT, "Failed to open i2c device %s", fn);
    return -1;
  }
  return fd;
}

static void mav_syscpld_write(int i2c_bus, int i2c_addr, uint8_t reg,
                              uint8_t val) {
  int res, fd;

  fd = mav_open_i2c_dev(i2c_bus);
  if (fd < 0) {
    return;
  }
  res = ioctl(fd, I2C_SLAVE_FORCE, i2c_addr);
  if (res < 0) {
    syslog(LOG_CRIT, "Failed to open slave @ address 0x%x error %d",
           i2c_addr, res);
    close(fd);
    return;
  }
  res = i2c_smbus_write_byte_data(fd, reg, val);
  if (res < 0) {
    syslog(LOG_CRIT, "Failed to write slave @ address 0x%x", i2c_addr);
    close(fd);
    return;
  }
  close(fd);
}

#endif

int read_fan_value(const int fan, const char *device, int *value) {
  char device_name[LARGEST_DEVICE_NAME];
  char output_value[LARGEST_DEVICE_NAME];
  char full_name[LARGEST_DEVICE_NAME];
#if defined(CONFIG_MAVERICKS)
  if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) { 
      snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
      snprintf(full_name, LARGEST_DEVICE_NAME, "%s%s", PWM_DIR_NEWPORT, device_name);
  } else {
    if (fan >= 100) {
      snprintf(device_name, LARGEST_DEVICE_NAME, device, fan-100);
      snprintf(full_name, LARGEST_DEVICE_NAME, "%s%s", PWM_UPPER_DIR, device_name);
    } else {
      snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
      snprintf(full_name, LARGEST_DEVICE_NAME, "%s%s", PWM_DIR, device_name);
    }
  }
#else
  snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", PWM_DIR,device_name);
#endif
  return read_device(full_name, value);
}

int write_fan_value(const int fan, const char *device, const int value) {
  char full_name[LARGEST_DEVICE_NAME];
  char device_name[LARGEST_DEVICE_NAME];
  char output_value[LARGEST_DEVICE_NAME];

#if defined(CONFIG_MAVERICKS)
  if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
    /*fantray_pwm*/
    snprintf(full_name, LARGEST_DEVICE_NAME, "%s%s", PWM_DIR_NEWPORT, device);
  } else {
    /*fantray%d_pwm*/
    if (fan >= 100) {
      snprintf(device_name, LARGEST_DEVICE_NAME, device, fan-100);
      snprintf(full_name, LARGEST_DEVICE_NAME, "%s%s", PWM_UPPER_DIR, device_name);
    } else {
      snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
      snprintf(full_name, LARGEST_DEVICE_NAME, "%s%s", PWM_DIR, device_name);
    }
  }
#else
  snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", PWM_DIR, device_name);
#endif
  snprintf(output_value, LARGEST_DEVICE_NAME, "%d", value);
  return write_device(full_name, output_value);
}

/* Return fan speed as a percentage of maximum -- not necessarily linear. */

int fan_rpm_to_pct(const struct rpm_to_pct_map *table,
                   const int table_len,
                   int rpm) {
  int i;

  for (i = 0; i < table_len; i++) {
    if (table[i].rpm > rpm) {
      break;
    }
  }

  /*
   * If the fan RPM is lower than the lowest value in the table,
   * we may have a problem -- fans can only go so slow, and it might
   * have stopped.  In this case, we'll return an interpolated
   * percentage, as just returning zero is even more problematic.
   */

  if (i == 0) {
    return (rpm * table[i].pct) / table[i].rpm;
  } else if (i == table_len) { // Fell off the top?
    return table[i - 1].pct;
  }

  // Interpolate the right percentage value:

  int percent_diff = table[i].pct - table[i - 1].pct;
  int rpm_diff = table[i].rpm - table[i - 1].rpm;
  int fan_diff = table[i].rpm - rpm;

  return table[i].pct - (fan_diff * percent_diff / rpm_diff);
}

int fan_speed_okay(const int fan, const int speed, const int slop) {
  int front_fan, rear_fan;
  int front_pct, rear_pct;
  int real_fan;
  int okay;

  /*
   * The hardware fan numbers are different from the physical order
   * in the box, so we have to map them:
   */

#if defined(CONFIG_MAVERICKS)
  if (mav_board_type == BF_BOARD_NEW) {
    real_fan = fan_to_rpm_map_newport[fan];
    front_fan = 0;
    read_fan_value(real_fan, FAN_READ_RPM_FORMAT, &front_fan);
    front_pct = fan_rpm_to_pct(rpm_front_map_newport, FRONT_MAP_SIZE_NEWPORT, front_fan);
#ifdef BACK_TO_BACK_FANS
    rear_fan = 0;
    read_fan_value(real_fan + REAR_FAN_OFFSET, FAN_READ_RPM_FORMAT, &rear_fan);
    rear_pct = fan_rpm_to_pct(rpm_rear_map_newport, REAR_MAP_SIZE_NEWPORT, rear_fan);
#endif
  } else if (mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_PSC) {
    real_fan = fan_to_rpm_map_stinson[fan];
    front_fan = 0;
    read_fan_value(real_fan, FAN_READ_RPM_FORMAT, &front_fan);
    front_pct = fan_rpm_to_pct(rpm_front_map_stinson, FRONT_MAP_SIZE_STINSON, front_fan);
#ifdef BACK_TO_BACK_FANS
    rear_fan = 0;
    read_fan_value(real_fan + REAR_FAN_OFFSET, FAN_READ_RPM_FORMAT, &rear_fan);
    rear_pct = fan_rpm_to_pct(rpm_rear_map_stinson, REAR_MAP_SIZE_STINSON, rear_fan);
#endif
} else if (mav_board_type == BF_BOARD_DVN) {
    real_fan = fan_to_rpm_map_newport[fan];
    front_fan = 0;
    read_fan_value(real_fan, FAN_READ_RPM_FORMAT, &front_fan);
    front_pct = fan_rpm_to_pct(rpm_front_map_davenport, FRONT_MAP_SIZE_DAVENPORT, front_fan);
#ifdef BACK_TO_BACK_FANS
    rear_fan = 0;
    read_fan_value(real_fan + REAR_FAN_OFFSET, FAN_READ_RPM_FORMAT, &rear_fan);
    rear_pct = fan_rpm_to_pct(rpm_rear_map_davenport, REAR_MAP_SIZE_DAVENPORT, rear_fan);
#endif
  } else
#endif
  {
    real_fan = fan_to_rpm_map[fan];
    front_fan = 0;
    read_fan_value(real_fan, FAN_READ_RPM_FORMAT, &front_fan);
    front_pct = fan_rpm_to_pct(rpm_front_map, FRONT_MAP_SIZE, front_fan);
#ifdef BACK_TO_BACK_FANS
    rear_fan = 0;
    read_fan_value(real_fan + REAR_FAN_OFFSET, FAN_READ_RPM_FORMAT, &rear_fan);
    rear_pct = fan_rpm_to_pct(rpm_rear_map, REAR_MAP_SIZE, rear_fan);
#endif
  }

  /*
   * If the fans are broken, the measured rate will be rather
   * different from the requested rate, and we can turn up the
   * rest of the fans to compensate.  The slop is the percentage
   * of error that we'll tolerate.
   *
   * XXX:  I suppose that we should only measure negative values;
   * running too fast isn't really a problem.
   */

#ifdef BACK_TO_BACK_FANS
  okay = (abs(front_pct - speed) * 100 / speed < slop &&
          abs(rear_pct - speed) * 100 / speed < slop);
#else
  okay = (abs(front_pct - speed) * 100 / speed < slop);
#endif

  if (!okay || verbose) {
    syslog(!okay ? LOG_WARNING : LOG_INFO,
#ifdef BACK_TO_BACK_FANS
           "fan %d rear %d (%d%%), front %d (%d%%), expected %d",
#else
           "fan %d %d RPM (%d%%), expected %d",
#endif
           fan,
#ifdef BACK_TO_BACK_FANS
           rear_fan,
           rear_pct,
#endif
           front_fan,
           front_pct,
           speed);
  }

  return okay;
}

/* Set fan speed as a percentage */

int write_fan_speed(const int fan, const int value) {
  /*
   * The hardware fan numbers for pwm control are different from
   * both the physical order in the box, and the mapping for reading
   * the RPMs per fan, above.
   */

  int real_fan = fan_to_pwm_map[fan];

#if defined(CONFIG_MAVERICKS)
  if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_DVN) {
    real_fan = fan_to_pwm_map_newport[fan];
  } else if (mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_PSC) {
    real_fan = fan_to_pwm_map_stinson[fan];
  }
#endif

  if (value == 0) {
#if defined(CONFIG_WEDGE100) || defined(CONFIG_MAVERICKS)
    if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
        return write_fan_value(real_fan, "fantray_pwm", 0);
    } else {
        return write_fan_value(real_fan, "fantray%d_pwm", 0);
    }
#else
    return write_fan_value(real_fan, "pwm%d_en", 0);
#endif
  } else {
    int unit = value * PWM_UNIT_MAX / 100;
    int status;

#if defined(CONFIG_WEDGE100) || defined(CONFIG_MAVERICKS)
    if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
        // According to Fan_board_CPLD_Specification
        unit = value * PWM_UNIT_MAX_NEWPORT / 100;
        if ((value==25) || (value==50) || (value==75) || (value==100)) {
            unit = unit - 1;
        }
        return write_fan_value(real_fan, "fantray_pwm", unit);
    } else {
        // Note that PWM for Wedge100 and Wedge100BF is in 32nds of a cycle
        return write_fan_value(real_fan, "fantray%d_pwm", unit);
    }
#else
    if (unit == PWM_UNIT_MAX)
      unit = 0;

    if ((status = write_fan_value(real_fan, "pwm%d_type", 0)) != 0 ||
      (status = write_fan_value(real_fan, "pwm%d_rising", 0)) != 0 ||
      (status = write_fan_value(real_fan, "pwm%d_falling", unit)) != 0 ||
      (status = write_fan_value(real_fan, "pwm%d_en", 1)) != 0) {
      return status;
    }
#endif
  }
}

#if defined(CONFIG_YOSEMITE)
int temp_to_fan_speed(int temp, struct temp_to_pct_map *map, int map_size) {
  int i = map_size - 1;

  while (i > 0 && temp < map[i].temp) {
    --i;
  }
  return map[i].speed;
}
#endif

/* Set up fan LEDs */

int write_fan_led(const int fan, const char *color) {
#if defined(CONFIG_WEDGE100) || defined(CONFIG_WEDGE) \
    || defined(CONFIG_MAVERICKS)
    if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
        if (strstr(color, FAN_LED_BLUE))
        {
            write_device(fan_led_g[fan], FAN_LED_ON);
            write_device(fan_led_r[fan], FAN_LED_OFF);
        }
        else if (strstr(color, FAN_LED_RED))
        {
            write_device(fan_led_g[fan], FAN_LED_OFF);
            write_device(fan_led_r[fan], FAN_LED_ON);
        }
        return 0;
    } else {
        return write_device(fan_led[fan], color);
    }
#else
        return 0;
#endif
}

int server_shutdown(const char *why) {
  if(0 == access("/mnt/data/etc/not_shutdown_come", F_OK))
  {
      return 0;
  }
  int fan;
  for (fan = 0; fan < total_fans; fan++) {
    write_fan_speed(fan + fan_offset, fan_max);
  }

  syslog(LOG_EMERG, "Shutting down:  %s", why);

#if defined(CONFIG_MAVERICKS)
  syslog(LOG_CRIT, "resetting Tofino...");
  system("/usr/local/bin/reset_tofino.sh");
  mav_syscpld_write(12, 0x31, 0x32, 0xf);
#endif

#if defined(CONFIG_WEDGE100) || defined(CONFIG_MAVERICKS)
  write_device(USERVER_POWER, "0");
  sleep(5);
  write_device(MAIN_POWER, "0");
#endif
#if defined(CONFIG_WEDGE) && !defined(CONFIG_WEDGE100) \
                          && !defined(CONFIG_MAVERICKS)
  write_device(GPIO_USERVER_POWER_DIRECTION, "out");
  write_device(GPIO_USERVER_POWER, "0");
  /*
   * Putting T2 in reset generates a non-maskable interrupt to uS,
   * the kernel running on uS might panic depending on its version.
   * sleep 5s here to make sure uS is completely down.
   */
  sleep(5);

  if (write_device(GPIO_T2_POWER_DIRECTION, "out") ||
      write_device(GPIO_T2_POWER, "1")) {
    /*
     * We're here because something has gone badly wrong.  If we
     * didn't manage to shut down the T2, cut power to the whole box,
     * using the PMBus OPERATION register.  This will require a power
     * cycle (removal of both power inputs) to recover.
     */
    syslog(LOG_EMERG, "T2 power off failed;  turning off via ADM1278");
    system("rmmod adm1275");
    system("i2cset -y 12 0x10 0x01 00");
  }
#else
  // TODO(7088822):  try throttling, then shutting down server.
  syslog(LOG_EMERG, "Need to implement actual shutdown!\n");
#endif

  /*
   * We have to stop the watchdog, or the system will be automatically
   * rebooted some seconds after fand exits (and stops kicking the
   * watchdog).
   */

  stop_watchdog();

  sleep(2);
  exit(2);
}

/* Gracefully shut down on receipt of a signal */

void fand_interrupt(int sig)
{
  int fan;
  for (fan = 0; fan < total_fans; fan++) {
    write_fan_speed(fan + fan_offset, fan_max);
  }

#if defined(CONFIG_MAVERICKS)
  if (mav_board_type == BF_BOARD_MAV) {
    for (fan = total_fans; fan < (total_fans + total_fans_upper); fan++) {
      write_fan_speed(fan + fan_offset, fan_max);
    }
  }
#endif

  syslog(LOG_WARNING, "Shutting down fand on signal %s", strsignal(sig));
  if (sig == SIGUSR1) {
    stop_watchdog();
  }
  exit(3);
}

#endif

int main(int argc, char **argv) {
  /* Sensor values */

#if defined(CONFIG_LIGHTNING)
  return 0;
#else

#if defined(CONFIG_WEDGE)
  int intake_temp;
  int exhaust_temp;
  int switch_temp;
  int userver_temp;
  /*The followings are to avoid mistaking temp value derived from EC*/
  int ignore_userver_temp;
  int prev_intake_temp = 0;
  int prev_exhaust_temp = 0;
  int prev_switch_temp = 0;
  int prev_userver_temp = 0;
#else
  float intake_temp;
  float exhaust_temp;
  float userver_temp;
#endif

  int fan_speed = fan_high;
  int bad_reads = 0;
  int fan_failure = 0;
  int fan_speed_changes = 0;
  int old_speed;
#if defined(CONFIG_MAVERICKS)
  int ignore_upper_fan_tray = 0;
  int enable_high_temp_profile = 0;
  int base_temperature = 95;
  int tofino_jct_temp, tofino_jct_temp2 = 0;
  //#define SERVER_TEST 1
  #if defined(SERVER_TEST)
  int test = 0;
  #endif
  int prev_tofino_jct_temp = 0, prev_tofino_jct_temp2=0;
  int fan_speed_upper = MAV_FAN_HIGH;
  int bad_reads_tofino = 0;
  int fan_failure_upper = 0;
  int fan_speed_changes_upper = 0;
  int old_speed_upper;
  int fan_bad[FANS*2]; /*Mavericks:10, Montara:5, Newport:6*/
  int prev_fans_bad_upper = 0;
#else
  int fan_bad[FANS];
#endif
  int fan;

  unsigned log_count = 0; // How many times have we logged our temps?
  int opt;
  int prev_fans_bad = 0;

  struct sigaction sa;

  sa.sa_handler = fand_interrupt;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);

  // Start writing to syslog as early as possible for diag purposes.

  openlog("fand", LOG_CONS, LOG_DAEMON);

#if defined(CONFIG_WEDGE) && !defined(CONFIG_WEDGE100) \
                          && !defined(CONFIG_MAVERICKS)
  if (is_two_fan_board(false)) {
    /* Alternate, two fan configuration */
    total_fans = 2;
    fan_offset = 2; /* fan 3 is the first */

    fan_low = SIXPACK_FAN_LOW;
    fan_medium = SIXPACK_FAN_MEDIUM;
    fan_high = SIXPACK_FAN_HIGH;
    fan_max = SIXPACK_FAN_MAX;
    fan_speed = fan_high;
  }
#endif

#if defined(CONFIG_MAVERICKS)
  mav_board_type = bf_board_type_get();
  syslog(LOG_CRIT, "board type is %d\n", mav_board_type);
  if (mav_board_type == BF_BOARD_MAV) {
    total_fans_upper = FANS;
    fd_tofino_ext_tmp = mav_open_i2c_dev(9);
  } else if (mav_board_type == BF_BOARD_MON) {
    fd_tofino_ext_tmp = mav_open_i2c_dev(3);
  } else if (mav_board_type == BF_BOARD_NEW) {
    total_fans = 6; /*6 fans on lower fan board*/
    if (tofino_ext_pvt_en == 0) {
      fd_tofino_ext_tmp = mav_open_i2c_dev(3);
    } else { // read it thru PVT register
      char filename[32];
      /* i2c read from tofino */
      fd_tofino_ext_tmp = tofino_open_i2c_dev(11, filename, sizeof(filename), 0);
      if (fd_tofino_ext_tmp < 0 || set_slave_addr(fd_tofino_ext_tmp, 0x58, 1)) {
        syslog(LOG_CRIT, "Error: cannot open i2c device file");
      }
      np_write_tofino_pvt_ctrl();
    }
  } else if (mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_PSC) {
    total_fans = 7; /* 7 fans on fan board*/
    fd_tofino_ext_tmp = mav_open_i2c_dev(3);
  } else if (mav_board_type == BF_BOARD_DVN) {
    total_fans = 6; /* 6 fans on fan board*/
    fd_tofino_ext_tmp = mav_open_i2c_dev(3);
  } else {
    fd_tofino_ext_tmp = mav_open_i2c_dev(3);
  }
  if (fd_tofino_ext_tmp < 0) {
    syslog (LOG_CRIT, "Error opening Tofino Temp i2c device");
  }
#endif

  while ((opt = getopt(argc, argv, "l:m:h:b:t:r:v:d:")) != -1) {
    switch (opt) {
    case 'l':
      fan_low = atoi(optarg);
      break;
    case 'm':
      fan_medium = atoi(optarg);
      break;
    case 'h':
      fan_high = atoi(optarg);
      break;
    case 'b':
      temp_bottom = INTERNAL_TEMPS(atoi(optarg));
      break;
    case 't':
      temp_top = INTERNAL_TEMPS(atoi(optarg));
      break;
    case 'r':
      report_temp = atoi(optarg);
      break;
    case 'v':
      verbose = true;
      break;
#if defined(CONFIG_MAVERICKS)
    case 'd':
      /* ignore upper fan tray errors */
      ignore_upper_fan_tray = 1;
      enable_high_temp_profile = 1; /* enable high temperature profile */
      base_temperature = atoi(optarg);
      if (base_temperature < 70 || base_temperature > 105) {
        printf("error in -d parameter, setting default to 95\n");
        base_temperature = 95;
      } else {
        printf("base_temperature is %d\n", base_temperature);
      }
      break;
#endif
    default:
      usage();
      break;
    }
  }

  if (optind > argc) {
    usage();
  }

  if (temp_bottom > temp_top) {
    fprintf(stderr,
            "Should temp-bottom (%d) be higher than "
            "temp-top (%d)?  Starting anyway.\n",
            EXTERNAL_TEMPS(temp_bottom),
            EXTERNAL_TEMPS(temp_top));
  }

  if (fan_low > fan_medium || fan_low > fan_high || fan_medium > fan_high) {
    fprintf(stderr,
            "fan RPMs not strictly increasing "
            "-- %d, %d, %d, starting anyway\n",
            fan_low,
            fan_medium,
            fan_high);
  }

  daemon(1, 0);

  if (verbose) {
    syslog(LOG_DEBUG, "Starting up;  system should have %d fans.",
           total_fans);
  }

#if defined(CONFIG_MAVERICKS)
  if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_PSC) {//FIXME; newport-temporary
    fan_speed =  NP_FAN_FIX;
  }
#endif
  for (fan = 0; fan < total_fans; fan++) {
    fan_bad[fan] = 0;
    write_fan_speed(fan + fan_offset, fan_speed);
    write_fan_led(fan + fan_offset, FAN_LED_BLUE);
  }

#if defined(CONFIG_MAVERICKS)
  if (mav_board_type == BF_BOARD_MAV) {
    /* additional upper fan tray */
    for (fan = total_fans; fan < (total_fans + total_fans_upper); fan++) {
      fan_bad[fan] = 0;
      write_fan_speed(fan + fan_offset, fan_speed_upper);
      write_fan_led(fan + fan_offset, FAN_LED_BLUE);
    }
  }
  if (ignore_upper_fan_tray) {
    total_fans_upper = 0;
    syslog(LOG_CRIT, "*** Not accessing upper fan tray anymore ***");
  }
#endif

#if defined(CONFIG_YOSEMITE)
  /* Ensure that we can read from sensors before proceeding. */

  int found = 0;
  userver_temp = 100;
  while (!found) {
    for (int node = 1; node <= TOTAL_1S_SERVERS && !found; node++) {
      if (!yosemite_sensor_read(node, BIC_SENSOR_SOC_THERM_MARGIN,
                               &userver_temp) &&
          userver_temp < 0) {
        syslog(LOG_DEBUG, "SOC_THERM_MARGIN first valid read of %f.",
               userver_temp);
        found = 1;
      }
      sleep(5);
    }
    // XXX:  Will it ever be a problem that we don't exit this until
    //       we see a valid value?
  }
#endif

  /* Start watchdog in manual mode */
  start_watchdog(0);

  /* Set watchdog to persistent mode so timer expiry will happen independent
   * of this process's liveliness. */
  set_persistent_watchdog(WATCHDOG_SET_PERSISTENT);

  sleep(5);  /* Give the fans time to come up to speed */

  while (1) {
    int max_temp;
    old_speed = fan_speed;
    static int newport_itr_cnt = 0;

    /* Read sensors */

#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
    read_temp(INTAKE_TEMP_DEVICE, &intake_temp);
    read_temp(EXHAUST_TEMP_DEVICE, &exhaust_temp);
    read_temp(CHIP_TEMP_DEVICE, &switch_temp);
    read_temp(USERVER_TEMP_DEVICE, &userver_temp);

    ignore_userver_temp = 0;
    if ((prev_userver_temp != 0)
        && (abs(prev_userver_temp-userver_temp) > OVER_NORMAL_TEMP_CHANGE)) {
        syslog(LOG_CRIT, "Temp userver changes unnaturally from %d to %d", prev_userver_temp, userver_temp);
        ignore_userver_temp = 1;
    }

#if defined(CONFIG_MAVERICKS)
    if (mav_board_type == BF_BOARD_MAV) {
      old_speed_upper = fan_speed_upper;
    }
    if (mav_board_type == BF_BOARD_NEW && tofino_ext_pvt_en) {
      np_read_tofino_temp_pvt(&tofino_jct_temp);
      if (tofino_jct_temp == BAD_TEMP) {
        tofino_jct_temp = TOFINO_THRESH; //FIXME; newport temporary until we understand PVT better
      }
      if (newport_itr_cnt++ >= 5) {
        /* write pvt_ctrl register, just in case it was changed in background */
        newport_itr_cnt= 0;
        np_write_tofino_pvt_ctrl();
      }
    } else {
      int *temp2 = NULL;
      if (mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
        temp2 = &tofino_jct_temp2;
      }
      mav_read_tofino_temp(&tofino_jct_temp,temp2);
    }

    if (tofino_jct_temp == BAD_TEMP || tofino_jct_temp2 == BAD_TEMP) {
      bad_reads_tofino++;
    }
    else {
      bad_reads_tofino = 0;
    }
#endif
    /*
     * uServer can be powered down, but all of the rest of the sensors
     * should be readable at any time.
     */

    /* TODO(vineelak) : Add userver_temp too , in case we fail to read temp */
    if ((intake_temp == BAD_TEMP || exhaust_temp == BAD_TEMP ||
         switch_temp == BAD_TEMP)) {
      bad_reads++;
	  //syslog(LOG_WARNING, "bad_reads++ %d", bad_reads);
    }
	else if((intake_temp != BAD_TEMP && exhaust_temp != BAD_TEMP && switch_temp != BAD_TEMP)) {
      bad_reads = 0;
	  //syslog(LOG_WARNING, "bad_reads 0");
    }
#else

    intake_temp = exhaust_temp = userver_temp = BAD_TEMP;
    if (yosemite_sensor_read(FRU_SPB, SP_SENSOR_INLET_TEMP, &intake_temp) ||
        yosemite_sensor_read(FRU_SPB, SP_SENSOR_OUTLET_TEMP, &exhaust_temp))
      bad_reads++;

    /*
     * There are a number of 1S servers;  any or all of them
     * could be powered off and returning no values.  Ignore these
     * invalid values.
     */
    for (int node = 1; node <= TOTAL_1S_SERVERS; node++) {
      float new_temp;
      if (!yosemite_sensor_read(node, BIC_SENSOR_SOC_THERM_MARGIN,
                    &new_temp)) {
        if (userver_temp < new_temp) {
          userver_temp = new_temp;
        }
      }

      // Since the yosemite_sensor_read() times out after 8secs, keep WDT from expiring
      kick_watchdog();
    }
#endif

    if (bad_reads > BAD_READ_THRESHOLD) {
      server_shutdown("Some sensors couldn't be read");
    }

#if defined(CONFIG_MAVERICKS)
    if (bad_reads_tofino > BAD_READ_THRESHOLD) {
      syslog(LOG_CRIT, "resetting Tofino: bad read");
      server_shutdown("Tofino sensors couldn't be read");
    }
#endif

    if ((log_count++ % report_temp == 0) ||
        (intake_temp > INTAKE_LIMIT) ||
#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
        (switch_temp > SWITCH_LIMIT) ||
#endif
#if defined(CONFIG_MAVERICKS)
        (tofino_jct_temp > TOFINO_LIMIT) || (tofino_jct_temp2 > TOFINO_LIMIT) ||
#endif
        ((ignore_userver_temp == 0)&&(userver_temp + USERVER_TEMP_FUDGE > USERVER_LIMIT))){
      syslog(LOG_DEBUG,
#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
             "Temp intake %d, switch %d, "
             "userver %d, exhaust %d, "
             "fan speed %d, speed changes %d",
#else
             "Temp intake %f, max server %f, exhaust %f, "
             "fan speed %d, speed changes %d",
#endif
             intake_temp,
#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
             switch_temp,
#endif
             userver_temp,
             exhaust_temp,
             fan_speed,
             fan_speed_changes);
 
#if defined(CONFIG_MAVERICKS)
      syslog(LOG_DEBUG, "Temp Tofino %d  Tofino2 %d", tofino_jct_temp, tofino_jct_temp2);
#endif
    }

    /* Protection heuristics */

    if (intake_temp > INTAKE_LIMIT) {
      syslog(LOG_CRIT, "previous Temp intake %d", prev_intake_temp);
      server_shutdown("Intake temp limit reached");
    }
  #if defined(SERVER_TEST)
    if (intake_temp > INTERNAL_TEMPS(50)) {
      syslog(LOG_WARNING, "previous Temp intake %d", intake_temp);
    }
  #endif
#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
    if (switch_temp > SWITCH_LIMIT) {
      syslog(LOG_CRIT, "previous Temp switch %d", prev_switch_temp);
      server_shutdown("T2 temp limit reached");
    }
  #if defined(SERVER_TEST)
    if (switch_temp > INTERNAL_TEMPS(50)) {
      syslog(LOG_WARNING, "previous Temp switch %d", switch_temp);
    }
  #endif
#endif

#if defined(CONFIG_MAVERICKS)
    if ((0xff != tofino_jct_temp) && (BAD_TEMP != tofino_jct_temp)) {
        if(abs(tofino_jct_temp - prev_tofino_jct_temp) <= TOFINO_DIFF) {
            if (tofino_jct_temp > TOFINO_LIMIT) {
              syslog(LOG_CRIT, "previous Tofino temp %d", prev_tofino_jct_temp);
              server_shutdown("Tofino temp limit reached");
            }

            #if defined(SERVER_TEST)
            if (tofino_jct_temp > 49) {
              syslog(LOG_WARNING, "previous Tofino temp %d", tofino_jct_temp);
            }
            #endif
        }
        prev_tofino_jct_temp = tofino_jct_temp;
    }
    if ((mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC)
          && (0xff != tofino_jct_temp2) && (BAD_TEMP != tofino_jct_temp2)) {
        if(abs(tofino_jct_temp2 - prev_tofino_jct_temp2) <= TOFINO_DIFF) {
            if (tofino_jct_temp2 > TOFINO_LIMIT) {
              syslog(LOG_CRIT, "previous Tofino temp2 %d", prev_tofino_jct_temp2);
              server_shutdown("Tofino temp2 limit reached");
            }

            #if defined(SERVER_TEST)
            if (tofino_jct_temp2 > 49) {
              syslog(LOG_WARNING, "previous Tofino temp2 %d", tofino_jct_temp2);
            }
            #endif
        }
        prev_tofino_jct_temp2 = tofino_jct_temp2;
    }
#endif

    if ((ignore_userver_temp == 0)&&(userver_temp + USERVER_TEMP_FUDGE > USERVER_LIMIT)) {
      syslog(LOG_CRIT, "previous Temp userver %d", prev_userver_temp);
      server_shutdown("uServer temp limit reached");
    }
#if defined(SERVER_TEST)
    if ((ignore_userver_temp == 0)&&(userver_temp + USERVER_TEMP_FUDGE > INTERNAL_TEMPS(52))) {
      syslog(LOG_WARNING, "previous Temp userver %d", userver_temp);
    }
#endif

    prev_intake_temp = intake_temp;
    prev_exhaust_temp = exhaust_temp;
    prev_switch_temp = switch_temp;
    prev_userver_temp = userver_temp;

    /*
     * Calculate change needed -- we should eventually
     * do something more sophisticated, like PID.
     *
     * We should use the intake temperature to adjust this
     * as well.
     */

#if defined(CONFIG_YOSEMITE)
    /* Use tables to lookup the new fan speed for Yosemite. */

    int intake_speed = temp_to_fan_speed(intake_temp, intake_map,
                                         INTAKE_MAP_SIZE);
    int cpu_speed = temp_to_fan_speed(userver_temp, cpu_map, CPU_MAP_SIZE);

    if (fan_speed == fan_max && fan_failure != 0) {
      /* Don't change a thing */
    } else if (intake_speed > cpu_speed) {
      fan_speed = intake_speed;
    } else {
      fan_speed = cpu_speed;
    }
#else
    /* Other systems use a simpler built-in table to determine fan speed. */

    if (switch_temp > userver_temp + USERVER_TEMP_FUDGE) {
      max_temp = switch_temp;
    } else {
      max_temp = userver_temp + USERVER_TEMP_FUDGE;
    }

    /*
     * If recovering from a fan problem, spin down fans gradually in case
     * temperatures are still high. Gradual spin down also reduces wear on
     * the fans.
     */
    if (fan_speed == fan_max) {
      if (fan_failure == 0) {
        fan_speed = fan_high;
      }
    } else if (fan_speed == fan_high) {
      if (max_temp + COOLDOWN_SLOP < temp_top) {
        fan_speed = fan_medium;
      }
    } else if (fan_speed == fan_medium) {
      if (max_temp > temp_top) {
        fan_speed = fan_high;
      } else if (max_temp + COOLDOWN_SLOP < temp_bottom) {
        fan_speed = fan_low;
      }
    } else {/* low */
      if (max_temp > temp_bottom) {
        fan_speed = fan_medium;
      }
    }

#if defined(CONFIG_MAVERICKS)

    if (mav_board_type == BF_BOARD_MON || mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_PSC) {
      /* bump up the fan speed if Tofino temp mandates */
      if (tofino_jct_temp > TOFINO_THRESH) {
        /* change the lower fan tray speed if Tofino temp is high */
        fan_speed = fan_max;
      } else if (tofino_jct_temp > TOFINO_MED) {
        if (fan_speed < fan_high) {
          fan_speed = fan_high;
        }
      } else if (tofino_jct_temp > TOFINO_LOW) {
        if (fan_speed < fan_medium) {
          fan_speed = fan_medium;
        }
      }
    } else if (mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
      /* bump up the fan speed if Tofino temp mandates */
      if (tofino_jct_temp > TOFINO_THRESH || tofino_jct_temp2 > TOFINO_THRESH) {
        /* change the lower fan tray speed if Tofino temp is high */
        fan_speed = fan_max;
      } else if (tofino_jct_temp > TOFINO_MED || tofino_jct_temp2 > TOFINO_MED) {
        if (fan_speed < fan_high) {
          fan_speed = fan_high;
        }
      } else if (tofino_jct_temp > TOFINO_LOW || tofino_jct_temp2 > TOFINO_LOW) {
        if (fan_speed < fan_medium) {
          fan_speed = fan_medium;
        }
      }
    } else { /* mavericks: handle upper fan tray speed */
      if (tofino_jct_temp > TOFINO_THRESH) {
        fan_speed_upper = MAV_FAN_MAX;
      } else if (fan_speed_upper == MAV_FAN_MAX) {
        if (fan_failure_upper  == 0) {
          if (tofino_jct_temp < TOFINO_HIGH) {
            fan_speed_upper = MAV_FAN_HIGH;
          }
        }
      } else if (fan_speed_upper ==  MAV_FAN_HIGH) {
        if (tofino_jct_temp < TOFINO_MED) {
          fan_speed_upper = MAV_FAN_MEDIUM;
        }
      } else if (fan_speed_upper ==  MAV_FAN_MEDIUM) {
        if (tofino_jct_temp < TOFINO_LOW) {
          fan_speed_upper = MAV_FAN_LOW;
        } else if (tofino_jct_temp > TOFINO_MED) {
          fan_speed_upper = MAV_FAN_HIGH;
        }
      } else if (fan_speed_upper ==  MAV_FAN_LOW) {
        if (tofino_jct_temp > TOFINO_MED) {
          fan_speed_upper = MAV_FAN_HIGH;
        }
      }
    }
#endif

#endif

    /*
     * Update fans only if there are no failed ones. If any fans failed
     * earlier, all remaining fans should continue to run at max speed.
     */

    if (fan_failure == 0 && fan_speed != old_speed) {
      syslog(LOG_NOTICE,
             "Fan speed changing from %d to %d",
             old_speed,
             fan_speed);
      fan_speed_changes++;
      for (fan = 0; fan < total_fans; fan++) {
        write_fan_speed(fan + fan_offset, fan_speed);
      }
    }

#if defined(CONFIG_MAVERICKS)
    if (mav_board_type == BF_BOARD_MAV) {
      if (fan_failure_upper == 0 && fan_speed_upper != old_speed_upper) {
        syslog(LOG_NOTICE,
             "Upper fan speed changing from %d to %d",
             old_speed_upper,
             fan_speed_upper);
        fan_speed_changes_upper++;
        for (fan = total_fans; fan < (total_fans + total_fans_upper); fan++) {
          write_fan_speed(fan + fan_offset, fan_speed_upper);
        }
      }
    }
#endif

    /*
     * Wait for some change.  Typical I2C temperature sensors
     * only provide a new value every second and a half, so
     * checking again more quickly than that is a waste.
     *
     * We also have to wait for the fan changes to take effect
     * before measuring them.
     */

    sleep(5);

    /* Check fan RPMs */

    for (fan = 0; fan < total_fans; fan++) {
      /*
       * Make sure that we're within some percentage
       * of the requested speed.
       */
      if (fan_speed_okay(fan + fan_offset, fan_speed, FAN_FAILURE_OFFSET)) {
        if (fan_bad[fan] > FAN_FAILURE_THRESHOLD) {
          write_fan_led(fan + fan_offset, FAN_LED_BLUE);
          syslog(LOG_CRIT,
                 "Fan %d has recovered",
                 fan);
        }
        fan_bad[fan] = 0;
      } else {
        fan_bad[fan]++;
      }
    }

    fan_failure = 0;
    for (fan = 0; fan < total_fans; fan++) {
      if (fan_bad[fan] > FAN_FAILURE_THRESHOLD) {
        fan_failure++;
#if defined(CONFIG_MAVERICKS)
        if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
            write_device(FAN_LED_DEBUG_MODE, "1"); /*LED debug mode on because fan LED change requires */
        }
#endif
        write_fan_led(fan + fan_offset, FAN_LED_RED);
      }
    }

    if (fan_failure > 0) {
      if (prev_fans_bad != fan_failure) {
        syslog(LOG_CRIT, "%d fans failed", fan_failure);
      }

      /*
       * If fans are bad, we need to blast all of the
       * fans at 100%;  we don't bother to turn off
       * the bad fans, in case they are all that is left.
       *
       * Note that we have a temporary bug with setting fans to
       * 100% so we only do fan_max = 99%.
       */

      fan_speed = fan_max;
      for (fan = 0; fan < total_fans; fan++) {
        write_fan_speed(fan + fan_offset, fan_speed);
      }

#if defined(CONFIG_WEDGE) || defined(CONFIG_WEDGE100) \
                          || defined(CONFIG_MAVERICKS)
      /*
       * On Wedge, we want to shut down everything if none of the fans
       * are visible, since there isn't automatic protection to shut
       * off the server or switch chip.  On other platforms, the CPUs
       * generating the heat will automatically turn off, so this is
       * unnecessary.
       */

      if (fan_failure == total_fans) {
        int count = 0;
        for (fan = 0; fan < total_fans; fan++) {
          if (fan_bad[fan] > FAN_SHUTDOWN_THRESHOLD)
            count++;
        }
        if (count == total_fans) {
          server_shutdown("all fans are bad for more than 12 cycles");
        }
      }
#endif

      /*
       * Fans can be hot swapped and replaced; in which case the fan daemon
       * will automatically detect the new fan and (assuming the new fan isn't
       * itself faulty), automatically readjust the speeds for all fans down
       * to a more suitable rpm. The fan daemon does not need to be restarted.
       */
    }
#if defined(CONFIG_MAVERICKS)
    else {
        if (mav_board_type == BF_BOARD_NEW || mav_board_type == BF_BOARD_STN || mav_board_type == BF_BOARD_DVN || mav_board_type == BF_BOARD_PSC) {
            write_device(FAN_LED_DEBUG_MODE, "0"); /*LED debug mode off*/
        }
    }
#endif

    /* Suppress multiple warnings for similar number of fan failures. */
    prev_fans_bad = fan_failure;

#if defined(CONFIG_MAVERICKS)
    if (mav_board_type == BF_BOARD_MAV) {
      /* upper fan tray  on Mavericks */
      /* Check fan RPMs */
      for (fan = total_fans; fan < (total_fans + total_fans_upper); fan++) {
        /*
         * Make sure that we're within some percentage
         * of the requested speed.
         */
        if (fan_speed_okay(fan + fan_offset, fan_speed_upper,
                           FAN_FAILURE_OFFSET)) {
          if (fan_bad[fan] > FAN_FAILURE_THRESHOLD) {
            write_fan_led(fan + fan_offset, FAN_LED_BLUE);
            syslog(LOG_CRIT,
                   "Fan %d has recovered",
                   fan);
          }
          fan_bad[fan] = 0;
        } else {
          fan_bad[fan]++;
        }
      }

      fan_failure_upper = 0;
      for (fan = total_fans; fan < (total_fans + total_fans_upper); fan++) {
        if (fan_bad[fan] > FAN_FAILURE_THRESHOLD) {
          /* FIXME: Not all mavericks have upper FAN tray always mounted.
           * so, dont count errors due to absent upper fan tray, for now.
           * at some point, below "#if" construct has to go away.
           */
          fan_failure_upper++;
          write_fan_led(fan + fan_offset, FAN_LED_RED);
        }
      }

      if (fan_failure_upper > 0) {
        if (prev_fans_bad_upper != fan_failure_upper) {
          syslog(LOG_CRIT, "%d upper fans failed", fan_failure_upper);
        }

        /*
         * If fans are bad, we need to blast all of the
         * fans at 100%;  we don't bother to turn off
         * the bad fans, in case they are all that is left.
         *
         * Note that we have a temporary bug with setting fans to
         * 100% so we only do fan_max = 99%.
         */

        fan_speed_upper = fan_max;
        for (fan = total_fans; fan < (total_fans + total_fans_upper); fan++) {
          write_fan_speed(fan + fan_offset, fan_speed_upper);
        }

        /*
         * On Wedge, we want to shut down everything if none of the fans
         * are visible, since there isn't automatic protection to shut
         * off the server or switch chip.  On other platforms, the CPUs
         * generating the heat will automatically turn off, so this is
         * unnecessary.
         */

        if (fan_failure_upper == total_fans_upper) {
          int count = 0;
          for (fan = total_fans; fan < (total_fans + total_fans_upper); fan++) {
            if (fan_bad[fan] > FAN_SHUTDOWN_THRESHOLD)
              count++;
          }
          if (count == total_fans_upper) {
            server_shutdown("all upper fans are bad for more than 12 cycles");
          }
        }
      }

      /* Suppress multiple warnings for similar number of fan failures. */
      prev_fans_bad_upper = fan_failure_upper;
      for (fan = 0; fan < (total_fans + total_fans_upper); fan++) {
        if (fan_bad[fan] > 0) {
          syslog(LOG_CRIT,"fan %d bad %d\n", fan, fan_bad[fan]);
        }
      }
    } else {
      for (fan = 0; fan < (total_fans); fan++) {
        if (fan_bad[fan] > 0) {
          syslog(LOG_CRIT,"fan %d bad %d\n", fan, fan_bad[fan]);
        }
      }
    }
#endif

    /* if everything is fine, restart the watchdog countdown. If this process
     * is terminated, the persistent watchdog setting will cause the system
     * to reboot after the watchdog timeout. */
    kick_watchdog();
  }
#endif
}
