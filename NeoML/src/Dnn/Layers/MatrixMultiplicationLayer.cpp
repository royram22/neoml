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

#include <NeoML/Dnn/Layers/MatrixMultiplicationLayer.h>

namespace NeoML {

CMatrixMultiplicationLayer::CMatrixMultiplicationLayer( IMathEngine& mathEngine ) :
	CBaseLayer( mathEngine, "CMatrixMultiplicationLayer", false )
{
}

static const int MatrixMultiplicationLayerVersion = 0;

void CMatrixMultiplicationLayer::Serialize( CArchive& archive )
{
	archive.SerializeVersion( MatrixMultiplicationLayerVersion );
	CBaseLayer::Serialize( archive );
}


void CMatrixMultiplicationLayer::Reshape()
{
	CheckInputs();
	CheckLayerArchitecture( inputDescs.Size() == 2, "layer must have 2 inputs" );

	CheckLayerArchitecture( inputDescs[0].Channels() == inputDescs[1].GeometricalSize(),
		"input[0].Channels must be equal to input[1].GeometricalSize" );
	if( IsBackwardPerformed() ) {
		CheckLayerArchitecture( inputDescs[0].ObjectCount() == inputDescs[1].ObjectCount(),
			"object count mismatch between inputs" );
	} else {
		CheckLayerArchitecture( inputDescs[0].ObjectCount() == inputDescs[1].ObjectCount()
			|| inputDescs[0].ObjectCount() == 1 || inputDescs[1].ObjectCount() == 1,
			"object count mismatch between inputs" );
	}

	outputDescs.SetSize( 1 );
	CBlobDesc outputDesc = inputDescs[0];
	outputDesc.SetDimSize( BD_Channels, inputDescs[1].Channels() );
	if( inputDescs[1].ObjectCount() > inputDescs[0].ObjectCount() ) {
		outputDesc.SetDimSize( BD_BatchLength, inputDescs[1].BatchLength() );
		outputDesc.SetDimSize( BD_BatchWidth, inputDescs[1].BatchWidth() );
		outputDesc.SetDimSize( BD_ListSize, inputDescs[1].ListSize() );
	}

	outputDescs[0] = outputDesc;
}

void CMatrixMultiplicationLayer::RunOnce()
{
	if( inputBlobs[0]->GetObjectCount() == inputBlobs[1]->GetObjectCount() ) {
		MathEngine().MultiplyMatrixByMatrix( inputBlobs[0]->GetObjectCount(), inputBlobs[0]->GetData(),
			inputBlobs[0]->GetGeometricalSize(), inputBlobs[0]->GetChannelsCount(), inputBlobs[1]->GetData(),
			inputBlobs[1]->GetChannelsCount(), outputBlobs[0]->GetData(), outputBlobs[0]->GetDataSize() );
	} else if( inputBlobs[1]->GetObjectCount() == 1 ) {
		for( int i = 0; i < inputBlobs[0]->GetObjectCount(); ++i ) {
			MathEngine().MultiplyMatrixByMatrix( 1, inputBlobs[0]->GetObjectData( i ),
				inputBlobs[0]->GetGeometricalSize(), inputBlobs[0]->GetChannelsCount(), inputBlobs[1]->GetData(),
				inputBlobs[1]->GetChannelsCount(), outputBlobs[0]->GetObjectData( i ), outputBlobs[0]->GetObjectSize() );
		}
	} else if( inputBlobs[0]->GetObjectCount() == 1 ) {
		for( int i = 0; i < inputBlobs[1]->GetObjectCount(); ++i ) {
			MathEngine().MultiplyMatrixByMatrix( 1, inputBlobs[0]->GetData(),
				inputBlobs[0]->GetGeometricalSize(), inputBlobs[0]->GetChannelsCount(), inputBlobs[1]->GetObjectData( i ),
				inputBlobs[1]->GetChannelsCount(), outputBlobs[0]->GetObjectData( i ), outputBlobs[0]->GetObjectSize() );
		}
	} else {
		NeoAssert( false );
	}
}

void CMatrixMultiplicationLayer::BackwardOnce()
{
	NeoAssert( inputBlobs[0]->GetObjectCount() == inputBlobs[1]->GetObjectCount() );
	NeoAssert( outputDiffBlobs[0]->GetChannelsCount() == inputBlobs[1]->GetChannelsCount() );
	NeoAssert( outputDiffBlobs[0]->GetGeometricalSize() == inputBlobs[0]->GetGeometricalSize() );

	MathEngine().MultiplyMatrixByTransposedMatrix( inputBlobs[0]->GetObjectCount(),
		outputDiffBlobs[0]->GetData(), outputDiffBlobs[0]->GetGeometricalSize(),
		outputDiffBlobs[0]->GetChannelsCount(), inputBlobs[1]->GetData(),
		inputBlobs[1]->GetGeometricalSize(), inputDiffBlobs[0]->GetData(), 
		inputDiffBlobs[0]->GetDataSize() );

	MathEngine().MultiplyTransposedMatrixByMatrix( inputBlobs[0]->GetObjectCount(),
		inputBlobs[0]->GetData(), inputBlobs[0]->GetGeometricalSize(), inputBlobs[0]->GetChannelsCount(),
		outputDiffBlobs[0]->GetData(), outputDiffBlobs[0]->GetChannelsCount(),
		inputDiffBlobs[1]->GetData(), inputDiffBlobs[1]->GetDataSize() );
}

CLayerWrapper<CMatrixMultiplicationLayer> MatrixMultiplication()
{
	return CLayerWrapper<CMatrixMultiplicationLayer>( "MatrixMultiplication" );
}

} // namespace NeoML
