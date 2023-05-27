#include "utils.h"

double dmap(double x, double in_min, double in_max, double out_min, double out_max) {
    double val = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if (val < out_min) return out_min;
    if (val > out_max) return out_max;
    return val;
}

double dmapLowConstrain(double x, double in_min, double in_max, double out_min, double out_max) {
    double val = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if (val < out_min) return out_min;
    return val;
}
