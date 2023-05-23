#ifndef SOC_H
#define SOC_H

#include <Arduino.h>

#include "utils.h"

const double SOC_TABLE_LIFEPO4[][2] = {
    {10.0, 0.0},
    {12.0, 9.0},
    {12.5, 14.0},
    {12.8, 17.0},
    {12.9, 20.0},
    {13.0, 30.0},
    {13.1, 40.0},
    {13.2, 70.0},
    {13.3, 90.0},
    {13.4, 98.0},
    {13.6, 100.0}
};

double calcSoc(double voltage);

#endif
