/*
 * pid.c
 *
 *  Created on: May 21, 2025
 *      Author: Luin
 */
#include "pid.h"

void PIDController_Init(PIDController *pid) {

    pid->Kp = PID_KP;
    pid->Ki = PID_KI;
    pid->Kd = PID_KD;
    pid->tau = PID_TAU;
    pid->limMin = PID_LIM_MIN;
    pid->limMax = PID_LIM_MAX;
    pid->limMinInt = PID_LIM_MIN_INT;
    pid->limMaxInt = PID_LIM_MAX_INT;
    pid->T = SAMPLE_TIME_S;

    pid->integrator = 0.0f;
    pid->prevError  = 0.0f;

    pid->differentiator  = 0.0f;
    pid->prevMeasurement = 0.0f;

    pid->out = 0.0f;

}

float PIDController_Update(PIDController *pid, float setpoint, float measurement) {

    /*
    * Tinh loi (error)
    */
    float error = setpoint - measurement; // Loi giua gia tri dat va gia tri do luong

    /*
    * Thanh phan ti le (Proportional)
    */
    float proportional = pid->Kp * error;

    /*
    * Thanh phan tich phan (Integral)
    */
    pid->integrator = pid->integrator + 0.5f * pid->Ki * pid->T * (error + pid->prevError); //Dung phuong phap hinh thang

    /* Chong tich phan qua lon (Anti-wind-up) */
    if (pid->integrator > pid->limMaxInt) {

        pid->integrator = pid->limMaxInt; // Gioi han tich phan lon nhat

    } else if (pid->integrator < pid->limMinInt) {

        pid->integrator = pid->limMinInt; // Gioi han tich phan nho nhat
    }

    /*
    * Thanh phan dao ham (Derivative) - Derivative on measurement với low-pass filter
    * Dùng measurement thay vì error để tránh derivative kick khi setpoint thay đổi
    */
    pid->differentiator = -(2.0f * pid->Kd * (measurement - pid->prevMeasurement) 
                            + (2.0f * pid->tau - pid->T) * pid->differentiator) 
                            / (2.0f * pid->tau + pid->T);


    /*
    * Tinh dau ra va ap dung gioi han
    */
    pid->out = proportional + pid->integrator + pid->differentiator;

    if (pid->out > pid->limMax) {

        pid->out = pid->limMax; // Gioi han dau ra lon nhat

    } else if (pid->out < pid->limMin) {

        pid->out = pid->limMin; // Gioi han dau ra nho nhat

    }

    pid->prevError       = error; // Luu loi hien tai
    pid->prevMeasurement = measurement; // Luu do luong hien tai

    /* Tra ve dau ra cua bo dieu khien */
    return pid->out;

}

