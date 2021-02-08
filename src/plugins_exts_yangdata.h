/**
 * @file plugins_exts_yangdata.h
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief ietf-yang-yangdata API
 *
 * Copyright (c) 2021 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef LY_PLUGINS_EXTS_YANGDATA_H_
#define LY_PLUGINS_EXTS_YANGDATA_H_

#include "tree_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Representation of the compiled yang-data substatements as provided by libyang 2 yang-data extension plugin.
 */
struct lyext_yangdata {
    struct lysc_node *data;            /**< template's data definition - exactly one container data definition */
};

#ifdef __cplusplus
}
#endif

#endif /* LY_PLUGINS_EXTS_YANGDATA_H_ */
