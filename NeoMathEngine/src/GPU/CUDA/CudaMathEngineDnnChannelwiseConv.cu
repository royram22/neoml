/* Copyright © 2017-2020 ABBYY Production LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
--------------------------------------------------------------------------------------------------------------*/

#include <NeoMathEngine/NeoMathEngineDefs.h>

#ifdef NEOML_USE_CUDA

#include <CudaMathEngine.h>
#include <CudaMathEngineDnnConvs.h>
#include <MemoryHandleInternal.h>
#include <MathEngineCommon.h>
#include <CudaCommon.h>
#include <CudaDevice.h>

#include <Kernels/CudaDnnChannelwiseConvKernels.h>

namespace NeoML {

CChannelwiseConvolutionDesc* CCudaMathEngine::InitBlobChannelwiseConvolution( const CBlobDesc& source,
	int paddingHeight, int paddingWidth, int strideHeight, int strideWidth,
	const CBlobDesc& filter, const CBlobDesc* freeTerm, const CBlobDesc& result )
{
	ASSERT_EXPR(source.Depth() == 1);
	ASSERT_EXPR(filter.Height() > paddingHeight);
	ASSERT_EXPR(filter.Height() <= source.Height() + 2 * paddingHeight);
	ASSERT_EXPR(filter.Width() > paddingWidth);
	ASSERT_EXPR(filter.Width() <= source.Width() + 2 * paddingWidth);
	ASSERT_EXPR(filter.ObjectCount() == 1);
	ASSERT_EXPR(filter.Channels() == source.Channels());
	ASSERT_EXPR(freeTerm == 0 || freeTerm->BlobSize() == filter.Channels());
	ASSERT_EXPR(result.BatchLength() == source.BatchLength());
	ASSERT_EXPR(result.BatchWidth() == source.BatchWidth());
	ASSERT_EXPR(result.ListSize() == source.ListSize());
	ASSERT_EXPR(result.Depth() == 1);
	ASSERT_EXPR(result.Channels() == source.Channels());
	const int expectedOutputHeight = (source.Height() - filter.Height() + 2 * paddingHeight) / strideHeight + 1;
	const int expectedOutputWidth = (source.Width() - filter.Width() + 2 * paddingWidth) / strideWidth + 1;
	ASSERT_EXPR(result.Height() == expectedOutputHeight);
	ASSERT_EXPR(result.Width() == expectedOutputWidth);

	CCudaChannelwiseConvolutionDesc* desc = new CCudaChannelwiseConvolutionDesc();
	desc->Internal.PaddingHeight = paddingHeight;
	desc->Internal.PaddingWidth = paddingWidth;
	desc->Internal.StrideHeight = strideHeight;
	desc->Internal.StrideWidth = strideWidth;
	desc->Internal.Source = source;
	desc->Internal.Filter = filter;
	desc->Internal.Result = result;
	return desc;
}

void CCudaMathEngine::BlobChannelwiseConvolution( const CChannelwiseConvolutionDesc& convDesc,
	const CConstFloatHandle& sourceData, const CConstFloatHandle& filterData, const CConstFloatHandle* freeTermData,
	const CFloatHandle& resultData )
{
	ASSERT_EXPR( sourceData.GetMathEngine() == this );
	ASSERT_EXPR( filterData.GetMathEngine() == this );
	ASSERT_EXPR( freeTermData == 0 || freeTermData->GetMathEngine() == this );
	ASSERT_EXPR( resultData.GetMathEngine() == this );
	SetCudaDevice( device->DeviceNumber );

	const CCudaChannelwiseConvolutionDescInternal& desc = static_cast<const CCudaChannelwiseConvolutionDesc&>( convDesc ).Internal;

	dim3 blockCount;
	dim3 threadCount;

	const CCudaBlobDesc& result = desc.Result;

	getCudaTaskGrid2D( blockCount, threadCount, result.ObjectCount() * result.Height(), result.Width() * result.Channels() );

	BlobChannelwiseConvolutionKernel<<<blockCount, threadCount>>>( desc, GetRaw( sourceData ), GetRaw( filterData ),
		freeTermData == 0 ? 0 : GetRaw( *freeTermData ), GetRaw( resultData ) );
}

void CCudaMathEngine::BlobChannelwiseConvolutionBackward( const CChannelwiseConvolutionDesc& convDesc,
	const CConstFloatHandle& sourceData, const CConstFloatHandle& filterData, const CFloatHandle& resultData )
{
	ASSERT_EXPR( sourceData.GetMathEngine() == this );
	ASSERT_EXPR( filterData.GetMathEngine() == this );
	ASSERT_EXPR( resultData.GetMathEngine() == this );
	SetCudaDevice( device->DeviceNumber );

	const CCudaChannelwiseConvolutionDescInternal& desc = static_cast<const CCudaChannelwiseConvolutionDesc&>( convDesc ).Internal;
	const CCudaBlobDesc& inputDiff = desc.Source;

	dim3 blockCount;
	dim3 threadCount;

	getCudaTaskGrid2D( blockCount, threadCount, inputDiff.ObjectCount() * inputDiff.Height(), inputDiff.Width() * inputDiff.Channels() );

	BlobChannelwiseConvolutionBackwardKernel<<<blockCount, threadCount>>>( desc, GetRaw( sourceData ), GetRaw( filterData ), GetRaw( resultData ) );
}

void CCudaMathEngine::BlobChannelwiseConvolutionLearnAdd( const CChannelwiseConvolutionDesc& convDesc,
	const CConstFloatHandle& inputData, const CConstFloatHandle& outputDiffData, const CFloatHandle& filterDiffData,
	const CFloatHandle* freeTermDiffData )
{
	ASSERT_EXPR( inputData.GetMathEngine() == this );
	ASSERT_EXPR( outputDiffData.GetMathEngine() == this );
	ASSERT_EXPR( filterDiffData.GetMathEngine() == this );
	ASSERT_EXPR( freeTermDiffData == 0 || freeTermDiffData->GetMathEngine() == this );
	SetCudaDevice( device->DeviceNumber );

	const CCudaChannelwiseConvolutionDescInternal& desc = static_cast<const CCudaChannelwiseConvolutionDesc&>( convDesc ).Internal;
	const CCudaBlobDesc& outputDiff = desc.Result;
	const CCudaBlobDesc& filterDiff = desc.Filter;

	if( freeTermDiffData != 0 ) {
		// Train the free term
		SumMatrixRowsAdd( 1, *freeTermDiffData, outputDiffData,
			outputDiff.Height() * outputDiff.Width() * outputDiff.ObjectCount(), outputDiff.Channels() );
	}

	dim3 blockCount;
	dim3 threadCount;

	getCudaTaskGrid2D( blockCount, threadCount, filterDiff.Height() * filterDiff.Width(), filterDiff.Channels() );

	BlobChannelwiseConvolutionLearnAddKernel<<<blockCount, threadCount>>>( desc, GetRaw(inputData), GetRaw(outputDiffData), GetRaw(filterDiffData) );
}

void CCudaMathEngine::MobileNetV2Block( const CBlobDesc& inputDesc, const CBlobDesc& outputDesc,
	const CChannelwiseConvolutionDesc& convDesc, const CConstFloatHandle& inputHandle,
	const CConstFloatHandle& expandFilter, const CConstFloatHandle* expandFreeTerm,
	const CConstFloatHandle& expandReLUThreshold, const CConstFloatHandle& channelwiseFilter,
	const CConstFloatHandle* channelwiseFreeTerm, const CConstFloatHandle& channelwiseReLUThreshold,
	const CConstFloatHandle& downFilter, const CConstFloatHandle* downFreeTerm, bool residual,
	const CFloatHandle& outputHandle )
{
	SetCudaDevice( device->DeviceNumber );
	const CCudaChannelwiseConvolutionDescInternal& desc = static_cast<const CCudaChannelwiseConvolutionDesc&>( convDesc ).Internal;

	CFloatHandleStackVar channelwiseInput( *this, desc.Source.BlobSize() );
	CFloatHandleStackVar channelwiseOutput( *this, desc.Result.BlobSize() );

	const int expandedChannels = desc.Filter.Channels();

	if( residual && inputHandle != outputHandle ) {
		VectorCopy( outputHandle, inputHandle, inputDesc.BlobSize() );
	}

	MultiplyMatrixByTransposedMatrix( 1, inputHandle, inputDesc.ObjectCount() * inputDesc.GeometricalSize(),
		inputDesc.Channels(), expandFilter, expandedChannels, channelwiseInput, channelwiseInput.Size() );

	if( expandFreeTerm != nullptr ) {
		AddVectorToMatrixRows( 1, channelwiseInput, channelwiseInput, channelwiseInput.Size() / expandedChannels,
			expandedChannels, *expandFreeTerm );
	}
	VectorReLU( channelwiseInput, channelwiseInput, channelwiseInput.Size(), expandReLUThreshold );

	BlobChannelwiseConvolution( convDesc, channelwiseInput, channelwiseFilter, channelwiseFreeTerm, channelwiseOutput );
	VectorReLU( channelwiseOutput, channelwiseOutput, channelwiseOutput.Size(), channelwiseReLUThreshold );

	if( residual ) {
		multiplyMatrixByTransposedMatrixAndAdd( channelwiseOutput, channelwiseOutput.Size() / expandedChannels,
			expandedChannels, expandedChannels, downFilter, outputDesc.Channels(), expandedChannels, outputHandle,
			outputDesc.Channels() );
	} else {
		MultiplyMatrixByTransposedMatrix( 1, channelwiseOutput, channelwiseOutput.Size() / expandedChannels,
			expandedChannels, downFilter, outputDesc.Channels(), outputHandle, outputDesc.BlobSize() );
	}

	if( downFreeTerm != nullptr ) {
		AddVectorToMatrixRows( 1, outputHandle, outputHandle, outputDesc.BlobSize() / outputDesc.Channels(),
			outputDesc.Channels(), *downFreeTerm );
	}
}

} // namespace NeoML

#endif // NEOML_USE_CUDA
