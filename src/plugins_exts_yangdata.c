/**
 * @file plugins_exts_yangdata.c
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief libyang extension plugin - yang-data (RFC 8040)
 *
 * Copyright (c) 2021 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#include "plugins_exts_yangdata.h"

#include <stdlib.h>

#include "common.h"
#include "plugins_exts.h"
#include "schema_compile.h"
#include "tree_schema.h"

/**
 * @brief Storage for ID used to check plugin API version compatibility.
 * Ignored here in the internal plugin.
LYEXT_VERSION_CHECK
 */

/**
 * @brief Free yang-data extension instances' data.
 *
 * Implementation of ::lyext_clb_free callback set as lyext_plugin::free.
 */
void
yangdata_free(struct ly_ctx *ctx, struct lysc_ext_instance *ext)
{
    struct lyext_yangdata *yangdata = (struct lyext_yangdata *)ext->data;

    if (yangdata) {
        struct lysc_ext_substmt yangdata_substmt[] = {
                {LY_STMT_CONTAINER, LY_STMT_CARD_ANY, &yangdata->data}, /* matches any lysc_node, not just containers */
                {0, 0, 0} /* terminating item */
        };

        lysc_extension_instance_free(ctx, yangdata_substmt);
        free(ext->data);
    }
}

/**
 * @brief Compile yang-data extension instances.
 *
 * Implementation of lyext_clb_compile callback set as lyext_plugin::compile.
 */
LY_ERR
yangdata_compile(struct lysc_ctx *cctx, const struct lysp_ext_instance *p_ext, struct lysc_ext_instance *c_ext)
{
    struct lyext_yangdata *yangdata;
    struct lysc_module *mod_c;
    LY_ARRAY_COUNT_TYPE u;

    /* yang-data can appear only at the top level of a YANG module or submodule */
    if (c_ext->parent_type != LYEXT_PAR_MODULE) {
        lyext_log(c_ext, LY_LLWRN, 0, cctx->path,
                "Extension %s is ignored since it appears as a non top-level statement in \"%s\" statement.",
                p_ext->name, lyext_parent2str(c_ext->parent_type));
        return LY_ENOT;
    }
    /* check mandatory argument */
    if (!c_ext->argument) {
        lyext_log(c_ext, LY_LLERR, LY_EVALID, cctx->path,
                "Extension %s is instantiated without mandatory argument representing YANG data template name.",
                p_ext->name);
        return LY_EVALID;
    }

    mod_c = (struct lysc_module *)c_ext->parent;

    /* check for duplication */
    LY_ARRAY_FOR(mod_c->exts, u) {
        if ((&mod_c->exts[u] != c_ext) && (mod_c->exts[u].def == c_ext->def) && !strcmp(mod_c->exts[u].argument, c_ext->argument)) {
            /* duplication of the same yang-data extension in a single module */
            lyext_log(c_ext, LY_LLERR, LY_EVALID, cctx->path, "Extension %s is instantiated multiple times.", p_ext->name);
            return LY_EVALID;
        }
    }

    /* compile annotation substatements */
    c_ext->data = yangdata = calloc(1, sizeof *yangdata);
    if (!yangdata) {
        LOGMEM(cctx->ctx);
        return LY_EMEM;
    } else {
        LY_ERR ret;
        struct lysc_ext_substmt yangdata_substmt[] = {
                {LY_STMT_CONTAINER, LY_STMT_CARD_OPT, &yangdata->data},
                {LY_STMT_CHOICE, LY_STMT_CARD_OPT, &yangdata->data},
                {LY_STMT_USES, LY_STMT_CARD_OPT, &yangdata->data},
                {0, 0, 0} /* terminating item */
        };
        ly_bool valid = 1;
        uint32_t prev_options = cctx->options;

        cctx->options |= LYS_COMPILE_NO_CONFIG;
        ret = lys_compile_extension_instance(cctx, p_ext, yangdata_substmt);
        cctx->options = prev_options;
        LY_CHECK_RET(ret);

        /* check that we have really just a single container data definition in the top */
        if (!yangdata->data) {
            valid = 0;
            lyext_log(c_ext, LY_LLERR, LY_EVALID, cctx->path,
                    "Extension %s is instantiated without any top level data node, but exactly one container data node is expected.",
                    p_ext->name);
        } else if (yangdata->data->next) {
            valid = 0;
            lyext_log(c_ext, LY_LLERR, LY_EVALID, cctx->path,
                    "Extension %s is instantiated with multiple top level data nodes, but only a single container data node is allowed.",
                    p_ext->name);
        } else if (yangdata->data->nodetype == LYS_CHOICE) {
            /* all the choice's case are expected to result to a single container node */
            const struct lysc_node *snode = NULL;

            while ((snode = lys_getnext(snode, yangdata->data, mod_c, 0))) {
                if (snode->next) {
                    valid = 0;
                    lyext_log(c_ext, LY_LLERR, LY_EVALID, cctx->path,
                            "Extension %s is instantiated with multiple top level data nodes (inside a single choice's case), "
                            "but only a single container data node is allowed.", p_ext->name);
                    break;
                } else if (snode->nodetype != LYS_CONTAINER) {
                    valid = 0;
                    lyext_log(c_ext, LY_LLERR, LY_EVALID, cctx->path,
                            "Extension %s is instantiated with %s top level data node (inside a choice), "
                            "but only a single container data node is allowed.", p_ext->name, lys_nodetype2str(snode->nodetype));
                    break;
                }
            }
        } else if (yangdata->data->nodetype != LYS_CONTAINER) {
            /* via uses */
            valid = 0;
            lyext_log(c_ext, LY_LLERR, LY_EVALID, cctx->path,
                    "Extension %s is instantiated with %s top level data node, but only a single container data node is allowed.",
                    p_ext->name, lys_nodetype2str(yangdata->data->nodetype));
        }

        if (!valid) {
            yangdata_free(cctx->ctx, c_ext);
            c_ext->data = NULL;
            return LY_EVALID;
        }

        return LY_SUCCESS;
    }
}

/**
 * @brief Plugin for the yang-data extension
 */
struct lyext_plugin yangdata_plugin = {
    .id = "libyang 2 - yang-data, version 1",
    .compile = &yangdata_compile,
    .validate = NULL,
    .free = yangdata_free
};
