// Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All rights reserved.
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

#include "benchmark_curand_utils.hpp"
#include "cmdparser.hpp"

#include <benchmark/benchmark.h>

#include <cuda_runtime.h>
#include <curand.h>

#ifndef DEFAULT_RAND_N
const size_t DEFAULT_RAND_N = 1024 * 1024 * 128;
#endif

typedef curandRngType rng_type_t;

template<typename T>
using generate_func_type = std::function<curandStatus_t(curandGenerator_t, T*, size_t)>;

template<typename T>
void run_benchmark(benchmark::State&     state,
                   generate_func_type<T> generate_func,
                   const size_t          size,
                   const bool            byte_size,
                   const size_t          trials,
                   const size_t          dimensions,
                   const size_t          offset,
                   const rng_type_t      rng_type,
                   const curandOrdering  ordering,
                   const bool            benchmark_host,
                   cudaStream_t          stream)
{
    const size_t binary_div   = byte_size ? sizeof(T) : 1;
    const size_t rounded_size = (size / binary_div / dimensions) * dimensions;

    T*                data;
    curandGenerator_t generator;

    if(benchmark_host)
    {
        data = new T[size];
        CURAND_CALL(curandCreateGeneratorHost(&generator, rng_type));
    }
    else
    {
        CUDA_CALL(cudaMalloc(&data, rounded_size * sizeof(T)));
        CURAND_CALL(curandCreateGenerator(&generator, rng_type));
    }

    CURAND_CALL(curandSetGeneratorOrdering(generator, ordering));

    curandStatus_t status = curandSetQuasiRandomGeneratorDimensions(generator, dimensions);
    if(status != CURAND_STATUS_TYPE_ERROR) // If the RNG is not quasi-random
    {
        CURAND_CALL(status);
    }

    CURAND_CALL(curandSetStream(generator, stream));

    status = curandSetGeneratorOffset(generator, offset);
    if(status != CURAND_STATUS_TYPE_ERROR) // If the RNG is not pseudo-random
    {
        CURAND_CALL(status);
    }

    // Warm-up
    for(size_t i = 0; i < 15; i++)
    {
        CURAND_CALL(generate_func(generator, data, size));
    }
    CUDA_CALL(cudaDeviceSynchronize());

    // Measurement
    cudaEvent_t start, stop;
    CUDA_CALL(cudaEventCreate(&start));
    CUDA_CALL(cudaEventCreate(&stop));
    for(auto _ : state)
    {
        CUDA_CALL(cudaEventRecord(start, stream));
        for(size_t i = 0; i < trials; i++)
        {
            CURAND_CALL(generate_func(generator, data, size));
        }
        CUDA_CALL(cudaEventRecord(stop, stream));
        CUDA_CALL(cudaEventSynchronize(stop));

        float elapsed = 0.0f;
        CUDA_CALL(cudaEventElapsedTime(&elapsed, start, stop));

        state.SetIterationTime(elapsed / 1000.f);
    }

    state.SetBytesProcessed(trials * state.iterations() * size * sizeof(T));
    state.SetItemsProcessed(trials * state.iterations() * size);

    CUDA_CALL(cudaEventDestroy(stop));
    CUDA_CALL(cudaEventDestroy(start));
    CURAND_CALL(curandDestroyGenerator(generator));

    if(benchmark_host)
    {
        delete[] data;
    }
    else
    {
        CUDA_CALL(cudaFree(data));
    }
}

void configure_parser(cli::Parser& parser)
{
    parser.set_optional<size_t>("size", "size", DEFAULT_RAND_N, "number of values");
    parser.set_optional<size_t>("trials", "trials", 20, "number of trials");
    parser.set_optional<size_t>("offset", "offset", 0, "offset of generated pseudo-random values");
    parser.set_optional<size_t>("dimensions",
                                "dimensions",
                                1,
                                "number of dimensions of quasi-random values");
    parser.set_optional<std::vector<double>>(
        "lambda",
        "lambda",
        {10.0},
        "space-separated list of lambdas of Poisson distribution");
    parser.set_optional<bool>("host",
                              "host",
                              false,
                              "run benchmarks on the host instead of on the device");
}

int main(int argc, char* argv[])
{
    // get paramaters before they are passed into
    std::string outFormat     = "";
    std::string filter        = "";
    std::string consoleFormat = "";

    getFormats(argc, argv, outFormat, filter, consoleFormat);

    // Parse argv
    benchmark::Initialize(&argc, argv);

    // Parse arguments from command line
    cli::Parser parser(argc, argv);
    configure_parser(parser);
    parser.run_and_exit_if_error();

    cudaStream_t stream;
    CUDA_CALL(cudaStreamCreate(&stream));

    add_common_benchmark_curand_info();

    const size_t              size            = parser.get<size_t>("size");
    const bool                byte_size       = parser.get<bool>("byte-size");
    const size_t              trials          = parser.get<size_t>("trials");
    const size_t              offset          = parser.get<size_t>("offset");
    const size_t              dimensions      = parser.get<size_t>("dimensions");
    const std::vector<double> poisson_lambdas = parser.get<std::vector<double>>("lambda");
    const bool                benchmark_host  = parser.get<bool>("host");

    benchmark::AddCustomContext("size", std::to_string(size));
    benchmark::AddCustomContext("byte-size", std::to_string(byte_size));
    benchmark::AddCustomContext("trials", std::to_string(trials));
    benchmark::AddCustomContext("offset", std::to_string(offset));
    benchmark::AddCustomContext("dimensions", std::to_string(dimensions));
    benchmark::AddCustomContext("benchmark_host", std::to_string(benchmark_host));

    const std::vector<rng_type_t> benchmarked_engine_types{
        CURAND_RNG_PSEUDO_MT19937,
        CURAND_RNG_PSEUDO_MTGP32,
        CURAND_RNG_PSEUDO_MRG32K3A,
        CURAND_RNG_PSEUDO_PHILOX4_32_10,
        CURAND_RNG_QUASI_SCRAMBLED_SOBOL32,
        CURAND_RNG_QUASI_SCRAMBLED_SOBOL64,
        CURAND_RNG_QUASI_SOBOL32,
        CURAND_RNG_QUASI_SOBOL64,
        CURAND_RNG_PSEUDO_XORWOW,
    };

    const std::map<curandOrdering, std::string> ordering_name_map{
        {CURAND_ORDERING_PSEUDO_DEFAULT, "default"},
        { CURAND_ORDERING_PSEUDO_LEGACY,  "legacy"},
        {   CURAND_ORDERING_PSEUDO_BEST,    "best"},
        {CURAND_ORDERING_PSEUDO_DYNAMIC, "dynamic"},
        { CURAND_ORDERING_PSEUDO_SEEDED,  "seeded"},
        { CURAND_ORDERING_QUASI_DEFAULT, "default"},
    };

    const std::map<rng_type_t, std::vector<curandOrdering>> benchmarked_orderings{
  // clang-format off
        {          CURAND_RNG_PSEUDO_MTGP32,
            {CURAND_ORDERING_PSEUDO_DEFAULT, CURAND_ORDERING_PSEUDO_DYNAMIC}},
        {         CURAND_RNG_PSEUDO_MT19937, {CURAND_ORDERING_PSEUDO_DEFAULT}},
        {          CURAND_RNG_PSEUDO_XORWOW,
            {CURAND_ORDERING_PSEUDO_DEFAULT, CURAND_ORDERING_PSEUDO_DYNAMIC} },
        {        CURAND_RNG_PSEUDO_MRG32K3A,
            {CURAND_ORDERING_PSEUDO_DEFAULT, CURAND_ORDERING_PSEUDO_DYNAMIC}},
        {   CURAND_RNG_PSEUDO_PHILOX4_32_10,
            {CURAND_ORDERING_PSEUDO_DEFAULT, CURAND_ORDERING_PSEUDO_DYNAMIC}},
        {          CURAND_RNG_QUASI_SOBOL32,  {CURAND_ORDERING_QUASI_DEFAULT}},
        {CURAND_RNG_QUASI_SCRAMBLED_SOBOL32,  {CURAND_ORDERING_QUASI_DEFAULT}},
        {          CURAND_RNG_QUASI_SOBOL64,  {CURAND_ORDERING_QUASI_DEFAULT}},
        {CURAND_RNG_QUASI_SCRAMBLED_SOBOL64,  {CURAND_ORDERING_QUASI_DEFAULT}},
  // clang-format on
    };

    const std::string benchmark_name_prefix = "device_generate";
    // Add benchmarks
    std::vector<benchmark::internal::Benchmark*> benchmarks = {};
    for(const rng_type_t engine_type : benchmarked_engine_types)
    {
        const std::string name = engine_name(engine_type);
        for(const curandOrdering ordering : benchmarked_orderings.at(engine_type))
        {
            const std::string name_engine_prefix
                = benchmark_name_prefix + "<" + name + "," + ordering_name_map.at(ordering) + ",";

            benchmarks.emplace_back(benchmark::RegisterBenchmark(
                (name_engine_prefix + "uniform-uint>").c_str(),
                &run_benchmark<unsigned int>,
                [](curandGenerator_t gen, unsigned int* data, size_t size_gen)
                { return curandGenerate(gen, data, size_gen); },
                size,
                byte_size,
                trials,
                dimensions,
                offset,
                engine_type,
                ordering,
                benchmark_host,
                stream));

            if(engine_type == CURAND_RNG_QUASI_SOBOL64
               || engine_type == CURAND_RNG_QUASI_SCRAMBLED_SOBOL64)
            {
                benchmarks.emplace_back(benchmark::RegisterBenchmark(
                    (name_engine_prefix + "uniform-long-long>").c_str(),
                    &run_benchmark<unsigned long long>,
                    [](curandGenerator_t gen, unsigned long long* data, size_t size)
                    { return curandGenerateLongLong(gen, data, size); },
                    size,
                    byte_size,
                    trials,
                    dimensions,
                    offset,
                    engine_type,
                    ordering,
                    benchmark_host,
                    stream));
            }

            benchmarks.emplace_back(
                benchmark::RegisterBenchmark((name_engine_prefix + "uniform-float>").c_str(),
                                             &run_benchmark<float>,
                                             [](curandGenerator_t gen, float* data, size_t size_gen)
                                             { return curandGenerateUniform(gen, data, size_gen); },
                                             size,
                                             byte_size,
                                             trials,
                                             dimensions,
                                             offset,
                                             engine_type,
                                             ordering,
                                             benchmark_host,
                                             stream));

            benchmarks.emplace_back(benchmark::RegisterBenchmark(
                (name_engine_prefix + "uniform-double>").c_str(),
                &run_benchmark<double>,
                [](curandGenerator_t gen, double* data, size_t size_gen)
                { return curandGenerateUniformDouble(gen, data, size_gen); },
                size,
                byte_size,
                trials,
                dimensions,
                offset,
                engine_type,
                ordering,
                benchmark_host,
                stream));

            benchmarks.emplace_back(benchmark::RegisterBenchmark(
                (name_engine_prefix + "normal-float>").c_str(),
                &run_benchmark<float>,
                [](curandGenerator_t gen, float* data, size_t size_gen)
                { return curandGenerateNormal(gen, data, size_gen, 0.0f, 1.0f); },
                size,
                byte_size,
                trials,
                dimensions,
                offset,
                engine_type,
                ordering,
                benchmark_host,
                stream));

            benchmarks.emplace_back(benchmark::RegisterBenchmark(
                (name_engine_prefix + "normal-double>").c_str(),
                &run_benchmark<double>,
                [](curandGenerator_t gen, double* data, size_t size_gen)
                { return curandGenerateNormalDouble(gen, data, size_gen, 0.0, 1.0); },
                size,
                byte_size,
                trials,
                dimensions,
                offset,
                engine_type,
                ordering,
                benchmark_host,
                stream));

            benchmarks.emplace_back(benchmark::RegisterBenchmark(
                (name_engine_prefix + "log-normal-float>").c_str(),
                &run_benchmark<float>,
                [](curandGenerator_t gen, float* data, size_t size_gen)
                { return curandGenerateLogNormal(gen, data, size_gen, 0.0f, 1.0f); },
                size,
                byte_size,
                trials,
                dimensions,
                offset,
                engine_type,
                ordering,
                benchmark_host,
                stream));

            benchmarks.emplace_back(benchmark::RegisterBenchmark(
                (name_engine_prefix + "log-normal-double>").c_str(),
                &run_benchmark<double>,
                [](curandGenerator_t gen, double* data, size_t size_gen)
                { return curandGenerateLogNormalDouble(gen, data, size_gen, 0.0, 1.0); },
                size,
                byte_size,
                trials,
                dimensions,
                offset,
                engine_type,
                ordering,
                benchmark_host,
                stream));

            for(auto lambda : poisson_lambdas)
            {
                const std::string poisson_dis_name
                    = std::string("poisson(lambda=") + std::to_string(lambda) + ")>";
                benchmarks.emplace_back(benchmark::RegisterBenchmark(
                    (name_engine_prefix + poisson_dis_name).c_str(),
                    &run_benchmark<unsigned int>,
                    [lambda](curandGenerator_t gen, unsigned int* data, size_t size_gen)
                    { return curandGeneratePoisson(gen, data, size_gen, lambda); },
                    size,
                    byte_size,
                    trials,
                    dimensions,
                    offset,
                    engine_type,
                    ordering,
                    benchmark_host,
                    stream));
            }
        }
    }

    for(auto& b : benchmarks)
    {
        b->UseManualTime();
        b->Unit(benchmark::kMillisecond);
    }

    benchmark::BenchmarkReporter* console_reporter  = getConsoleReporter(consoleFormat);
    benchmark::BenchmarkReporter* out_file_reporter = getOutFileReporter(outFormat);

    std::string spec = (filter == "" || filter == "all") ? "." : filter;

    // Run benchmarks
    if(outFormat == "") // default case
    {
        benchmark::RunSpecifiedBenchmarks(console_reporter, spec);
    }
    else
    {
        benchmark::RunSpecifiedBenchmarks(console_reporter, out_file_reporter, spec);
    }

    CUDA_CALL(cudaStreamDestroy(stream));

    return 0;
}
