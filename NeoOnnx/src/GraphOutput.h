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

#pragma once

#include "Tensor.h"

// Forward declaration(s)
namespace onnx {
class ValueInfoProto;
} // namespace onnx

namespace NeoOnnx {

// Graph output
class CGraphOutput {
public:
	explicit CGraphOutput( const onnx::ValueInfoProto& output );

	CGraphOutput( const CGraphOutput& other ) = delete;
	CGraphOutput& operator= ( const CGraphOutput& other ) = delete;

	// Graph output name
	const CString& Name() const { return name; }

	// Adds corresponding sink layer to the dnn
	// If layout is given, it will be used as a layout for this tensor
	// If layout is not given then CTensorLayout::IOLayout( dimCount ) is used
	CPtr<const CSinkLayer> AddSinkLayer( const CUserTensor& input, const CTensorLayout* layout,
		CDnn& dnn ) const;

private:
	// Graph output name
	const CString name;
};

} // namespace NeoOnnx

