//
// Created by inż. Dawid Pisarczyk on 28.12.2025.
//

#include "can_converter.h"

K_MUTEX_DEFINE(data_mutex);
master_data_t data = {0};
