#include "EmonLib.h" // Include Emon Library

float cosphi = 0.928;

//Curent: input pin, calibration.
emon1.current(8, 51.5);

double Irms = emon1.calcIrms(1480); // Calculate Irms only
float watts = Irms*230.0*cosphi;
  
