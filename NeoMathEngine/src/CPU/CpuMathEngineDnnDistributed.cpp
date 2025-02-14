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

#include <CpuMathEngine.h>
#include <CpuMathEngineDnnDistributed.h>
#include <MemoryHandleInternal.h>

namespace NeoML {

CMultiThreadDistributedCommunicator::CMultiThreadDistributedCommunicator( int _n_threads )
    : counter( _n_threads ), waiting_flag( true ), n_threads( _n_threads ), isAbort( false )
{
    handles.resize( n_threads );
}

void CMultiThreadDistributedCommunicator::collectHandles( const CFloatHandle& handle )
{
    IMathEngine* mathEngine = handle.GetMathEngine();
    int thread = mathEngine->GetDistributedInfo().Thread;
    handles[thread] = reinterpret_cast<float*>( GetRaw( handle ) );
}

void CMultiThreadDistributedCommunicator::barrier()
{
    int wait = waiting_flag;
    if( !--counter ){
        counter = n_threads;
        waiting_flag++;
    } else {
        while( waiting_flag == wait ){
            if( isAbort ){
                throw std::logic_error( "Stopping due to error in another thread." );
            }
            std::this_thread::yield();
        }
    }
}

void CMultiThreadDistributedCommunicator::AllReduce( const CFloatHandle& handle, int size )
{
    collectHandles( handle );
    int thread = handle.GetMathEngine()->GetDistributedInfo().Thread;

    barrier();

    int perThread = ( size + n_threads - 1 ) / n_threads;
    float buf;
    for( int i = thread * perThread; i < std::min( ( thread + 1 ) * perThread, size ); i++ ){
        buf = 0;
        for( int j = 0; j < n_threads; j++ ){
            buf += handles[j][i];
        }
        buf /= n_threads;
        for( int j = 0; j < n_threads; j++ ){
            handles[j][i] = buf;
        }
    }
    
    barrier();
}

void CMultiThreadDistributedCommunicator::Broadcast( const CFloatHandle& handle, int size, int root )
{
    collectHandles( handle );
    int thread = handle.GetMathEngine()->GetDistributedInfo().Thread;
    barrier();

    if( thread != root ){
        for( int i = 0; i < size; i++ ){
            handles[thread][i] = handles[root][i];
        }
    }
    barrier();
}

void CreateDistributedCpuMathEngines( IMathEngine** mathEngines, int count )
{
    auto comm = std::make_shared<CMultiThreadDistributedCommunicator>( count );
    for( int i = 0; i < count; i++ ){
        mathEngines[i] = new CCpuMathEngine( 1, 0, comm, CMathEngineDistributedInfo( i, count ) );
    }
}

}