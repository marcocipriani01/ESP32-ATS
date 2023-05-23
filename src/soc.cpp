#include "soc.h"

double calcSoc(double voltage) {
    if (voltage <= SOC_TABLE_LIFEPO4[0][0])
        return SOC_TABLE_LIFEPO4[0][1];
    const uint8_t table_size = sizeof(SOC_TABLE_LIFEPO4) / sizeof(SOC_TABLE_LIFEPO4[0]);
    for (uint8_t i = 1; i < table_size; i++) {
        if (voltage <= SOC_TABLE_LIFEPO4[i][0])
            return dmap(voltage, SOC_TABLE_LIFEPO4[i - 1][0], SOC_TABLE_LIFEPO4[i][0], SOC_TABLE_LIFEPO4[i - 1][1], SOC_TABLE_LIFEPO4[i][1]);
    }
    return SOC_TABLE_LIFEPO4[table_size - 1][1];
}
