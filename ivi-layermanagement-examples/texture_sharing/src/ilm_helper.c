/**
 * \file: ilm_helper.c
 *
 * \version: $Id:$
 *
 * \release: $Name:$
 *
 * <brief description>.
 * <detailed description>
 * \component: <componentname>
 *
 * \author: <author>
 *
 * \copyright (c) 2012, 2013 Advanced Driver Information Technology.
 * This code is developed by Advanced Driver Information Technology.
 * Copyright of Advanced Driver Information Technology, Bosch, and DENSO.
 * All rights reserved.
 *
 * \see <related items>
 *
 * \history
 * <history item>
 * <history item>
 * <history item>
 *
 ***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ilm_helper.h"

int g_ilm_init;

/**
 * \func   create_ilm_context
 *
 * \param  p_params: parameters for create Surface and Layer
 *
 * \return int: return status
 *
 * \see    ilm_helper.h
 */
int
create_ilm_context(ilm_params *p_param)
{
    t_ilm_int n_handle = 0;
    t_ilm_nativehandle *p_handle = NULL;
    t_ilm_surface surface_id = p_param->surface_id;
    t_ilm_layer layer_id = p_param->layer_id;

    if (0 == g_ilm_init)
    {
        ilm_initWithNativedisplay((t_ilm_nativedisplay)p_param->wlDisplay);
        g_ilm_init = 1;
    }

    /* Surface creation */
    ilm_surfaceCreate((t_ilm_nativehandle)p_param->nativehandle, p_param->surface_width,
        p_param->surface_height, ILM_PIXELFORMAT_RGBA_8888, &surface_id);

    ilm_surfaceSetSourceRectangle(surface_id, 0, 0,
        p_param->surface_width, p_param->surface_height);

    ilm_surfaceSetDestinationRectangle(surface_id, 0, 0,
        p_param->surface_width, p_param->surface_height);

    ilm_surfaceSetOpacity(surface_id, 1.0f);

    ilm_surfaceSetVisibility(surface_id, ILM_TRUE);

    /* Layer creation */
    ilm_layerCreateWithDimension(&layer_id, p_param->layer_width,
        p_param->layer_height);

    ilm_layerAddSurface(layer_id, surface_id);

    ilm_layerSetSourceRectangle(layer_id, 0, 0,
        p_param->layer_width, p_param->layer_height);

    ilm_layerSetDestinationRectangle(layer_id,
        p_param->layer_dest_x, p_param->layer_dest_y,
        p_param->layer_width, p_param->layer_height);

    ilm_layerSetOpacity(layer_id, 1.0f);

    ilm_layerSetVisibility(layer_id, ILM_TRUE);

    /* Commit */
    ilm_commitChanges();

    if (p_param->surface_id != surface_id)
    {
        p_param->surface_id = surface_id;
    }

    if (p_param->layer_id != layer_id)
    {
        p_param->layer_id = layer_id;
    }

    return 0;
}

/**
 * \func   destroy_ilm_context
 *
 * \param  p_params: parameters for create Surface and Layer
 *
 * \return int: return status
 *
 * \see    ilm_helper.h
 */
int
destroy_ilm_context(ilm_params *p_param)
{
    if (0 == g_ilm_init)
    {
        return 1; /* Not initialized */
    }
    ilm_surfaceRemove(p_param->surface_id);
    ilm_layerRemove(p_param->layer_id);
    ilm_destroy();
    return 0;
}

/**
 * \func   display_layer
 *
 * \param  screen_id: Screen ID
 * \param  layer_id: Layer ID
 * \param  render_order_flag: render order flag
 *
 * \return int: return status
 *
 * \see    ilm_helper.h
 */
int
display_layer(t_ilm_int screen_id, t_ilm_layer layer_id,
    unsigned int render_order_flag)
{
    ilmErrorTypes rc;
    t_ilm_layer *p_layer_ids = NULL;
    t_ilm_int n_layer = 0;
    int i;
    struct ilmScreenProperties screen_properties;

    if (0 != (render_order_flag & RENDER_ORDER_ADD))
    {
        rc = ilm_getPropertiesOfScreen(screen_id, &screen_properties);
        if (rc != ILM_SUCCESS)
        {
            ILM_error("Get screen properties failed\n");
            return -1;
        }

        n_layer = screen_properties.layerCount + 1;

        p_layer_ids = (t_ilm_layer*)malloc(sizeof(t_ilm_layer) * n_layer);
        if (NULL == p_layer_ids)
        {
            ILM_error("Insufficient memory\n");
            return -1;
        }

        if (0 < screen_properties.layerCount)
        {
            if (0 != (render_order_flag & RENDER_ORDER_FRONT))
            {
                p_layer_ids[n_layer-1] = layer_id;
                i = 0;
            }
            else
            {
                p_layer_ids[0] = layer_id;
                i = 1;
            }

            memcpy(&p_layer_ids[i], screen_properties.layerIds,
                sizeof(t_ilm_layer) * screen_properties.layerCount);
        }
        else
        {
            p_layer_ids[0] = layer_id;
        }
    }
    else
    {
        p_layer_ids = (t_ilm_layer*)malloc(sizeof(t_ilm_layer));
        if (NULL == p_layer_ids)
        {
            ILM_error("Insufficient memory\n");
            return -1;
        }
        n_layer = 1;
        p_layer_ids[0] = layer_id;
    }

    ilm_displaySetRenderOrder(screen_id, p_layer_ids, n_layer);

    ilm_commitChanges();

    if (NULL != p_layer_ids)
    {
        free(p_layer_ids);
    }

    if (NULL != screen_properties.layerIds)
    {
        free(screen_properties.layerIds);
    }

    return 0;
}
