//
// Created by Lao·Zhu on 2021/9/2.
//

#include "encoder.h"
#include "spi.h"
#include "gd32f1x0.h"
#include "foc.h"
#include "timer.h"
#include "system.h"
#include "config.h"

#define MECHANGLE_COEFFICIENT   6.2831854f / ENCODER_RESO
#define ELECANGLE_COEFFICIENT   (6.2831854f * POLAR_PAIRS) / ENCODER_RESO
#define SPEED_COEFFICIENT       0.4882812        // rps

unsigned short machine_angle_offset = 0;
unsigned short last_mechanical_angle = 0;
unsigned long total_machine_angle = 0;

/*!
    \brief      delay function for magnetic encoder
    \param[in]  none
    \param[out] none
    \retval     none
*/
void encoder_delay(void) {
    unsigned char delay_counter = 0xff;
    while (delay_counter)
        delay_counter--;
}

/*!
    \brief      read data from the register of the magnetic encoder
    \param[in]  TxData: data to be sent to magnetic encoder
    \param[out] none
    \retval     data read from magnetic encoder
*/
unsigned short encoder_read_data(unsigned short TxData) {
    unsigned short data;
    /* pull down the CS pin and prepare to send data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, RESET);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* call SPI related functions to send and receive data */
    data = spi_readwrite_halfworld(TxData);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* pull up CS pin to end sending data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, SET);
    return data;
}

/*!
    \brief      register for configuring magnetic encoder
    \param[in]  none
    \param[out] none
    \retval     none
*/
void encoder_config(void) {
    /* delay 100 ms to ensure the normal startup of the magnetic encoder chip */
    delayms(100);
    /* pull down the CS pin and prepare to send data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, RESET);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* set io current 60mA 10kHz, ab phase STAY, pin mode PP_OUTPUT*/
    spi_readwrite_halfworld(0x47CE);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* pull up CS pin to end sending data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, SET);

    delayms(1);
    /* pull down the CS pin and prepare to send data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, RESET);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* set zero position angle to 0, polar to 1*/
    spi_readwrite_halfworld(0x452F);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* pull up CS pin to end sending data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, SET);

    delayms(1);
    /* pull down the CS pin and prepare to send data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, RESET);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* set zero position angle to 0*/
    spi_readwrite_halfworld(0x44FF);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* pull up CS pin to end sending data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, SET);

    delayms(1);
    /* pull down the CS pin and prepare to send data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, RESET);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* set zero width as LSB1_WIDTH*/
    spi_readwrite_halfworld(0x4840);
    /* delay preventing the timing between CS and CLK from exceeding the standard */
    encoder_delay();
    /* pull up CS pin to end sending data */
    gpio_bit_write(GPIOA, GPIO_PIN_4, SET);

    /* read angle data once */
    encoder_read_data(0x0000);
}

/*!
    \brief      read mechanical angle directly from encoder
    \param[in]  none
    \param[out] none
    \retval     register raw data reading back
*/
unsigned short encoder_get_mechanical_angle(void) {
    /* read back register raw data */
    unsigned short angle = encoder_read_data(0x0000) >> 4;
    /* statistical total rotation angle */
    total_machine_angle = total_machine_angle + angle - last_mechanical_angle;
    last_mechanical_angle = angle;
    /* returns the result of subtracting the offset. */
    return ((total_machine_angle - machine_angle_offset) % ENCODER_RESO);
}

/*!
    \brief      according to the electrical angle calculated from the mechanical angle,
                this function will call encoder_get_mechanical_angle() function.
    \param[in]  none
    \param[out] none
    \retval     register raw data reading back
*/
float encoder_get_electronic_angle(void) {
    unsigned short tmp_mechanical_angle = encoder_get_mechanical_angle();
    float mechanical_angle = (float) tmp_mechanical_angle * MECHANGLE_COEFFICIENT;
    float electric_angle = (float) (tmp_mechanical_angle % (ENCODER_RESO / POLAR_PAIRS)) * ELECANGLE_COEFFICIENT;
    return electric_angle;
}

//unsigned long systick_mechanical_angle_last = 0;
//void Update_AngleSpeed(void) {
//    unsigned short tmp_mechanical_angle_velocity = total_machine_angle - systick_mechanical_angle_last;
//    FOC_Structure.velocity =
//        FirstOrder_Filter_Calculate(&Velocity_f, qfp_fmul(tmp_mechanical_angle_velocity, SPEED_COEFFICIENT));
//    systick_mechanical_angle_last = total_machine_angle;
//}

/*!
    \brief      correct the mechanical angle zero deviation between the magnetic encoder and FOC.
    \param[in]  none
    \param[out] none
    \retval     none
*/
void encoder_zeroing(void) {
    float u, v, w;
    /* set that there is only a magnetic field on the straight axis. */
    foc_calculate_dutycycle(0, 0.6f, 0, &u, &v, &w);
    /* Apply to PWM */
    update_pwm_dutycycle(u, v, w);
    /* delay to wait for the motor to respond */
    delayms(250);
    machine_angle_offset = 0;
    total_machine_angle = 0;
    /* read the angle at this time as the offset angle */
    machine_angle_offset = encoder_get_mechanical_angle();
    /* zero the torque in all directions to release the motor */
    foc_calculate_dutycycle(0, 0, 0, &u, &v, &w);
    /* Apply to PWM */
    update_pwm_dutycycle(u, v, w);
}