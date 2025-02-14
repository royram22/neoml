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

#include <common.h>
#pragma hdrstop

#include <NeoMathEngine/NeoMathEngine.h>
#include <NeoML/Dnn/Layers/TimeConvLayer.h>

namespace NeoML {

CTimeConvLayer::CTimeConvLayer( IMathEngine& mathEngine ) :
	CBaseLayer( mathEngine, "CnnTimeConvLayer", true ),
	desc( 0 ),
	filterCount(0),
	filterSize(0),
	stride(0),
	paddingFront(0),
	paddingBack(0),
	dilation(1)
{
	paramBlobs.SetSize(2);
}

CPtr<CDnnBlob> CTimeConvLayer::GetFilterData() const
{
	if( filter() == 0 ) {
		return 0;
	}

	return filter()->GetCopy();
}

void CTimeConvLayer::SetFilterData(const CPtr<CDnnBlob>& newFilter)
{
	if( newFilter == 0 ) {
		NeoAssert(filter() == 0 || GetDnn() == 0);
		filter() = 0;
	} else if( filter() != 0 && GetDnn() != 0 ) {
		NeoAssert(filter()->HasEqualDimensions(newFilter));
		filter()->CopyFrom(newFilter);
	} else {
		filter() = newFilter->GetCopy();
	}
}

CPtr<CDnnBlob> CTimeConvLayer::GetFreeTermData() const
{
	if( freeTerms() == 0 ) {
		return 0;
	}

	return freeTerms()->GetCopy();
}

void CTimeConvLayer::SetFreeTermData(const CPtr<CDnnBlob>& newFreeTerms)
{
	if( newFreeTerms == 0 ) {
		NeoAssert(freeTerms() == 0 || GetDnn() == 0);
		freeTerms() = 0;
	} else {
		if( freeTerms() != 0 && GetDnn() != 0 ) {
			NeoAssert(freeTerms()->GetDataSize() == newFreeTerms->GetDataSize());

			freeTerms()->CopyFrom(newFreeTerms);
		} else {
			freeTerms() = newFreeTerms->GetCopy();
		}
	}
}

void CTimeConvLayer::Reshape()
{
	CheckInputs();
	CheckLayerArchitecture( GetInputCount() == GetOutputCount(),
		"different number of inputs and outputs in time-conv layer" );
	CheckLayerArchitecture( filterCount > 0, "Filter count must be positive" );
	CheckLayerArchitecture( filterSize > 0, "Filter size must be positive" );
	CheckLayerArchitecture( stride > 0, "Stride must be positive" );

	for(int i = 0; i < GetInputCount(); ++i) {
		const int outputSize = (inputDescs[i].BatchLength() - ( filterSize - 1 ) * dilation - 1 + paddingFront + paddingBack) / stride + 1;

		CheckLayerArchitecture( filterSize <= inputDescs[i].BatchLength() + paddingFront + paddingBack,
			"Filter is bigger than input" );
		if(filter() == 0) {
			filter() = CDnnBlob::Create2DImageBlob( MathEngine(), CT_Float, 1, filterCount, filterSize, 1,
				inputDescs[i].ObjectSize() );
			InitializeParamBlob( i, *filter(), filterSize * inputDescs[i].ObjectSize() );
		} else {
			NeoAssert(filter()->GetBatchLength() == 1);
			NeoAssert(filter()->GetBatchWidth() == filterCount);
			NeoAssert(filter()->GetHeight() == filterSize);
			NeoAssert(filter()->GetWidth() == 1);
			NeoAssert(filter()->GetDepth() == 1);
			NeoAssert(filter()->GetChannelsCount() == inputDescs[i].ObjectSize());
		}
		outputDescs[i] = CBlobDesc( inputDescs[i].GetDataType() );
		outputDescs[i].SetDimSize( BD_BatchLength, outputSize );
		outputDescs[i].SetDimSize( BD_BatchWidth, inputDescs[i].BatchWidth() );
		outputDescs[i].SetDimSize( BD_Channels, filterCount );
	}

	if(freeTerms() == 0) {
		freeTerms() = CDnnBlob::CreateVector( MathEngine(), CT_Float, filterCount );
		freeTerms()->Fill(0);
	} else {
		CheckLayerArchitecture( freeTerms()->GetDataSize() == filterCount,
			"number of free members in conv-time layer is not equal to number of filters" );
	}
	destroyDesc();
}

static const int TimeConvLayerVersion = 2001;

void CTimeConvLayer::Serialize( CArchive& archive )
{
	const int version = archive.SerializeVersion( TimeConvLayerVersion, CDnn::ArchiveMinSupportedVersion );
	CBaseLayer::Serialize( archive );

	archive.Serialize( filterSize );
	archive.Serialize( stride );
	if( version < 2001 ) {
		archive.Serialize( paddingFront );
		paddingBack = paddingFront;
	} else {
		archive.Serialize( paddingFront );
		archive.Serialize( paddingBack );
	}
	archive.Serialize( filterCount );
	archive.Serialize( dilation );

	if( archive.IsLoading() ) {
		// Converts the free terms blob into a new tensor 
		// with the length in the first dimension not Channels
		CDnnBlob* pFreeTerms = freeTerms();
		if( pFreeTerms != 0 && pFreeTerms->DimSize(0) != pFreeTerms->GetDataSize() ) {
			NeoAssert( pFreeTerms->GetChannelsCount() == pFreeTerms->GetDataSize() );
			CBlobDesc desc( CT_Float );
			desc.SetDimSize( 0, pFreeTerms->GetDataSize() );
			pFreeTerms->ReinterpretDimensions( desc );
		}
	}
}

void CTimeConvLayer::RunOnce()
{
	initDesc();

	for( int i = 0; i < outputBlobs.Size(); ++i ) {
		MathEngine().BlobTimeConvolution( *desc,
			inputBlobs[i]->GetData(), filter()->GetData(),
			freeTerms()->GetData(), outputBlobs[i]->GetData() );
	}
}

void CTimeConvLayer::BackwardOnce()
{
	initDesc();

	for( int i = 0; i < inputDiffBlobs.Size(); ++i ) {
		MathEngine().BlobTimeConvolutionBackward( *desc,
			outputDiffBlobs[i]->GetData(), filter()->GetData(),
			freeTerms()->GetData(), inputDiffBlobs[i]->GetData() );
	}
}

void CTimeConvLayer::LearnOnce()
{
	initDesc();

	for( int i = 0; i < outputDiffBlobs.Size(); ++i ) {
		MathEngine().BlobTimeConvolutionLearnAdd( *desc,
			inputBlobs[i]->GetData(), outputDiffBlobs[i]->GetData(),
			filterDiff()->GetData(), freeTermsDiff()->GetData() );
	}
}

void CTimeConvLayer::FilterLayerParams( float threshold )
{
	for( int blobIndex = 0; blobIndex < paramBlobs.Size(); ++blobIndex ) {
		if( paramBlobs[blobIndex] != 0 ) {
			MathEngine().FilterSmallValues( paramBlobs[blobIndex]->GetData(),
				paramBlobs[blobIndex]->GetDataSize(), threshold );
		}
	}
}

void CTimeConvLayer::initDesc()
{
	if( desc == 0 && !inputBlobs.IsEmpty() && !outputBlobs.IsEmpty() ) {
		NeoPresume( inputBlobs[0] != nullptr || inputDiffBlobs[0] != nullptr );
		NeoPresume( outputBlobs[0] != nullptr || outputDiffBlobs[0] != nullptr );
		desc = MathEngine().InitTimeConvolution(
			inputBlobs[0] != nullptr ? inputBlobs[0]->GetDesc() : inputDiffBlobs[0]->GetDesc(),
			stride, paddingFront, paddingBack, dilation, filter()->GetDesc(),
			outputBlobs[0] != nullptr ? outputBlobs[0]->GetDesc() : outputDiffBlobs[0]->GetDesc() );
	}
}

void CTimeConvLayer::destroyDesc()
{
	if( desc != 0 ) {
		delete desc;
		desc = 0;
	}
}

CLayerWrapper<CTimeConvLayer> TimeConv( int filterCount, int filterSize, int padding,
	int stride, int dilation )
{
	return CLayerWrapper<CTimeConvLayer>( "ChannelwiseConv", [=]( CTimeConvLayer* result ) {
		result->SetFilterCount( filterCount );
		result->SetFilterSize( filterSize );
		result->SetPadding( padding );
		result->SetStride( stride );
		result->SetDilation( dilation );
	} );
}

} // namespace NeoML
