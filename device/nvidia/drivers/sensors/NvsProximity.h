/* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef NVS_PROXIMITY_H
#define NVS_PROXIMITY_H

#include "Nvs.h"


class NvsProximity: public Nvs {

public:
    NvsProximity(int devNum, enum NVS_DRIVER_TYPE driverType, Nvs *link = NULL);
    virtual ~NvsProximity();
};

#endif /* NVS_PROXIMITY_H */

