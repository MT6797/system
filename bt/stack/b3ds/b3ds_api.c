/* Copyright Statement:
 * *
 * * This software/firmware and related documentation ("MediaTek Software") are
 * * protected under relevant copyright laws. The information contained herein
 * * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * * Without the prior written permission of MediaTek inc. and/or its licensors,
 * * any reproduction, modification, use or disclosure of MediaTek Software,
 * * and information contained herein, in whole or in part, shall be strictly prohibited.
 * *
 * * MediaTek Inc. (C) 2010. All rights reserved.
 * *
 * * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 * *
 * * The following software/firmware and/or related documentation ("MediaTek Software")
 * * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * * applicable license agreements with MediaTek Inc.
 * */

#include "bt_target.h"

#if defined(MTK_B3DS_INCLUDED) && (MTK_B3DS_INCLUDED == TRUE)

#include <string.h>
#include "gki.h"
#include "bt_types.h"
#include "b3ds_api.h"
#include "b3ds_int.h"
#include "sdp_api.h"
#include "sdpdefs.h"
#include "l2c_api.h"
#include "hcidefs.h"
#include "btm_api.h"
#include "bta_sys.h"

const tB3DS_SHUTTER_OFFSET b3ds_shutter_offsets[] = {
    { 0xffff, 0xffff, 0xffff, 0xffff },
    { 0xffff, 0xffff, 0xffff, 0xffff },     // 2D         - 65535, 65535, 65535, 65535
    { 0x0002, 0x28b1, 0x28b2, 0x5161 },     // 3D 48Hz    -     2, 10417, 10418, 20833
    { 0x0002, 0x270f, 0x2711, 0x4e1e },     // 3D 50Hz    -     2,  9999, 10001, 19998
    { 0x0002, 0x208c, 0x208e, 0x4118 },     // 3D 60Hz    -     2,  8332,  8334, 16664
    { 0x0002, 0x208c, 0x208e, 0x4118 },     // 3D 60Hz    -     2,  8332,  8334, 16664
};

const tB3DS_CAL_FRAME_SYNC_PERIOD b3ds_cal_frame_sync_period[] = {
    { 0,        0,      0,      0     },
    { 0,        0,      0,      0     },
    { 20833,    85,     20491,  21187 },    // 48.8 Hz - 47.2 Hz
    { 20000,    0,      19685,  20326 },    // 50.8 Hz - 49.2 Hz
    { 16666,    171,    16447,  16892 },    // 60.8 Hz - 59.2 Hz
    { 16666,    171,    16447,  16892 },    // 60.8 Hz - 59.2 Hz
};

void B3DS_Register (tB3DS_REGISTER *p_register)
{
    B3DS_TRACE_DEBUG("B3DS_Register");

    b3ds_register_with_l2cap ();

    if (b3ds_cb.b3ds_sdp_handle == 0)
        b3ds_cb.b3ds_sdp_handle = b3ds_register_with_sdp(UUID_SERVCLASS_3D_DISPLAY, "3D Display");

    if (!p_register)
        return;

    b3ds_cb.b3ds_con_anno_msg_cb = p_register->b3ds_con_anno_msg_cb;

#if defined(MTK_LEGACY_B3DS_INCLUDED) && (MTK_LEGACY_B3DS_INCLUDED == TRUE)
    b3ds_cb.b3ds_ref_proto_assoc_notify_ev_cb = p_register->b3ds_ref_proto_assoc_notify_ev_cb;
#endif

    return;
}

void B3DS_Deregister (void)
{
    B3DS_TRACE_DEBUG("B3DS_Deregister");

    if (b3ds_cb.b3ds_sdp_handle != 0)
    {
        SDP_DeleteRecord (b3ds_cb.b3ds_sdp_handle);
        b3ds_cb.b3ds_sdp_handle = 0;
    }

    L2CA_UcdDeregister(BT_PSM_B3DS);

    btu_stop_timer(&b3ds_cb.set_2d_timer);

    b3ds_cb.b3ds_con_anno_msg_cb = NULL;

#if defined(MTK_LEGACY_B3DS_INCLUDED) && (MTK_LEGACY_B3DS_INCLUDED == TRUE)
    b3ds_cb.b3ds_ref_proto_assoc_notify_ev_cb = NULL;
#endif

    // L2CA_Deregister (BT_PSM_B3DS);
}

void B3DS_SetBroadcast (tB3DS_VEDIO_MODE vedio_mode, tB3DS_PERIOD period_mode, UINT32 delay)
{
    B3DS_TRACE_DEBUG("B3DS_SetBroadcast: vedio_mode %d, period_mode %d, delay %d", vedio_mode, period_mode, delay);

    b3ds_cb.broadcast.vedio_mode = vedio_mode;
    b3ds_cb.broadcast.period_mode = period_mode;
    b3ds_cb.broadcast.period_mode_dynamic = (period_mode == B3DS_PERIOD_DYNAMIC_CALCULATED) ? TRUE : FALSE;
    b3ds_cb.broadcast.delay = delay;

    b3ds_set_csb_data(0, 0);
}

void B3DS_ClockTick(UINT32 clock, UINT16 offset)
{
    B3DS_TRACE_DEBUG("B3DS_ClockTick");

    if (b3ds_cb.broadcast.period_mode_dynamic)
        b3ds_cal_frame_period(clock, offset);

    btu_stop_timer(&b3ds_cb.set_2d_timer);
    btu_start_timer(&b3ds_cb.set_2d_timer, BTU_TTYPE_B3DS_SET_2D, B3DS_SET_2D_TIMEOUT);
    b3ds_set_csb_data(clock, offset);
}

UINT8 B3DS_SetTraceLevel (UINT8 new_level)
{
    B3DS_TRACE_DEBUG("B3DS_SetTraceLevel");

    if (new_level != 0xFF)
        b3ds_cb.trace_level = new_level;

    return (b3ds_cb.trace_level);
}

void B3DS_Init ()
{
    UINT8 i = 0;
    B3DS_TRACE_DEBUG("B3DS_Init");
    memset (&b3ds_cb, 0, sizeof (tB3DS_CB));

#if defined(B3DS_INITIAL_TRACE_LEVEL)
    b3ds_cb.trace_level = B3DS_INITIAL_TRACE_LEVEL;
#else
    b3ds_cb.trace_level = BT_TRACE_LEVEL_NONE;    /* No traces */
#endif
    // Default value
    b3ds_cb.broadcast.vedio_mode = B3DS_VEDIO_MODE_3D;
    b3ds_cb.broadcast.period_mode = B3DS_PERIOD_DYNAMIC_CALCULATED;
    b3ds_cb.broadcast.period_mode_dynamic = TRUE;
    b3ds_cb.broadcast.delay = 0;

    for(i = 0; i<=B3DS_PERIOD_MAX; i++)
        b3ds_cb.broadcast.shutter_offset[i] = b3ds_shutter_offsets[i];

    for(i = 0; i<=B3DS_PERIOD_MAX; i++)
        b3ds_cb.broadcast.cal_frame_sync_period[i] = b3ds_cal_frame_sync_period[i];

}

#if defined(MTK_LEGACY_B3DS_INCLUDED) && (MTK_LEGACY_B3DS_INCLUDED == TRUE)
void B3DS_ProcRefProtoAssocNotifyEvt(void)
{
    B3DS_TRACE_DEBUG("B3DS_ProcRefProtoAssocNotifyEvt");
    b3ds_cb.b3ds_ref_proto_assoc_notify_ev_cb();
}
#endif

#endif

