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
 * @file  llpcCodeGenManager.cpp
 * @brief LLPC source file: contains implementation of class Llpc::CodeGenManager.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-code-gen-manager"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#include "llpcCodeGenManager.h"
#include "llpcContext.h"
#include "llpcElfReader.h"
#include "llpcFile.h"
#include "llpcInternal.h"
#include "llpcPassManager.h"
#include "llpcPipelineState.h"
#include "llpcTargetInfo.h"

namespace llvm
{

namespace cl
{

// -disable-fp32-denormals: disable target option fp32-denormals
static opt<bool> DisableFp32Denormals("disable-fp32-denormals",
                                      desc("Disable target option fp32-denormals"),
                                      init(false));

} // cl

} // llvm

using namespace llvm;

namespace Llpc
{

// =====================================================================================================================
// Setup LLVM target features, target features are set per entry point function.
void CodeGenManager::SetupTargetFeatures(
    PipelineState*      pPipelineState, // [in] Pipeline state
    Module*             pModule)        // [in, out] LLVM module
{
    std::string globalFeatures = "";

    if (pPipelineState->GetOptions().includeDisassembly)
    {
        globalFeatures += ",+DumpCode";
    }

    if (cl::DisableFp32Denormals)
    {
        globalFeatures += ",-fp32-denormals";
    }

    for (auto pFunc = pModule->begin(), pEnd = pModule->end(); pFunc != pEnd; ++pFunc)
    {
        if ((pFunc->empty() == false) && (pFunc->getLinkage() == GlobalValue::ExternalLinkage))
        {
             std::string targetFeatures(globalFeatures);
             AttrBuilder builder;

             ShaderStage shaderStage = GetShaderStageFromCallingConv(pPipelineState->GetShaderStageMask(),
                                                                     pFunc->getCallingConv());

            bool useSiScheduler = pPipelineState->GetShaderOptions(shaderStage).useSiScheduler;
            if (useSiScheduler)
            {
                // It was found that enabling both SIScheduler and SIFormClauses was bad on one particular
                // game. So we disable the latter here. That only affects XNACK targets.
                targetFeatures += ",+si-scheduler";
                builder.addAttribute("amdgpu-max-memory-clause", "1");
            }

#if LLPC_BUILD_GFX10
            if (pFunc->getCallingConv() == CallingConv::AMDGPU_GS)
            {
                // NOTE: For NGG primitive shader, enable 128-bit LDS load/store operations to optimize gvec4 data
                // read/write. This usage must enable the feature of using CI+ additional instructions.
                const auto pNggControl = pPipelineState->GetNggControl();
                if (pNggControl->enableNgg && (pNggControl->passthroughMode == false))
                {
                    targetFeatures += ",+ci-insts,+enable-ds128";
                }
            }
#endif
            if (pFunc->getCallingConv() == CallingConv::AMDGPU_HS)
            {
                // Force s_barrier to be present (ignore optimization)
                builder.addAttribute("amdgpu-flat-work-group-size", "128,128");
            }
            if (pFunc->getCallingConv() == CallingConv::AMDGPU_CS)
            {
                // Set the work group size
                const auto& csBuiltInUsage = pPipelineState->GetShaderModes()->GetComputeShaderMode();
                uint32_t flatWorkGroupSize =
                    csBuiltInUsage.workgroupSizeX * csBuiltInUsage.workgroupSizeY * csBuiltInUsage.workgroupSizeZ;
                auto flatWorkGroupSizeString = std::to_string(flatWorkGroupSize);
                builder.addAttribute("amdgpu-flat-work-group-size",
                                     flatWorkGroupSizeString + "," + flatWorkGroupSizeString);
            }

            auto gfxIp = pPipelineState->GetTargetInfo().GetGfxIpVersion();
            if (gfxIp.major >= 9)
            {
                targetFeatures += ",+enable-scratch-bounds-checks";
            }

#if LLPC_BUILD_GFX10
            if (gfxIp.major >= 10)
            {
                // Setup wavefront size per shader stage
                uint32_t waveSize = pPipelineState->GetShaderWaveSize(shaderStage);

                targetFeatures += ",+wavefrontsize" + std::to_string(waveSize);

                // Allow driver setting for WGP by forcing backend to set 0
                // which is then OR'ed with the driver set value
                targetFeatures += ",+cumode";
            }
#endif

            if (shaderStage != ShaderStageCopyShader)
            {
                const auto& shaderMode = pPipelineState->GetShaderModes()->GetCommonShaderMode(shaderStage);
                if ((shaderMode.fp16DenormMode == FpDenormMode::FlushNone) ||
                    (shaderMode.fp16DenormMode == FpDenormMode::FlushIn) ||
                    (shaderMode.fp64DenormMode == FpDenormMode::FlushNone) ||
                    (shaderMode.fp64DenormMode == FpDenormMode::FlushIn))
                {
                    targetFeatures += ",+fp64-fp16-denormals";
                }
                else if ((shaderMode.fp16DenormMode == FpDenormMode::FlushOut) ||
                         (shaderMode.fp16DenormMode == FpDenormMode::FlushInOut) ||
                         (shaderMode.fp64DenormMode == FpDenormMode::FlushOut) ||
                         (shaderMode.fp64DenormMode == FpDenormMode::FlushInOut))
                {
                    targetFeatures += ",-fp64-fp16-denormals";
                }
                if ((shaderMode.fp32DenormMode == FpDenormMode::FlushNone) ||
                    (shaderMode.fp32DenormMode == FpDenormMode::FlushIn))
                {
                    targetFeatures += ",+fp32-denormals";
                }
                else if ((shaderMode.fp32DenormMode == FpDenormMode::FlushOut) ||
                         (shaderMode.fp32DenormMode == FpDenormMode::FlushInOut))
                {
                    targetFeatures += ",-fp32-denormals";
                }
            }

            builder.addAttribute("target-features", targetFeatures);
            AttributeList::AttrIndex attribIdx = AttributeList::AttrIndex(AttributeList::FunctionIndex);
            pFunc->addAttributes(attribIdx, builder);
        }
    }
}

} // Llpc
