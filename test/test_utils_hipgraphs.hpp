// Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef ROCRAND_TEST_UTILS_HIPGRAPHS_HPP
#define ROCRAND_TEST_UTILS_HIPGRAPHS_HPP

#include <hip/hip_runtime.h>
#include "test_common.hpp"

// Helper functions for testing with hipGraph stream capture.
// Note: graphs will not work on the default stream.
namespace test_utils
{
    class GraphHelper{
        private:
            hipGraph_t graph;
            hipGraphExec_t graph_instance;
        public:

            inline void startStreamCapture(hipStream_t & stream)
            {
                HIP_CHECK_NON_VOID(hipStreamBeginCapture(stream, hipStreamCaptureModeGlobal));
            }

            inline void endStreamCapture(hipStream_t & stream)
            {
                HIP_CHECK_NON_VOID(hipStreamEndCapture(stream, &graph));
            }

            inline void createAndLaunchGraph(hipStream_t & stream, const bool launchGraph=true, const bool sync=true)
            {
                
                endStreamCapture(stream);
                
                HIP_CHECK_NON_VOID(hipGraphInstantiate(&graph_instance, graph, nullptr, nullptr, 0));

                // Optionally launch the graph
                if (launchGraph)
                    HIP_CHECK_NON_VOID(hipGraphLaunch(graph_instance, stream));

                // Optionally synchronize the stream when we're done
                if (sync)
                    HIP_CHECK_NON_VOID(hipStreamSynchronize(stream));
            } 
    
            inline void cleanupGraphHelper()
            {
                HIP_CHECK_NON_VOID(hipGraphDestroy(this->graph));
                HIP_CHECK_NON_VOID(hipGraphExecDestroy(this->graph_instance));
            }

            inline void resetGraphHelper(hipStream_t& stream, const bool beginCapture=true)
            {
                // Destroy the old graph and instance
                cleanupGraphHelper();

                if(beginCapture)
                    startStreamCapture(stream);
            }

            inline void launchGraphHelper(hipStream_t& stream,const bool sync=false)
            {
                HIP_CHECK_NON_VOID(hipGraphLaunch(this->graph_instance, stream));

                // Optionally sync after the launch
                if (sync)
                    HIP_CHECK_NON_VOID(hipStreamSynchronize(stream));
            }
    };
} // end namespace test_utils

#endif //ROCRAND_TEST_UTILS_HIPGRAPHS_HPP
