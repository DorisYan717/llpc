/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
 ***********************************************************************************************************************
 * @file  llpcGfx6Chip.h
 * @brief LLPC header file: contains various definitions for Gfx6 chips.
 ***********************************************************************************************************************
 */
#pragma once

#include <cstdint>
#include <unordered_map>
#include "llpcAbiMetadata.h"
#include "llpcElfReader.h"

namespace Llpc
{

namespace Gfx6
{

#include "si_ci_vi_merged_registers.h"
#include "si_ci_vi_merged_typedef.h"

// =====================================================================================================================
// Helper macros to operate registers

// Defines fields: register ID (byte-based) and its value
#define DEF_REG(_reg)             uint32_t _reg##_ID; reg##_reg _reg##_VAL;

// Initializes register ID and its value
#define INIT_REG(_reg)            { _reg##_ID = mm##_reg; _reg##_VAL.u32All = 0; }

// Adds an entry for the map from register ID to its name string
#define ADD_REG_MAP(_reg)         RegNameMap[mm##_reg * 4] = #_reg;

// Gets register value
#define GET_REG(_stage, _reg)                      ((_stage)->_reg##_VAL.u32All)

// Sets register value
#define SET_REG(_stage, _reg, _val)                (_stage)->_reg##_VAL.u32All = (_val);

// Gets register field value
#define GET_REG_FIELD(_stage, _reg, _field)        ((_stage)->_reg##_VAL.bits._field)

// Sets register field value
#define SET_REG_FIELD(_stage, _reg, _field, _val)  (_stage)->_reg##_VAL.bits._field = (_val);

// Preferred number of ES threads per GS thread.
constexpr uint32_t EsThreadsPerGsThread = 128;

// Preferred number of GS primitives per ES thread.
constexpr uint32_t GsPrimsPerEsThread = 256;

// Preferred number of GS threads per VS thread.
constexpr uint32_t GsThreadsPerVsThread = 2;

// Max size of primitives per subgroup for adjacency primitives or when GS instancing is used. This restriction is
// applicable only when GS on-chip mode is used.
constexpr uint32_t GsOnChipMaxPrimsPerSubgroup = 128;

// The register headers don't specify an enum for the values of VGT_GS_MODE.ONCHIP.
enum VGT_GS_MODE_ONCHIP_TYPE : uint32_t
{
    VGT_GS_MODE_ONCHIP_OFF         = 0,
    VGT_GS_MODE_ONCHIP_ON          = 3,
};

// =====================================================================================================================
// Represents configuration of static registers relevant to hardware vertex shader.
struct VsRegConfig
{
    DEF_REG(SPI_SHADER_PGM_RSRC1_VS);
    DEF_REG(SPI_SHADER_PGM_RSRC2_VS);
    DEF_REG(SPI_SHADER_POS_FORMAT);
    DEF_REG(SPI_VS_OUT_CONFIG);
    DEF_REG(PA_CL_VS_OUT_CNTL);
    DEF_REG(PA_CL_CLIP_CNTL);
    DEF_REG(PA_CL_VTE_CNTL);
    DEF_REG(PA_SU_VTX_CNTL);
    DEF_REG(VGT_PRIMITIVEID_EN);
    DEF_REG(VGT_REUSE_OFF);
    DEF_REG(VGT_VERTEX_REUSE_BLOCK_CNTL);
    DEF_REG(VGT_STRMOUT_CONFIG);
    DEF_REG(VGT_STRMOUT_BUFFER_CONFIG);
    DEF_REG(VGT_STRMOUT_VTX_STRIDE_0);
    DEF_REG(VGT_STRMOUT_VTX_STRIDE_1);
    DEF_REG(VGT_STRMOUT_VTX_STRIDE_2);
    DEF_REG(VGT_STRMOUT_VTX_STRIDE_3);

    VsRegConfig();
};

// =====================================================================================================================
// Represents configuration of static registers relevant to hardware hull shader.
struct HsRegConfig
{
    DEF_REG(SPI_SHADER_PGM_RSRC1_HS);
    DEF_REG(SPI_SHADER_PGM_RSRC2_HS);
    DEF_REG(VGT_LS_HS_CONFIG);
    DEF_REG(VGT_HOS_MIN_TESS_LEVEL);
    DEF_REG(VGT_HOS_MAX_TESS_LEVEL);

    HsRegConfig();
};

// =====================================================================================================================
// Represents configuration of static registers relevant to hardware export shader.
struct EsRegConfig
{
    DEF_REG(SPI_SHADER_PGM_RSRC1_ES);
    DEF_REG(SPI_SHADER_PGM_RSRC2_ES);
    DEF_REG(VGT_ESGS_RING_ITEMSIZE);

    EsRegConfig();
};

// =====================================================================================================================
// Represents configuration of static registers relevant to hardware local shader.
struct LsRegConfig
{
    DEF_REG(SPI_SHADER_PGM_RSRC1_LS);
    DEF_REG(SPI_SHADER_PGM_RSRC2_LS);
    LsRegConfig();
};

// =====================================================================================================================
// Represents configuration of static registers relevant to hardware geometry shader.
struct GsRegConfig
{
    DEF_REG(SPI_SHADER_PGM_RSRC1_GS);
    DEF_REG(SPI_SHADER_PGM_RSRC2_GS);
    DEF_REG(VGT_GS_MAX_VERT_OUT);
    DEF_REG(VGT_GS_ONCHIP_CNTL__CI__VI);
    DEF_REG(VGT_ES_PER_GS);
    DEF_REG(VGT_GS_VERT_ITEMSIZE);
    DEF_REG(VGT_GS_INSTANCE_CNT);
    DEF_REG(VGT_GS_PER_VS);
    DEF_REG(VGT_GS_OUT_PRIM_TYPE);
    DEF_REG(VGT_GSVS_RING_ITEMSIZE);
    DEF_REG(VGT_GS_PER_ES);
    DEF_REG(VGT_GS_VERT_ITEMSIZE_1);
    DEF_REG(VGT_GS_VERT_ITEMSIZE_2);
    DEF_REG(VGT_GS_VERT_ITEMSIZE_3);
    DEF_REG(VGT_GSVS_RING_OFFSET_1);
    DEF_REG(VGT_GSVS_RING_OFFSET_2);
    DEF_REG(VGT_GSVS_RING_OFFSET_3);
    DEF_REG(VGT_GS_MODE);

    GsRegConfig();
};

// =====================================================================================================================
// Represents configuration of static registers relevant to hardware pixel shader.
struct PsRegConfig
{
    DEF_REG(SPI_SHADER_PGM_RSRC1_PS);
    DEF_REG(SPI_SHADER_PGM_RSRC2_PS);
    DEF_REG(SPI_SHADER_Z_FORMAT);
    DEF_REG(SPI_SHADER_COL_FORMAT);
    DEF_REG(SPI_BARYC_CNTL);
    DEF_REG(SPI_PS_IN_CONTROL);
    DEF_REG(SPI_PS_INPUT_ENA);
    DEF_REG(SPI_PS_INPUT_ADDR);
    DEF_REG(SPI_INTERP_CONTROL_0);
    DEF_REG(PA_SC_MODE_CNTL_1);
    DEF_REG(DB_SHADER_CONTROL);
    DEF_REG(CB_SHADER_MASK);

    static uint32_t GetPsInputCntlStart();
    static uint32_t GetPsUserDataStart();

    PsRegConfig();
};

// =====================================================================================================================
// Represents configuration of registers relevant to graphics pipeline (VS-FS).
struct PipelineVsFsRegConfig
{
    static constexpr bool containsPalAbiMetadataOnly = true;

    VsRegConfig m_vsRegs;   // VS -> hardware VS
    PsRegConfig m_psRegs;   // FS -> hardware PS
    DEF_REG(VGT_SHADER_STAGES_EN);
    DEF_REG(IA_MULTI_VGT_PARAM);

    PipelineVsFsRegConfig();
};

// =====================================================================================================================
// Represents configuration of registers relevant to graphics pipeline (VS-TS-FS).
struct PipelineVsTsFsRegConfig
{
    static constexpr bool containsPalAbiMetadataOnly = true;

    LsRegConfig m_lsRegs;   // VS  -> hardware LS
    HsRegConfig m_hsRegs;   // TCS -> hardware HS
    VsRegConfig m_vsRegs;   // TES -> hardware VS
    PsRegConfig m_psRegs;   // FS  -> hardware PS

    DEF_REG(VGT_SHADER_STAGES_EN);
    DEF_REG(IA_MULTI_VGT_PARAM);
    DEF_REG(VGT_TF_PARAM);

    PipelineVsTsFsRegConfig();
};

// =====================================================================================================================
// Represents configuration of registers relevant to graphics pipeline (VS-GS-FS).
struct PipelineVsGsFsRegConfig
{
    static constexpr bool containsPalAbiMetadataOnly = true;

    EsRegConfig m_esRegs;   // VS -> hardware ES
    GsRegConfig m_gsRegs;   // GS -> hardware GS
    PsRegConfig m_psRegs;   // FS -> hardware PS
    VsRegConfig m_vsRegs;   // Copy shader -> hardware VS

    DEF_REG(VGT_SHADER_STAGES_EN);
    DEF_REG(IA_MULTI_VGT_PARAM);

    PipelineVsGsFsRegConfig();
};

// =====================================================================================================================
// Represents configuration of registers relevant to graphics pipeline (VS-TS-GS-FS).
struct PipelineVsTsGsFsRegConfig
{
    static constexpr bool containsPalAbiMetadataOnly = true;

    LsRegConfig m_lsRegs;   // VS  -> hardware LS
    HsRegConfig m_hsRegs;   // TCS -> hardware HS
    EsRegConfig m_esRegs;   // TES -> hardware ES
    GsRegConfig m_gsRegs;   // GS  -> hardware GS
    PsRegConfig m_psRegs;   // FS  -> hardware PS
    VsRegConfig m_vsRegs;   // Copy shader -> hardware VS

    DEF_REG(VGT_SHADER_STAGES_EN);
    DEF_REG(IA_MULTI_VGT_PARAM);
    DEF_REG(VGT_TF_PARAM);

    PipelineVsTsGsFsRegConfig();
};

// =====================================================================================================================
// Represents configuration of registers relevant to compute shader.
struct CsRegConfig
{
    static constexpr bool containsPalAbiMetadataOnly = true;

    DEF_REG(COMPUTE_PGM_RSRC1);
    DEF_REG(COMPUTE_PGM_RSRC2);
    DEF_REG(COMPUTE_NUM_THREAD_X);
    DEF_REG(COMPUTE_NUM_THREAD_Y);
    DEF_REG(COMPUTE_NUM_THREAD_Z);

    CsRegConfig();
};

// Map from register ID to its name string
static std::unordered_map<uint32_t, const char*>    RegNameMap;

// Adds entries to register name map.
void InitRegisterNameMap(GfxIpVersion gfxIp);

// Gets the name string from byte-based ID of the register
const char* GetRegisterNameString(GfxIpVersion gfxIp, uint32_t regId);

} // Gfx6

} // Llpc
