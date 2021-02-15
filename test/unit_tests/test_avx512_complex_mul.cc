#include <gtest/gtest.h>
#include <time.h>
#include "comms-lib.h"


#ifdef __AVX512F__


TEST(TestComplexMul, Multiply) 
{
    float values[32] __attribute((aligned(64)));
    float out256[32] __attribute((aligned(64)));
    float out512[32] __attribute((aligned(64)));
    clock_t time;
    double cpu_time;
    for (int i = 0; i < 32; i++) {
        // Set each float to a random value between -1 and 1
        values[i] 
            = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    }
    /* Load the generated values */
    __m256 values0_lower_256 
        = _mm256_load_ps(reinterpret_cast<const float*>(values));
    __m256 values0_upper_256 
        = _mm256_load_ps(reinterpret_cast<const float*>(values + 8));
    __m256 values1_lower_256
        = _mm256_load_ps(reinterpret_cast<const float*>(values + 16));
    __m256 values1_upper_256
        = _mm256_load_ps(reinterpret_cast<const float*>(values + 24));
    __m512 values0_512
        = _mm512_load_ps(reinterpret_cast<const float*>(values));
    __m512 values1_512
        = _mm512_load_ps(reinterpret_cast<const float*>(values + 16));
    /* Do the multiplication */
    time = clock();
    __m256 result_256_lower = CommsLib::__m256_complex_cf32_mult(
        values0_lower_256, values1_lower_256, false);
    __m256 result_256_upper = CommsLib::__m256_complex_cf32_mult(
        values0_upper_256, values1_upper_256, false);
    time = clock() - time;
    cpu_time = ((double)time) / CLOCKS_PER_SEC;
    std::cout << "AVX256 Multiplication took " << cpu_time << "seconds";
    time = clock();
    __m512 result_512 
        = CommsLib::__m512_complex_cf32_mult(values0_512, values1_512, false);
    time = clock() - time;
    cpu_time = ((double)time) / CLOCKS_PER_SEC;
    std::cout << "AVX512 Multiplication took " << cpu_time << "seconds";
    /* Extract the results into output buffers */
    _mm256_stream_ps(reinterpret_cast<float*>(out256), result_256_lower);
    _mm256_stream_ps(reinterpret_cast<float*>(out256 + 16), result_256_upper);
    _mm512_stream_ps(reinterpret_cast<float*>(out512), result_512);
    ASSERT_EQ(memcmp(out512, out256, sizeof(out512)), 0) 
        << "AVX512 and AVX256 multiplication differ";
}

TEST(TestComplexMul, ConjMultiply)
{
    float values[32] __attribute((aligned(64)));
    float out256[32] __attribute((aligned(64)));
    float out512[32] __attribute((aligned(64)));
    clock_t time;
    double cpu_time;
    for (int i = 0; i < 32; i++) {
        // Set each float to a random value between -1 and 1
        values[i] 
            = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    }
    /* Load the generated values */
    __m256 values0_lower_256 
        = _mm256_load_ps(reinterpret_cast<const float*>(values));
    __m256 values0_upper_256 
        = _mm256_load_ps(reinterpret_cast<const float*>(values + 8));
    __m256 values1_lower_256
        = _mm256_load_ps(reinterpret_cast<const float*>(values + 16));
    __m256 values1_upper_256
        = _mm256_load_ps(reinterpret_cast<const float*>(values + 24));
    __m512 values0_512
        = _mm512_load_ps(reinterpret_cast<const float*>(values));
    __m512 values1_512
        = _mm512_load_ps(reinterpret_cast<const float*>(values + 16));
    /* Do the multiplication */
    time = clock();
    __m256 result_256_lower = CommsLib::__m256_complex_cf32_mult(
        values0_lower_256, values1_lower_256, true);
    __m256 result_256_upper = CommsLib::__m256_complex_cf32_mult(
        values0_upper_256, values1_upper_256, true);
    time = clock() - time;
    cpu_time = ((double)time) / CLOCKS_PER_SEC;
    std::cout << "AVX256 Conj Multiplication took " << cpu_time << "seconds";
    time = clock();
    __m512 result_512 
        = CommsLib::__m512_complex_cf32_mult(values0_512, values1_512, true);
    time = clock() - time;
    cpu_time = ((double)time) / CLOCKS_PER_SEC;
    std::cout << "AVX512 Conj Multiplication took " << cpu_time << "seconds";
    /* Extract the results into output buffers */
    _mm256_stream_ps(reinterpret_cast<float*>(out256), result_256_lower);
    _mm256_stream_ps(reinterpret_cast<float*>(out256 + 16), result_256_upper);
    _mm512_stream_ps(reinterpret_cast<float*>(out512), result_512);
    ASSERT_EQ(memcmp(out512, out256, sizeof(out512)), 0) 
        << "AVX512 and AVX256 conjugate multiplication differ";
}

#endif

int main(int argc, char** argv)
{
#ifdef __AVX512F__
    testing::InitGoogleTest(&argc, argv);
    std::cout << "---- CommsLib AVX512 Channel Estimation ----\n";
    return RUN_ALL_TESTS();
    #else
    (void)argc;
    (void)argv;
    std::cout << "Platform does not support AVX512!\n";
    return 0;
#endif 
}