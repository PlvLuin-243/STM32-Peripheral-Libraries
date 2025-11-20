/*
 * pid.h
 *
 *  Created on: May 21, 2025
 *      Author: Luin
 */

 #ifndef PID_H_
 #define PID_H_
 #ifndef PID_CONTROLLER_H
 #define PID_CONTROLLER_H
 
 // Cac he so PID
 #define PID_KP  2.0f       // He so ti le (Proportional)
 #define PID_KI  6.0f       // He so tich phan (Integral)
 #define PID_KD  0.0f        // He so vi phan (Derivative)
 
 // Thoi gian loc (filter time) cho dao ham
 #define PID_TAU 0.02f
 
 // Gioi han dau ra (Output limits)
 #define PID_LIM_MIN  0.0f   // Gioi han toi thieu
 #define PID_LIM_MAX  9999.0f // Gioi han toi da
 
 // Gioi han tich luy (Integral term limits)
 #define PID_LIM_MIN_INT  0.0f   // Gioi han toi thieu cho tich phan
 #define PID_LIM_MAX_INT  9999.0f // Gioi han toi da cho tich phan
 
 // Thoi gian lay mau (sampling time)
 #define SAMPLE_TIME_S 0.005f
 
 // Cau truc PIDController
 typedef struct {
 
     /* He so dieu khien */
     double Kp; // He so ti le
     double Ki; // He so tich phan
     double Kd; // He so dao ham
 
     /* Hang so bo loc thong thap cho dao ham */
     float tau;
 
     /* Gioi han dau ra */
     float limMin;
     float limMax;
 
     /* Gioi han tich phan */
     float limMinInt;
     float limMaxInt;
 
     /* Thoi gian lay mau (tinh bang giay) */
     float T;
 
     float integrator; // Gia tri tich phan
     float prevError; // Loi truoc do
     float differentiator; // Gia tri dao ham
     float prevMeasurement; // Do luong truoc do
 
     /* Dau ra cua bo dieu khien */
     float out;
 
 } PIDController;
 
 extern PIDController pid;
 // Ham khoi tao bo dieu khien PID
 void  PIDController_Init(PIDController *pid);
 
 // Ham cap nhat bo dieu khien PID
 float PIDController_Update(PIDController *pid, float setpoint, float measurement);
 
 #endif
 
 
 #endif /* PID_H_ */
 