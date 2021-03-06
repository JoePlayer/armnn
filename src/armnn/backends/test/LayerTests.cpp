﻿//
// Copyright © 2017 Arm Ltd. All rights reserved.
// See LICENSE file in the project root for full license information.
//
#include "LayerTests.hpp"

#include "test/TensorHelpers.hpp"
#include "TensorCopyUtils.hpp"

#include <boost/test/unit_test.hpp>

#include "armnn/LayerSupport.hpp"

#include "backends/CpuTensorHandle.hpp"
#include "backends/WorkloadFactory.hpp"

#ifdef ARMCOMPUTECL_ENABLED
#include "backends/ClTensorHandle.hpp"
#include "backends/ArmComputeTensorUtils.hpp"
#endif

#include <algorithm>
#include <boost/cast.hpp>

#include "WorkloadTestUtils.hpp"
#include "Conv2dTestImpl.hpp"
#include "BatchNormTestImpl.hpp"
#include "ActivationTestImpl.hpp"
#include "Pooling2dTestImpl.hpp"
#include "ReshapeTestImpl.hpp"
#include "FullyConnectedTestImpl.hpp"
#include "SplitterTestImpl.hpp"
#include "SoftmaxTestImpl.hpp"
#include "NormTestImpl.hpp"
#include "PermuteTestImpl.hpp"

// 3-channel 16x8 image used as common input data for a number of Conv2d tests
static std::vector<float> ConvInput3x8x16({
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
});

// 2-channel bias used by a number of Conv2d tests
static std::vector<float> Bias2({0, 2});

// Helper function that returns either Bias2 or an empty vector depending on whether bias is enabled
template<typename T>
boost::multi_array<T, 1> GetBias2(bool biasEnabled, float qScale, int32_t qOffset)
{
    if(biasEnabled)
    {
        armnn::TensorInfo biasDesc({static_cast<unsigned int>(Bias2.size())}, armnn::GetDataType<T>());
        boost::multi_array<T, 1> bias = MakeTensor<T, 1>(biasDesc, QuantizedVector<T>(qScale, qOffset, Bias2));
        return bias;
    }
    else
    {
        return boost::multi_array<T, 1>();
    }
}

template<typename T>
LayerTestResult<T, 4> SimpleConvolution2d3x5TestCommon(armnn::IWorkloadFactory& workloadFactory,
                                                       float                    qScale,
                                                       int32_t                  qOffset,
                                                       bool                     biasEnabled)
{
    // Use common single-batch 3-channel 16x8 image
    armnn::TensorInfo inputDesc({1, 3, 8, 16}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, QuantizedVector<T>(qScale, qOffset, ConvInput3x8x16));

    // Use a 2-element batch with 3-channel 3x5 kernels
    armnn::TensorInfo kernelDesc({2, 3, 5, 3}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            1, 1, 1,
            1, -1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            2, 2, 2,
            2, 2, 2,
            2, 2, 2,
            2, 2, 2,
            2, 2, 2,


            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            1, 1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0
        })));

    // Expected output is 2 batch elements of a 1-channel 14x4 image
    armnn::TensorInfo outputDesc({1, 2, 4, 14}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24,
            -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
            -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f,
            -23.5f, -23.5f, -23.5f,
            -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f,
            -23.5f, -23.5f, -23.5f,

            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        })));

    return SimpleConvolution2dTestImpl<T>(workloadFactory,
      input,
      kernel,
      GetBias2<typename FullyConnectedBiasTypeForInputType<T>::Type>(biasEnabled, qScale, qOffset),
      expectedOutput,
      qScale,
      qOffset);
}

template<typename T>
LayerTestResult<T, 4> SimpleConvolution2d3x3TestCommon(armnn::IWorkloadFactory& workloadFactory,
                                                       float                    qScale,
                                                       int32_t                  qOffset,
                                                       bool                     biasEnabled)
{
    // Use a 3x3 kernel, which exercises ArmCompute's direct convolution path

    // Use common single-batch 3-channel 16x8 image
    armnn::TensorInfo inputDesc({1, 3, 8, 16}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, QuantizedVector<T>(qScale, qOffset, ConvInput3x8x16));

    // Use a 2-element batch of 3-channel 3x3 kernels
    armnn::TensorInfo kernelDesc({2, 3, 3, 3}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            1, 1, 1,
            1, -1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            2, 2, 2,
            2, 2, 2,
            2, 2, 2,


            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            1, 1, 1,
            1, 1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0
        })));

    // Expected output is 1 batch of a 2-channel 14x6 image
    armnn::TensorInfo outputDesc({1, 2, 6, 14}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15,
            -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,

            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        })));

    return SimpleConvolution2dTestImpl<T>(workloadFactory,
      input,
      kernel,
      GetBias2<typename FullyConnectedBiasTypeForInputType<T>::Type>(biasEnabled, qScale, qOffset),
      expectedOutput,
      qScale,
      qOffset);
}

LayerTestResult<float, 4> SimpleConvolution2d3x5Test(armnn::IWorkloadFactory& workloadFactory,
                                                     bool                     biasEnabled)
{
    return SimpleConvolution2d3x5TestCommon<float>(workloadFactory, 0.f, 0, biasEnabled);
}

LayerTestResult<uint8_t, 4> SimpleConvolution2d3x5Uint8Test(armnn::IWorkloadFactory& workloadFactory,
                                                         bool                     biasEnabled)
{
    return SimpleConvolution2d3x5TestCommon<uint8_t>(workloadFactory, 0.5f, 50, biasEnabled);
}

LayerTestResult<float, 4> SimpleConvolution2d3x3Test(armnn::IWorkloadFactory& workloadFactory,
                                                     bool                     biasEnabled)
{
    return SimpleConvolution2d3x3TestCommon<float>(workloadFactory, 0.f, 0, biasEnabled);
}

LayerTestResult<uint8_t, 4> SimpleConvolution2d3x3Uint8Test(armnn::IWorkloadFactory& workloadFactory,
                                                            bool                     biasEnabled)
{
    return SimpleConvolution2d3x3TestCommon<uint8_t>(workloadFactory, 0.5f, 50, biasEnabled);
}

template<typename T>
LayerTestResult<T, 4> Convolution2dAsymmetricPaddingLargerThanHalfKernelSizeTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    float                    qScale,
    int32_t                  qOffset)
{
    // Use a single-batch 1-channel 3x3 image as input
    armnn::TensorInfo inputDesc({1, 1, 3, 3}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            11,21,31,
            12,22,32,
            13,23,33
        })));

    // Use 1 batch of a 1-channel 2x2 kernel
    armnn::TensorInfo kernelDesc({1, 1, 2, 2}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -11,-21,
            -12,-22,
        })));

// Expected output is 1 batch of a 1-channel 6x8 image
// Manually calculated like this:
//[-11*0 -21*0  -12*0 -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0 -12*0  -22*0 ..]
//[-11*0 -21*0  -12*0 -22*11 ; -11*0  -21*0  -12*11 -22*21 ; -11*0  -21*0  -12*21 -22*31 ; -11*0  -21*0 -12*31 -22*0 ..]
//[-11*0 -21*11 -12*0 -22*12 ; -11*11 -21*21 -12*12 -22*22 ; -11*21 -21*31 -12*22 -22*32 ; -11*31 -21*0 -12*32 -22*0 ..]
//[-11*0 -21*12 -12*0 -22*13 ; -11*12 -21*22 -12*13 -22*23 ; -11*22 -21*32 -12*23 -22*33 ; -11*32 -21*0 -12*33 -22*0 ..]
//[-11*0 -21*13 -12*0 -22*0  ; -11*13 -21*23 -12*0  -22*0  ; -11*23 -21*33 -12*0  -22*0  ; -11*33 -21*0 -12*0  -22*0 ..]
//[-11*0 -21*0  -12*0 -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0 -12*0  -22*0 ..]
//[..... .....  ..... .....  ; .....  .....  .....  .....  ; .....  .....  .....  .....  ; .....  ..... .....  ..... ..]
    armnn::TensorInfo outputDesc({1, 1, 8, 6}, armnn::GetDataType<T>());
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
               0,    0,      0,    0,    0,    0,
            -242,  -594,  -934, -372,    0,    0,
            -495, -1190, -1850, -725,    0,    0,
            -538, -1256, -1916, -748,    0,    0,
            -273, -626,  -946,  -363,    0,    0,
               0,    0,     0,     0,    0,    0,
               0,    0,     0,     0,    0,    0,
               0,    0,     0,     0,    0,    0
        })));

    return SimpleConvolution2dTestImpl<T>(workloadFactory,
      input,
      kernel,
      GetBias2<typename FullyConnectedBiasTypeForInputType<T>::Type>(false, qScale, qOffset),
      expectedOutput,
      qScale,
      qOffset,
      1,  // padding left
      2,  // padding top
      3,  // padding right
      4); // padding bottom
}

template<typename T>
LayerTestResult<T, 4> SimpleConvolution2dAsymmetricPaddingTestCommon(armnn::IWorkloadFactory& workloadFactory,
    float                    qScale,
    int32_t                  qOffset)
{
    // Use a single-batch 1-channel 5x5 image as input
    armnn::TensorInfo inputDesc({ 1, 1, 5, 5 }, armnn::GetDataType<T>());
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            11,21,31,41,51,
            12,22,32,42,52,
            13,23,33,43,53,
            14,24,34,44,54,
            15,25,35,45,55,
        })));

    // Use 1 batch of a 1-channel 4x4 kernel
    armnn::TensorInfo kernelDesc({ 1, 1, 4, 4 }, armnn::GetDataType<T>());
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -11,-21,-31,-41,
            -12,-22,-32,-42,
            -13,-23,-33,-43,
            -14,-24,-34,-44,
        })));

    // Expected output is 1 batch of a 1-channel 5x5 image
    armnn::TensorInfo outputDesc({ 1, 1, 5, 5 }, armnn::GetDataType<T>());
    std::vector<T> myVec(outputDesc.GetNumElements(), 0);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -4723,  -7044,  -9324,  -6253, -3542,
            -7140, -10580, -13940,  -9300, -5230,
            -9590, -14120, -18520, -12290, -6860,
            -9980, -14560, -18960, -12560, -7000,
            -7518, -10904, -14144,  -9318, -5152,
        })));

    return SimpleConvolution2dTestImpl<T>(workloadFactory,
        input,
        kernel,
        GetBias2<typename FullyConnectedBiasTypeForInputType<T>::Type>(false, qScale, qOffset),
        expectedOutput,
        qScale,
        qOffset,
        1,  // padding left
        2,  // padding top
        2,  // padding right
        1); // padding bottom
}

LayerTestResult<float, 4>
Convolution2dAsymmetricPaddingLargerThanHalfKernelSizeTest(armnn::IWorkloadFactory& workloadFactory)
{
    return Convolution2dAsymmetricPaddingLargerThanHalfKernelSizeTestCommon<float>(workloadFactory, 0.0f, 0);
}

LayerTestResult<float, 4> Convolution2dAsymmetricPaddingTest(armnn::IWorkloadFactory& workloadFactory)
{
    return SimpleConvolution2dAsymmetricPaddingTestCommon<float>(workloadFactory, 0.0f, 0);
}

LayerTestResult<float, 4> DepthwiseConvolution2dTest(armnn::IWorkloadFactory& workloadFactory,
                                                     bool                     biasEnabled)
{
    return DepthwiseConvolution2dTestImpl<float, float>(workloadFactory, 0.0f, 0, biasEnabled);
}

LayerTestResult<float, 4> DepthwiseConvolution2dDepthMul1Test(armnn::IWorkloadFactory& workloadFactory,
                                                              bool biasEnabled)
{
    return DepthwiseConvolution2dDepthMul1TestImpl<float, float>(workloadFactory, 0.0f, 0, biasEnabled);
}

LayerTestResult<uint8_t, 4> DepthwiseConvolution2dUint8Test(armnn::IWorkloadFactory& workloadFactory,
                                                            bool                     biasEnabled)
{
    return DepthwiseConvolution2dTestImpl<uint8_t, int32_t>(workloadFactory, 0.5f, 50, biasEnabled);
}

LayerTestResult<uint8_t, 4> DepthwiseConvolution2dDepthMul1Uint8Test(armnn::IWorkloadFactory& workloadFactory,
                                                                     bool biasEnabled)
{
    return DepthwiseConvolution2dDepthMul1TestImpl<uint8_t, int32_t>(workloadFactory, 0.5f, 50, biasEnabled);
}

LayerTestResult<float, 4> Convolution1dTest(armnn::IWorkloadFactory& workloadFactory, bool biasEnabled)
{
    return Convolution1dTestImpl<float>(workloadFactory, 0.0f, 0, biasEnabled);
}

LayerTestResult<uint8_t, 4> Convolution1dUint8Test(armnn::IWorkloadFactory& workloadFactory, bool biasEnabled)
{
    return Convolution1dTestImpl<uint8_t>(workloadFactory, 0.1f, 128, biasEnabled);
}

LayerTestResult<float,4> CompareConvolution2dTest(armnn::IWorkloadFactory& workloadFactory,
                                                armnn::IWorkloadFactory& refWorkloadFactory)
{
    return CompareConvolution2dTestImpl<float>(workloadFactory, refWorkloadFactory);
}

template<typename T>
LayerTestResult<T,4> CompareDepthwiseConvolution2dTest(armnn::IWorkloadFactory& workloadFactory,
    armnn::IWorkloadFactory& refWorkloadFactory)
{
    return CompareDepthwiseConvolution2dTestImpl<T>(workloadFactory, refWorkloadFactory);
}

template LayerTestResult<float, 4> CompareDepthwiseConvolution2dTest<float>(
    armnn::IWorkloadFactory&, armnn::IWorkloadFactory&);
template LayerTestResult<uint8_t, 4> CompareDepthwiseConvolution2dTest<uint8_t>(
    armnn::IWorkloadFactory&, armnn::IWorkloadFactory&);

LayerTestResult<float,4> SimpleNormalizationAcrossTest(armnn::IWorkloadFactory& workloadFactory)
{
    auto normMethod = armnn::NormalizationAlgorithmMethod::LocalBrightness;
    auto normChannel = armnn::NormalizationAlgorithmChannel::Across;
    return SimpleNormalizationTestImpl(workloadFactory, normChannel, normMethod);
}

LayerTestResult<float,4> SimpleNormalizationWithinTest(armnn::IWorkloadFactory& workloadFactory)
{
    auto normMethod = armnn::NormalizationAlgorithmMethod::LocalBrightness;
    auto normChannel = armnn::NormalizationAlgorithmChannel::Within;
    return SimpleNormalizationTestImpl(workloadFactory, normChannel, normMethod);
}

LayerTestResult<float,2> SimpleSoftmaxTest(armnn::IWorkloadFactory& workloadFactory, float beta)
{
    return SimpleSoftmaxTestImpl<float>(workloadFactory, beta);
}

LayerTestResult<uint8_t,2> SimpleSoftmaxUint8Test(armnn::IWorkloadFactory& workloadFactory, float beta)
{
    return SimpleSoftmaxTestImpl<uint8_t>(workloadFactory, beta);
}

LayerTestResult<float,4> CompareNormalizationTest(armnn::IWorkloadFactory& workloadFactory,
                                                  armnn::IWorkloadFactory& refWorkloadFactory,
                                                  armnn::NormalizationAlgorithmChannel normChannel,
                                                  armnn::NormalizationAlgorithmMethod normMethod)
{
    return CompareNormalizationTestImpl(workloadFactory, refWorkloadFactory, normChannel, normMethod);
}

LayerTestResult<float,2> CompareSoftmaxTest(armnn::IWorkloadFactory& workloadFactory,
    armnn::IWorkloadFactory& refWorkloadFactory,
    float beta)
{
    return CompareSoftmaxTestImpl<float>(workloadFactory, refWorkloadFactory, beta);
}

LayerTestResult<uint8_t,2> CompareSoftmaxUint8Test(armnn::IWorkloadFactory& workloadFactory,
    armnn::IWorkloadFactory& refWorkloadFactory,
    float beta)
{
    return CompareSoftmaxTestImpl<uint8_t>(workloadFactory, refWorkloadFactory, beta);
}

std::vector<LayerTestResult<float,3>> SplitterTest(armnn::IWorkloadFactory& workloadFactory)
{
    return SplitterTestCommon<float>(workloadFactory);
}

std::vector<LayerTestResult<uint8_t,3>> SplitterUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return SplitterTestCommon<uint8_t>(workloadFactory, 1.0f, 0);
}

LayerTestResult<float, 3> CopyViaSplitterTest(armnn::IWorkloadFactory& workloadFactory)
{
    return CopyViaSplitterTestImpl<float>(workloadFactory, 0.0f, 0);
}

LayerTestResult<uint8_t, 3> CopyViaSplitterUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return CopyViaSplitterTestImpl<uint8_t>(workloadFactory, 1.0f, 0);
}

LayerTestResult<float,3> MergerTest(armnn::IWorkloadFactory& workloadFactory)
{
    unsigned int outputWidth = 5;
    unsigned int outputHeight = 6;
    unsigned int outputChannels = 3;

    unsigned int inputWidth1 = 2;
    unsigned int inputHeight1 = 2;
    unsigned int inputChannels1 = 3;

    unsigned int inputWidth2 = 2;
    unsigned int inputHeight2 = 4;
    unsigned int inputChannels2 = 3;

    unsigned int inputWidth3 = 3;
    unsigned int inputHeight3 = 6;
    unsigned int inputChannels3 = 2;

    unsigned int inputWidth4 = 3;
    unsigned int inputHeight4 = 6;
    unsigned int inputChannels4 = 1;

    // Define the tensor descriptors
    armnn::TensorInfo outputTensorInfo({ outputChannels, outputHeight, outputWidth }, armnn::DataType::Float32);
    armnn::TensorInfo inputTensorInfo1({ inputChannels1, inputHeight1, inputWidth1 }, armnn::DataType::Float32);
    armnn::TensorInfo inputTensorInfo2({ inputChannels2, inputHeight2, inputWidth2 }, armnn::DataType::Float32);
    armnn::TensorInfo inputTensorInfo3({ inputChannels3, inputHeight3, inputWidth3 }, armnn::DataType::Float32);
    armnn::TensorInfo inputTensorInfo4({ inputChannels4, inputHeight4, inputWidth4 }, armnn::DataType::Float32);

    LayerTestResult<float,3> ret(outputTensorInfo);


    ret.outputExpected = MakeTensor<float, 3>(outputTensorInfo, std::vector<float>(
        {
            1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
            6.0f, 7.0f, 8.0f, 9.0f, 10.0f,
            11.0f, 12.0f, 13.0f, 14.0f, 15.0f,
            16.0f, 17.0f, 18.0f, 19.0f, 20.0f,
            21.0f, 22.0f, 23.0f, 24.0f, 25.0f,
            26.0f, 27.0f, 28.0f, 29.0f, 30.0f,

            31.0f, 32.0f, 33.0f, 34.0f, 35.0f,
            36.0f, 37.0f, 38.0f, 39.0f, 40.0f,
            41.0f, 42.0f, 43.0f, 44.0f, 45.0f,
            46.0f, 47.0f, 48.0f, 49.0f, 50.0f,
            51.0f, 52.0f, 53.0f, 54.0f, 55.0f,
            56.0f, 57.0f, 58.0f, 59.0f, 60.0f,

            61.0f, 62.0f, 63.0f, 64.0f, 65.0f,
            66.0f, 67.0f, 68.0f, 69.0f, 70.0f,
            71.0f, 72.0f, 73.0f, 74.0f, 75.0f,
            76.0f, 77.0f, 78.0f, 79.0f, 80.0f,
            81.0f, 82.0f, 83.0f, 84.0f, 85.0f,
            86.0f, 87.0f, 88.0f, 89.0f, 90.0f,

        })
    );


    auto input1 = MakeTensor<float, 3>(inputTensorInfo1, std::vector<float>(
        {
            1.0f, 2.0f,
            6.0f, 7.0f,

            31.0f, 32.0f,
            36.0f, 37.0f,

            61.0f, 62.0f,
            66.0f, 67.0f,
        })
    );

    auto input2 = MakeTensor<float, 3>(inputTensorInfo2, std::vector<float>(
        {
            11.0f, 12.0f,
            16.0f, 17.0f,
            21.0f, 22.0f,
            26.0f, 27.0f,

            41.0f, 42.0f,
            46.0f, 47.0f,
            51.0f, 52.0f,
            56.0f, 57.0f,

            71.0f, 72.0f,
            76.0f, 77.0f,
            81.0f, 82.0f,
            86.0f, 87.0f,
        })
    );

    auto input3 = MakeTensor<float, 3>(inputTensorInfo3, std::vector<float>(
        {
            3.0f, 4.0f, 5.0f,
            8.0f, 9.0f, 10.0f,
            13.0f, 14.0f, 15.0f,
            18.0f, 19.0f, 20.0f,
            23.0f, 24.0f, 25.0f,
            28.0f, 29.0f, 30.0f,

            33.0f, 34.0f, 35.0f,
            38.0f, 39.0f, 40.0f,
            43.0f, 44.0f, 45.0f,
            48.0f, 49.0f, 50.0f,
            53.0f, 54.0f, 55.0f,
            58.0f, 59.0f, 60.0f,
        })
    );


    auto input4 = MakeTensor<float, 3>(inputTensorInfo4, std::vector<float>(
        {
            63.0f, 64.0f, 65.0f,
            68.0f, 69.0f, 70.0f,
            73.0f, 74.0f, 75.0f,
            78.0f, 79.0f, 80.0f,
            83.0f, 84.0f, 85.0f,
            88.0f, 89.0f, 90.0f,
        })
    );

    std::vector<unsigned int> wOrigin1 = {0, 0, 0}; //extent of the window is defined by size of input[0]
    armnn::MergerQueueDescriptor::ViewOrigin window1(wOrigin1);

    std::vector<unsigned int> wOrigin2 = {0, 2, 0}; //extent of the window is defined by size of input[1]
    armnn::MergerQueueDescriptor::ViewOrigin window2(wOrigin2);

    std::vector<unsigned int> wOrigin3 = {0, 0, 2}; //extent of the window is defined by size of input[2]
    armnn::MergerQueueDescriptor::ViewOrigin window3(wOrigin3);

    std::vector<unsigned int> wOrigin4 = {2, 0, 2}; //extent of the window is defined by size of input[3]
    armnn::MergerQueueDescriptor::ViewOrigin window4(wOrigin4);


    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    bool subTensorsSupported = workloadFactory.SupportsSubTensors();

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo1.GetShape(), wOrigin1.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo1);

    std::unique_ptr<armnn::ITensorHandle> inputHandle2  =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo2.GetShape(), wOrigin2.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo2);

    std::unique_ptr<armnn::ITensorHandle> inputHandle3  =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo3.GetShape(), wOrigin3.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo3);

    std::unique_ptr<armnn::ITensorHandle> inputHandle4  =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo4.GetShape(), wOrigin4.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo4);


    armnn::MergerQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddInputToWorkload(data, info, inputTensorInfo3, inputHandle3.get());
    AddInputToWorkload(data, info, inputTensorInfo4, inputHandle4.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_ViewOrigins.push_back(window1);
    data.m_ViewOrigins.push_back(window2);
    data.m_ViewOrigins.push_back(window3);
    data.m_ViewOrigins.push_back(window4);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateMerger(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    inputHandle3->Allocate();
    inputHandle4->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0]);
    CopyDataToITensorHandle(inputHandle3.get(), &input3[0][0][0]);
    CopyDataToITensorHandle(inputHandle4.get(), &input4[0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0], outputHandle.get());

    return ret;
}

LayerTestResult<float,4> AdditionTest(armnn::IWorkloadFactory& workloadFactory)
{
    unsigned int batchSize = 2;
    unsigned int channels  = 2;
    unsigned int height    = 2;
    unsigned int width     = 3;

    armnn::TensorInfo inputTensorInfo1, inputTensorInfo2;
    armnn::TensorInfo outputTensorInfo;

    unsigned int shape[] = {batchSize, channels, height, width};

    inputTensorInfo1 = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    inputTensorInfo2 = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    outputTensorInfo = armnn::TensorInfo(4, shape, armnn::DataType::Float32);


    auto input1 = MakeTensor<float, 4>(inputTensorInfo1, std::vector<float>(
        {
            0.0f, 2.0f, 1.0f,
            0.2f, 1.0f, 2.0f,

            1.0f, 2.0f, 1.0f,
            0.2f, 1.0f, 2.0f,

            0.0f, 2.0f, 1.0f,
            4.2f, 1.0f, 2.0f,

            0.0f, 0.0f, 1.0f,
            0.2f, 1.0f, 2.0f,
        }));

    auto input2 = MakeTensor<float, 4>(inputTensorInfo2, std::vector<float>(
        {
            1.0f, 2.0f, 1.0f,
            0.0f, 1.0f, 2.0f,

            1.0f, 2.0f, -2.0f,
            0.2f, 1.0f, 2.0f,

            0.0f, 2.0f, 1.0f,
            4.2f, 0.0f, -3.0f,

            0.0f, 0.0f, 1.0f,
            0.7f, 1.0f, 5.0f,
        }));

    LayerTestResult<float,4> ret(outputTensorInfo);
    ret.outputExpected = MakeTensor<float, 4>(outputTensorInfo, std::vector<float>(
        {
            1.0f, 4.0f, 2.0f,
            0.2f, 2.0f, 4.0f,

            2.0f, 4.0f, -1.0f,
            0.4f, 2.0f, 4.0f,

            0.0f, 4.0f, 2.0f,
            8.4f, 1.0f, -1.0f,

            0.0f, 0.0f, 2.0f,
            0.9f, 2.0f, 7.0f,
        }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> inputHandle2 = workloadFactory.CreateTensorHandle(inputTensorInfo2);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::AdditionQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateAddition(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template <typename T>
LayerTestResult<T, 4> AdditionBroadcastTestImpl(armnn::IWorkloadFactory& workloadFactory,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo1 = armnn::TensorInfo({1, 3, 2, 1}, armnn::GetDataType<T>());
    armnn::TensorInfo inputTensorInfo2 = armnn::TensorInfo({1, 1, 2, 3}, armnn::GetDataType<T>());
    armnn::TensorInfo outputTensorInfo = armnn::TensorInfo({1, 3, 2, 3}, armnn::GetDataType<T>());

    if (armnn::IsQuantizedType<T>())
    {
        inputTensorInfo1.SetQuantizationScale(qScale);
        inputTensorInfo1.SetQuantizationOffset(qOffset);
        inputTensorInfo2.SetQuantizationScale(qScale);
        inputTensorInfo2.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
    }

    auto input1 = MakeTensor<T, 4>(inputTensorInfo1, QuantizedVector<T>(qScale, qOffset,
        {
            0.0f,
            1.0f,

            2.0f,
            3.0f,

            4.0f,
            5.0f,
        }));

    auto input2 = MakeTensor<T, 4>(inputTensorInfo2, QuantizedVector<T>(qScale, qOffset,
        {
            0.5f, 1.5f, 2.5f,
            3.5f, 4.5f, 5.5f,
        }));

    LayerTestResult<T,4> ret(outputTensorInfo);
    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset,
        {
            0.5f, 1.5f, 2.5f,
            4.5f, 5.5f, 6.5f,

            2.5f, 3.5f, 4.5f,
            6.5f, 7.5f, 8.5f,

            4.5f, 5.5f, 6.5f,
            8.5f, 9.5f, 10.5f,
        }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> inputHandle2 = workloadFactory.CreateTensorHandle(inputTensorInfo2);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::AdditionQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateAddition(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template <typename T>
LayerTestResult<T, 4> AdditionBroadcast1ElementTestImpl(armnn::IWorkloadFactory& workloadFactory,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo1 = armnn::TensorInfo({1, 3, 2, 3}, armnn::GetDataType<T>());
    armnn::TensorInfo inputTensorInfo2 = armnn::TensorInfo({1, 1, 1, 1}, armnn::GetDataType<T>());
    armnn::TensorInfo outputTensorInfo = armnn::TensorInfo({1, 3, 2, 3}, armnn::GetDataType<T>());

    if (armnn::IsQuantizedType<T>())
    {
        inputTensorInfo1.SetQuantizationScale(qScale);
        inputTensorInfo1.SetQuantizationOffset(qOffset);
        inputTensorInfo2.SetQuantizationScale(qScale);
        inputTensorInfo2.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
    }

    auto input1 = MakeTensor<T, 4>(inputTensorInfo1, QuantizedVector<T>(qScale, qOffset,
        {
             0.0f,  1.0f,  2.0f,
             3.0f,  4.0f,  5.0f,
             6.0f,  7.0f,  8.0f,
             9.0f, 10.0f, 11.0f,
            12.0f, 13.0f, 14.0f,
            15.0f, 16.0f, 17.0f,
        }));

    auto input2 = MakeTensor<T, 4>(inputTensorInfo2, QuantizedVector<T>(qScale, qOffset,
        {
            0.5f,
        }));

    LayerTestResult<T,4> ret(outputTensorInfo);
    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset,
        {
             0.5f,  1.5f,  2.5f,
             3.5f,  4.5f,  5.5f,
             6.5f,  7.5f,  8.5f,
             9.5f, 10.5f, 11.5f,
            12.5f, 13.5f, 14.5f,
            15.5f, 16.5f, 17.5f,
        }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> inputHandle2 = workloadFactory.CreateTensorHandle(inputTensorInfo2);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::AdditionQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateAddition(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

LayerTestResult<float, 4> AdditionBroadcastTest(armnn::IWorkloadFactory& workloadFactory)
{
    return AdditionBroadcastTestImpl<float>(workloadFactory, 0.0f, 0);
}

LayerTestResult<uint8_t, 4> AdditionBroadcastUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return AdditionBroadcastTestImpl<uint8_t>(workloadFactory, 2.f, 0);
}

LayerTestResult<float, 4> AdditionBroadcast1ElementTest(armnn::IWorkloadFactory& workloadFactory)
{
    return AdditionBroadcast1ElementTestImpl<float>(workloadFactory, 0.0f, 0);
}

LayerTestResult<uint8_t, 4> AdditionBroadcast1ElementUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return AdditionBroadcast1ElementTestImpl<uint8_t>(workloadFactory, 0.1333333f, 128);
}

LayerTestResult<float,4> CompareAdditionTest(armnn::IWorkloadFactory& workloadFactory,
                                    armnn::IWorkloadFactory& refWorkloadFactory)
{
    unsigned int batchSize = 4;
    unsigned int channels  = 1;
    unsigned int height    = 2;
    unsigned int width     = 3;

    armnn::TensorInfo inputTensorInfo1, inputTensorInfo2;
    armnn::TensorInfo outputTensorInfo;

    unsigned int shape[] = {batchSize, channels, height, width};

    inputTensorInfo1 = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    inputTensorInfo2 = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    outputTensorInfo = armnn::TensorInfo(4, shape, armnn::DataType::Float32);

    auto input1 = MakeRandomTensor<float, 4>(inputTensorInfo1, 1232);
    auto input2 = MakeRandomTensor<float, 4>(inputTensorInfo2, 456);

    LayerTestResult<float,4> ret(outputTensorInfo);

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> inputHandle2 = workloadFactory.CreateTensorHandle(inputTensorInfo2);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    std::unique_ptr<armnn::ITensorHandle> inputHandle1Ref = refWorkloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> inputHandle2Ref = refWorkloadFactory.CreateTensorHandle(inputTensorInfo2);
    std::unique_ptr<armnn::ITensorHandle> outputHandleRef = refWorkloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::AdditionQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    armnn::AdditionQueueDescriptor refData = data;
    armnn::WorkloadInfo refInfo = info;
    SetWorkloadInput(refData, refInfo, 0, inputTensorInfo1, inputHandle1Ref.get());
    SetWorkloadInput(refData, refInfo, 1, inputTensorInfo2, inputHandle2Ref.get());
    SetWorkloadOutput(refData, refInfo, 0, outputTensorInfo, outputHandleRef.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateAddition(data, info);
    std::unique_ptr<armnn::IWorkload> workloadRef = refWorkloadFactory.CreateAddition(refData, refInfo);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();
    inputHandle1Ref->Allocate();
    inputHandle2Ref->Allocate();
    outputHandleRef->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle1Ref.get(), &input1[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle2Ref.get(), &input2[0][0][0][0]);

    workload->Execute();
    workloadRef->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());
    CopyDataFromITensorHandle(&ret.outputExpected[0][0][0][0], outputHandleRef.get());

    return ret;
}

namespace {
LayerTestResult<float,4> MultiplicationTestHelper(armnn::IWorkloadFactory& workloadFactory,
                                                  const unsigned int shape0[4],
                                                  const std::vector<float> & values0,
                                                  const unsigned int shape1[4],
                                                  const std::vector<float> & values1,
                                                  const unsigned int outShape[4],
                                                  const std::vector<float> & outValues)
{
    const size_t dimensionCount = 4;
    armnn::TensorInfo inputTensorInfo0{dimensionCount, shape0, armnn::DataType::Float32};
    armnn::TensorInfo inputTensorInfo1{dimensionCount, shape1, armnn::DataType::Float32};
    armnn::TensorInfo outputTensorInfo{dimensionCount, outShape, armnn::DataType::Float32};

    auto input0 = MakeTensor<float, 4>(inputTensorInfo0, values0);
    auto input1 = MakeTensor<float, 4>(inputTensorInfo1, values1);

    LayerTestResult<float,4> ret(outputTensorInfo);

    std::unique_ptr<armnn::ITensorHandle> inputHandle0 = workloadFactory.CreateTensorHandle(inputTensorInfo0);
    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::MultiplicationQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo0, inputHandle0.get());
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateMultiplication(data, info);

    inputHandle0->Allocate();
    inputHandle1->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle0.get(), &input0[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    ret.outputExpected = MakeTensor<float, 4>(outputTensorInfo, outValues);
    return ret;
}
} // anonymous namespace


LayerTestResult<float,4> MultiplicationTest(armnn::IWorkloadFactory& workloadFactory)
{
    const unsigned int width = 2;
    const unsigned int height = 2;
    const unsigned int channelCount = 2;
    const unsigned int batchSize = 2;

    unsigned int shape[] = { batchSize, channelCount, height, width };

    std::vector<float> input0({
        1,  1,  1,  1,    2,  2,  2,  2,
        3,  3,  3,  3,    4,  4,  4,  4 });

    std::vector<float> input1({
        2,  2,  2,  2,    3,  3,  3,  3,
        4,  4,  4,  4,    5,  5,  5,  5 });

    std::vector<float> output({
        2,  2,  2,  2,    6,  6,  6,  6,
        12, 12, 12, 12,  20, 20, 20, 20 });

    return MultiplicationTestHelper(workloadFactory,
                                    shape,
                                    input0,
                                    shape,
                                    input1,
                                    shape,
                                    output);
}

LayerTestResult<float, 4> MultiplicationBroadcast1ElementTest(armnn::IWorkloadFactory& workloadFactory)
{
    unsigned int shape0[] = { 1, 2, 2, 2 };
    std::vector<float> input0({ 1, 2, 3, 4, 5, 6, 7, 8});

    unsigned int shape1[] = { 1, 1, 1, 1 };
    std::vector<float> input1({ 2 });

    std::vector<float> output({ 2, 4, 6, 8, 10, 12, 14, 16});

    return MultiplicationTestHelper(workloadFactory,
                                    shape0,
                                    input0,
                                    shape1,
                                    input1,
                                    shape0,
                                    output);
}

LayerTestResult<float, 4> MultiplicationBroadcast1DVectorTest(armnn::IWorkloadFactory& workloadFactory)
{
    unsigned int shape0[] = { 1, 3, 3, 2 };
    std::vector<float> input0({
        1,   2,      3,  4,      5,  6,
        7,   8,      9, 10,     11, 12,
        13, 14,     15, 16,     17, 18});

    unsigned int shape1[] = { 1, 1, 1, 2 };
    std::vector<float> input1({ 1, 2 });

    std::vector<float> output({
        1,   4,       3,  8,      5, 12,
        7,   16,      9, 20,     11, 24,
        13,  28,     15, 32,     17, 36});

    return MultiplicationTestHelper(workloadFactory,
                                    shape0,
                                    input0,
                                    shape1,
                                    input1,
                                    shape0,
                                    output);
}

LayerTestResult<float,4> CompareMultiplicationTest(armnn::IWorkloadFactory& workloadFactory,
                                          armnn::IWorkloadFactory& refWorkloadFactory)
{
    const unsigned int width = 16;
    const unsigned int height = 32;
    const unsigned int channelCount = 2;
    const unsigned int batchSize = 5;

    armnn::TensorInfo inputTensorInfo0;
    armnn::TensorInfo inputTensorInfo1;
    armnn::TensorInfo outputTensorInfo;

    constexpr unsigned int shape[] = { batchSize, channelCount, height, width };

    inputTensorInfo0 = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    inputTensorInfo1 = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    outputTensorInfo = armnn::TensorInfo(4, shape, armnn::DataType::Float32);

    LayerTestResult<float,4> comparisonResult(outputTensorInfo);

    auto input0 = MakeRandomTensor<float, 4>(inputTensorInfo0, 803506992);
    auto input1 = MakeRandomTensor<float, 4>(inputTensorInfo1, 54902257);

    std::unique_ptr<armnn::ITensorHandle> inputHandle0 = workloadFactory.CreateTensorHandle(inputTensorInfo0);
    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    std::unique_ptr<armnn::ITensorHandle> inputHandle0Ref = refWorkloadFactory.CreateTensorHandle(inputTensorInfo0);
    std::unique_ptr<armnn::ITensorHandle> inputHandle1Ref = refWorkloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> outputHandleRef = refWorkloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::MultiplicationQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo0, inputHandle0.get());
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    armnn::MultiplicationQueueDescriptor refData = data;
    armnn::WorkloadInfo refInfo = info;
    SetWorkloadInput(refData, refInfo, 0, inputTensorInfo0, inputHandle0Ref.get());
    SetWorkloadInput(refData, refInfo, 1, inputTensorInfo1, inputHandle1Ref.get());
    SetWorkloadOutput(refData, refInfo, 0, outputTensorInfo, outputHandleRef.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateMultiplication(data, info);
    std::unique_ptr<armnn::IWorkload> workloadRef = refWorkloadFactory.CreateMultiplication(refData, refInfo);

    inputHandle0->Allocate();
    inputHandle1->Allocate();
    outputHandle->Allocate();
    inputHandle0Ref->Allocate();
    inputHandle1Ref->Allocate();
    outputHandleRef->Allocate();

    CopyDataToITensorHandle(inputHandle0.get(), &input0[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle0Ref.get(), &input0[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle1Ref.get(), &input1[0][0][0][0]);

    workload->Execute();
    workloadRef->Execute();

    CopyDataFromITensorHandle(&comparisonResult.output[0][0][0][0], outputHandle.get());
    CopyDataFromITensorHandle(&comparisonResult.outputExpected[0][0][0][0], outputHandleRef.get());

    return comparisonResult;
}

LayerTestResult<float,4> CompareBatchNormTest(armnn::IWorkloadFactory& workloadFactory,
                                     armnn::IWorkloadFactory& refWorkloadFactory)
{
    const unsigned int width     = 2;
    const unsigned int height    = 3;
    const unsigned int channels  = 5;
    const unsigned int batchSize = 3;

    armnn::TensorInfo inputTensorInfo;
    armnn::TensorInfo outputTensorInfo;
    armnn::TensorInfo tensorInfo;

    constexpr unsigned int shape[]       = {batchSize, channels, height, width};
    constexpr unsigned int tensorShape[] = {channels};

    inputTensorInfo = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    outputTensorInfo = armnn::TensorInfo(4, shape, armnn::DataType::Float32);
    tensorInfo = armnn::TensorInfo(1, tensorShape, armnn::DataType::Float32);

    auto input = MakeRandomTensor<float, 4>(inputTensorInfo, 21312);

    auto mean     = MakeRandomTensor<float, 1>(tensorInfo, 123);
    auto variance = MakeRandomTensor<float, 1>(tensorInfo, 234, 0.0f);
    auto beta     = MakeRandomTensor<float, 1>(tensorInfo, 123);
    auto gamma    = MakeRandomTensor<float, 1>(tensorInfo, 345);

    LayerTestResult<float,4> ret(outputTensorInfo);

    std::unique_ptr<armnn::ITensorHandle> inputHandle  = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    std::unique_ptr<armnn::ITensorHandle> inputHandleRef  = refWorkloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandleRef = refWorkloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::BatchNormalizationQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle meanTensor(tensorInfo);
    armnn::ScopedCpuTensorHandle varianceTensor(tensorInfo);
    armnn::ScopedCpuTensorHandle betaTensor(tensorInfo);
    armnn::ScopedCpuTensorHandle gammaTensor(tensorInfo);

    AllocateAndCopyDataToITensorHandle(&meanTensor, &mean[0]);
    AllocateAndCopyDataToITensorHandle(&varianceTensor, &variance[0]);
    AllocateAndCopyDataToITensorHandle(&betaTensor, &beta[0]);
    AllocateAndCopyDataToITensorHandle(&gammaTensor, &gamma[0]);

    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());
    data.m_Mean             = &meanTensor;
    data.m_Variance         = &varianceTensor;
    data.m_Beta             = &betaTensor;
    data.m_Gamma            = &gammaTensor;
    data.m_Parameters.m_Eps = 0.01f;

    armnn::BatchNormalizationQueueDescriptor refData = data;
    armnn::WorkloadInfo refInfo = info;
    SetWorkloadInput(refData, refInfo, 0, inputTensorInfo, inputHandleRef.get());
    SetWorkloadOutput(refData, refInfo, 0, outputTensorInfo, outputHandleRef.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateBatchNormalization(data, info);
    std::unique_ptr<armnn::IWorkload> workloadRef = refWorkloadFactory.CreateBatchNormalization(refData, refInfo);

    inputHandle->Allocate();
    outputHandle->Allocate();
    inputHandleRef->Allocate();
    outputHandleRef->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);
    CopyDataToITensorHandle(inputHandleRef.get(), &input[0][0][0][0]);

    workload->Execute();
    workloadRef->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());
    CopyDataFromITensorHandle(&ret.outputExpected[0][0][0][0], outputHandleRef.get());

    return ret;
}

void Concatenate(armnn::IWorkloadFactory& workloadFactory,
    std::initializer_list<const armnn::TensorInfo> inputTensorInfos,
    std::initializer_list<void*> inputs,
    const armnn::TensorInfo& outputTensorInfo,
    void* output,
    unsigned int concatDim)
{
    armnn::MergerQueueDescriptor queueDescriptor;

    std::vector<armnn::TensorShape> shapes;
    shapes.reserve(inputTensorInfos.size());
    for (const armnn::TensorInfo& it: inputTensorInfos)
    {
        shapes.push_back(it.GetShape());
    }
    armnn::OriginsDescriptor viewsDescriptor = armnn::CreateMergerDescriptorForConcatenation(shapes.begin(),
        shapes.end(), concatDim);

    queueDescriptor.m_ViewOrigins.reserve(viewsDescriptor.GetNumViews());
    for (unsigned int i = 0; i < viewsDescriptor.GetNumViews(); ++i)
    {
        queueDescriptor.m_ViewOrigins.emplace_back(std::vector<unsigned int>(viewsDescriptor.GetViewOrigin(i),
            viewsDescriptor.GetViewOrigin(i) + viewsDescriptor.GetNumDimensions()));
    }

    const size_t inputCount = inputTensorInfos.size();

    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    std::vector<std::unique_ptr<armnn::ITensorHandle>> inputHandles;
    inputHandles.reserve(inputCount);

    const bool subTensorsSupported = workloadFactory.SupportsSubTensors();
    for (unsigned int i = 0; i < inputCount; ++i)
    {
        const armnn::TensorInfo& inputTensorInfo = inputTensorInfos.begin()[i];

        std::unique_ptr<armnn::ITensorHandle> inputHandle = subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo.GetShape(),
                queueDescriptor.m_ViewOrigins[i].m_Origin.data())
            : workloadFactory.CreateTensorHandle(inputTensorInfo);

        inputHandles.emplace_back(std::move(inputHandle));
    }

    armnn::WorkloadInfo workloadInfo;

    for (unsigned int i = 0; i < inputCount; ++i)
    {
        AddInputToWorkload(queueDescriptor, workloadInfo, inputTensorInfos.begin()[i], inputHandles[i].get());
    }

    AddOutputToWorkload(queueDescriptor, workloadInfo, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateMerger(queueDescriptor, workloadInfo);

    for (auto& inputHandle : inputHandles)
    {
        inputHandle->Allocate();
    }

    outputHandle->Allocate();

    unsigned int nextInputId = 0;
    for (auto& inputHandle : inputHandles)
    {
        CopyDataToITensorHandle(inputHandle.get(), *(inputs.begin() + nextInputId++));
    }

    workload->Execute();

    CopyDataFromITensorHandle(output, outputHandle.get());
}

template <typename T>
LayerTestResult<T, 1> Concatenation1dTestImpl(armnn::IWorkloadFactory& workloadFactory, float qScale, int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo({ 3 }, armnn::GetDataType<T>());

    auto input0 = MakeTensor<T, 1>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, { 1.0f, 2.0f, 3.0f }));
    auto input1 = MakeTensor<T, 1>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, { 4.0f, 5.0f, 6.0f }));
    auto input2 = MakeTensor<T, 1>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, { 7.0f, 8.0f, 9.0f }));

    armnn::TensorInfo outputTensorInfo({ 9 }, armnn::GetDataType<T>());

    LayerTestResult<T, 1> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { inputTensorInfo, inputTensorInfo, inputTensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        0);

    result.output = MakeTensor<T, 1>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 1>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f
    }));

    return result;
}

LayerTestResult<float, 1> Concatenation1dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation1dTestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 2> Concatenation2dTestImpl(armnn::IWorkloadFactory& workloadFactory,
    const armnn::TensorInfo& outputTensorInfo,
    unsigned int dimension,
    const float qScale,
    const int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo({ 2, 3 }, armnn::GetDataType<T>());

    auto input0 = MakeTensor<T, 2>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,
    }));

    auto input1 = MakeTensor<T, 2>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        4.0f, 5.0f, 6.0f,

        // Batch 1
        13.0f, 14.0f, 15.0f,
    }));

    auto input2 = MakeTensor<T, 2>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        7.0f, 8.0f, 9.0f,

        // Batch 1
        16.0f, 17.0f, 18.0f,
    }));

    LayerTestResult<T, 2> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { inputTensorInfo, inputTensorInfo, inputTensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        dimension);

    result.output = MakeTensor<T, 2>(outputTensorInfo, output);
    return result;
}

template <typename T>
LayerTestResult<T, 2> Concatenation2dDim0TestImpl(armnn::IWorkloadFactory& workloadFactory,
    float qScale, int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 6, 3 }, armnn::GetDataType<T>());

    LayerTestResult<T, 2> result = Concatenation2dTestImpl<T>(workloadFactory, outputTensorInfo, 0, qScale, qOffset);
    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,

        // Batch 2
        4.0f, 5.0f, 6.0f,

        // Batch 3
        13.0f, 14.0f, 15.0f,

        // Batch 4
        7.0f, 8.0f, 9.0f,

        // Batch 5
        16.0f, 17.0f, 18.0f,
    }));

    return result;
}

LayerTestResult<float, 2> Concatenation2dDim0Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim0TestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 2> Concatenation2dDim1TestImpl(armnn::IWorkloadFactory& workloadFactory,
    float qScale, int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 2, 9 }, armnn::GetDataType<T>());

    LayerTestResult<T, 2> result = Concatenation2dTestImpl<T>(workloadFactory, outputTensorInfo, 1, qScale, qOffset);
    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f
    }));

    return result;
}

LayerTestResult<float, 2> Concatenation2dDim1Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim1TestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 2> Concatenation2dDim0DiffInputDimsTestImpl(armnn::IWorkloadFactory& workloadFactory, float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3 }, armnn::GetDataType<T>());
    auto input0 = MakeTensor<T, 2>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,
    }));

    armnn::TensorInfo input1TensorInfo({ 3, 3 }, armnn::GetDataType<T>());
    auto input1 = MakeTensor<T, 2>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        4.0f, 5.0f, 6.0f,

        // Batch 1
        13.0f, 14.0f, 15.0f,

        // Batch 0
        7.0f, 8.0f, 9.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 1, 3 }, armnn::GetDataType<T>());
    auto input2 = MakeTensor<T, 2>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 1
        16.0f, 17.0f, 18.0f,
    }));

    armnn::TensorInfo outputTensorInfo({ 6, 3 }, armnn::GetDataType<T>());
    LayerTestResult<T, 2> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { input0TensorInfo, input1TensorInfo, input2TensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        0);

    result.output = MakeTensor<T, 2>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,

        // Batch 2
        4.0f, 5.0f, 6.0f,

        // Batch 3
        13.0f, 14.0f, 15.0f,

        // Batch 4
        7.0f, 8.0f, 9.0f,

        // Batch 5
        16.0f, 17.0f, 18.0f,
    }));

    return result;
}

LayerTestResult<float, 2> Concatenation2dDim0DiffInputDimsTest(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim0DiffInputDimsTestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 2> Concatenation2dDim1DiffInputDimsTestImpl(armnn::IWorkloadFactory& workloadFactory, float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3 }, armnn::GetDataType<T>());
    auto input0 = MakeTensor<T, 2>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,
    }));

    armnn::TensorInfo input1TensorInfo({ 2, 5 }, armnn::GetDataType<T>());
    auto input1 = MakeTensor<T, 2>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        4.0f, 5.0f, 6.0f, 7.0f, 8.0f,

        // Batch 1
        13.0f, 14.0f, 15.0f, 16.0f, 17.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 2, 1 }, armnn::GetDataType<T>());
    auto input2 = MakeTensor<T, 2>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        9.0f,

        // Batch 1
        18.0f
    }));

    armnn::TensorInfo outputTensorInfo({ 2, 9 }, armnn::GetDataType<T>());
    LayerTestResult<T, 2> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { input0TensorInfo, input1TensorInfo, input2TensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        1);

    result.output = MakeTensor<T, 2>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f,
    }));

    return result;
}

LayerTestResult<float, 2> Concatenation2dDim1DiffInputDimsTest(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim1DiffInputDimsTestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 3> Concatenation3dTestImpl(armnn::IWorkloadFactory& workloadFactory,
    const armnn::TensorInfo& outputTensorInfo,
    unsigned int dimension,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo({ 2, 3, 2 }, armnn::GetDataType<T>());

    auto input0 = MakeTensor<T, 3>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f
    }));

    auto input1 = MakeTensor<T, 3>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        7.0f, 8.0f,

        // Batch 0, Channel 1
        9.0f, 10.0f,

        // Batch 0, Channel 2
        11.0f, 12.0f,

        // Batch 1, Channel 0
        25.0f, 26.0f,

        // Batch 1, Channel 1
        27.0f, 28.0f,

        // Batch 1, Channel 2
        29.0f, 30.0f
    }));

    auto input2 = MakeTensor<T, 3>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        13.0f, 14.0f,

        // Batch 0, Channel 1
        15.0f, 16.0f,

        // Batch 0, Channel 2
        17.0f, 18.0f,

        // Batch 1, Channel 0
        31.0f, 32.0f,

        // Batch 1, Channel 1
        33.0f, 34.0f,

        // Batch 1, Channel 2
        35.0f, 36.0f
    }));

    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { inputTensorInfo, inputTensorInfo, inputTensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        dimension);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    return result;
}

template <typename T>
LayerTestResult<T, 3> Concatenation3dDim0TestImpl(armnn::IWorkloadFactory& workloadFactory, float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 6, 3, 2 }, armnn::GetDataType<T>());

    LayerTestResult<T, 3> result = Concatenation3dTestImpl<T>(workloadFactory, outputTensorInfo, 0,
        qScale, qOffset);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 2, Channel 0
        7.0f, 8.0f,

        // Batch 2, Channel 1
        9.0f, 10.0f,

        // Batch 2, Channel 2
        11.0f, 12.0f,

        // Batch 3, Channel 0
        25.0f, 26.0f,

        // Batch 3, Channel 1
        27.0f, 28.0f,

        // Batch 3, Channel 2
        29.0f, 30.0f,

        // Batch 4, Channel 0
        13.0f, 14.0f,

        // Batch 4, Channel 1
        15.0f, 16.0f,

        // Batch 4, Channel 2
        17.0f, 18.0f,

        // Batch 5, Channel 0
        31.0f, 32.0f,

        // Batch 5, Channel 1
        33.0f, 34.0f,

        // Batch 5, Channel 2
        35.0f, 36.0f
    }));
    return result;
}

LayerTestResult<float, 3> Concatenation3dDim0Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim0TestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 3> Concatenation3dDim1TestImpl(armnn::IWorkloadFactory& workloadFactory,
    float qScale, int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 2, 9, 2 }, armnn::GetDataType<T>());

    LayerTestResult<T, 3> result = Concatenation3dTestImpl<T>(workloadFactory, outputTensorInfo, 1, qScale, qOffset);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 0, Channel 3
        7.0f, 8.0f,

        // Batch 0, Channel 4
        9.0f, 10.0f,

        // Batch 0, Channel 5
        11.0f, 12.0f,

        // Batch 0, Channel 6
        13.0f, 14.0f,

        // Batch 0, Channel 7
        15.0f, 16.0f,

        // Batch 0, Channel 8
        17.0f, 18.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 1, Channel 3
        25.0f, 26.0f,

        // Batch 1, Channel 4
        27.0f, 28.0f,

        // Batch 1, Channel 5
        29.0f, 30.0f,

        // Batch 1, Channel 6
        31.0f, 32.0f,

        // Batch 1, Channel 7
        33.0f, 34.0f,

        // Batch 1, Channel 8
        35.0f, 36.0f
    }));

    return result;
}

LayerTestResult<float, 3> Concatenation3dDim1Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim1TestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 3> Concatenation3dDim2TestImpl(armnn::IWorkloadFactory& workloadFactory,
    float qScale, int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 2, 3, 6 }, armnn::GetDataType<T>());

    LayerTestResult<T, 3> result = Concatenation3dTestImpl<T>(workloadFactory, outputTensorInfo, 2, qScale, qOffset);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f, 7.0f, 8.0f, 13.0f, 14.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f, 9.0f, 10.0f, 15.0f, 16.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f, 11.0f, 12.0f, 17.0f, 18.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f, 25.0f, 26.0f, 31.0f, 32.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f, 27.0f, 28.0f, 33.0f, 34.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f, 29.0f, 30.0f, 35.0f, 36.0f,
    }));

    return result;
}

LayerTestResult<float, 3> Concatenation3dDim2Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim2TestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 3> Concatenation3dDim0DiffInputDimsTestImpl(armnn::IWorkloadFactory& workloadFactory, float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3, 2 }, armnn::GetDataType<T>());
    auto input0 = MakeTensor<T, 3>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
            // Batch 0, Channel 0
            1.0f, 2.0f,

            // Batch 0, Channel 1
            3.0f, 4.0f,

            // Batch 0, Channel 2
            5.0f, 6.0f,

            // Batch 1, Channel 0
            19.0f, 20.0f,

            // Batch 1, Channel 1
            21.0f, 22.0f,

            // Batch 1, Channel 2
            23.0f, 24.0f
    }));

    armnn::TensorInfo input1TensorInfo({ 1, 3, 2 }, armnn::GetDataType<T>());
    auto input1 = MakeTensor<T, 3>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
            // Batch 0, Channel 0
            7.0f, 8.0f,

            // Batch 0, Channel 1
            9.0f, 10.0f,

            // Batch 0, Channel 2
            11.0f, 12.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 3, 3, 2 }, armnn::GetDataType<T>());
    auto input2 = MakeTensor<T, 3>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
            // Batch 0, Channel 0
            25.0f, 26.0f,

            // Batch 0, Channel 1
            27.0f, 28.0f,

            // Batch 0, Channel 2
            29.0f, 30.0f,

            // Batch 1, Channel 0
            13.0f, 14.0f,

            // Batch 1, Channel 1
            15.0f, 16.0f,

            // Batch 1, Channel 2
            17.0f, 18.0f,

            // Batch 2, Channel 0
            31.0f, 32.0f,

            // Batch 2, Channel 1
            33.0f, 34.0f,

            // Batch 2, Channel 2
            35.0f, 36.0f
    }));

    armnn::TensorInfo outputTensorInfo({ 6, 3, 2 }, armnn::GetDataType<T>());
    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { input0TensorInfo, input1TensorInfo, input2TensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        0);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 2, Channel 0
        7.0f, 8.0f,

        // Batch 2, Channel 1
        9.0f, 10.0f,

        // Batch 2, Channel 2
        11.0f, 12.0f,

        // Batch 3, Channel 0
        25.0f, 26.0f,

        // Batch 3, Channel 1
        27.0f, 28.0f,

        // Batch 3, Channel 2
        29.0f, 30.0f,

        // Batch 4, Channel 0
        13.0f, 14.0f,

        // Batch 4, Channel 1
        15.0f, 16.0f,

        // Batch 4, Channel 2
        17.0f, 18.0f,

        // Batch 5, Channel 0
        31.0f, 32.0f,

        // Batch 5, Channel 1
        33.0f, 34.0f,

        // Batch 5, Channel 2
        35.0f, 36.0f
    }));

    return result;
}

LayerTestResult<float, 3> Concatenation3dDim0DiffInputDimsTest(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim0DiffInputDimsTestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 3> Concatenation3dDim1DiffInputDimsTestImpl(armnn::IWorkloadFactory& workloadFactory, float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3, 2 }, armnn::GetDataType<T>());
    auto input0 = MakeTensor<T, 3>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f
    }));

    armnn::TensorInfo input1TensorInfo({ 2, 4, 2 }, armnn::GetDataType<T>());
    auto input1 = MakeTensor<T, 3>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        7.0f, 8.0f,

        // Batch 0, Channel 1
        9.0f, 10.0f,

        // Batch 0, Channel 2
        11.0f, 12.0f,

        // Batch 0, Channel 3
        25.0f, 26.0f,

        // Batch 1, Channel 0
        27.0f, 28.0f,

        // Batch 1, Channel 1
        29.0f, 30.0f,

        // Batch 1, Channel 2
        13.0f, 14.0f,

        // Batch 1, Channel 3
        15.0f, 16.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 2, 1, 2 }, armnn::GetDataType<T>());
    auto input2 = MakeTensor<T, 3>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        17.0f, 18.0f,

        // Batch 1, Channel 0
        31.0f, 32.0f,
    }));

    armnn::TensorInfo outputTensorInfo({ 2, 8, 2 }, armnn::GetDataType<T>());
    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { input0TensorInfo, input1TensorInfo, input2TensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        1);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 0, Channel 3
        7.0f, 8.0f,

        // Batch 0, Channel 4
        9.0f, 10.0f,

        // Batch 0, Channel 5
        11.0f, 12.0f,

        // Batch 0, Channel 6
        25.0f, 26.0f,

        // Batch 0, Channel 7
        17.0f, 18.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 1, Channel 3
        27.0f, 28.0f,

        // Batch 1, Channel 4
        29.0f, 30.0f,

        // Batch 1, Channel 5
        13.0f, 14.0f,

        // Batch 1, Channel 6
        15.0f, 16.0f,

        // Batch 1, Channel 7
        31.0f, 32.0f,
    }));

    return result;
}

LayerTestResult<float, 3> Concatenation3dDim1DiffInputDimsTest(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim1DiffInputDimsTestImpl<float>(workloadFactory, 0.0f, 0);
}

template <typename T>
LayerTestResult<T, 3> Concatenation3dDim2DiffInputDimsTestImpl(armnn::IWorkloadFactory& workloadFactory, float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3, 2 }, armnn::GetDataType<T>());
    auto input0 = MakeTensor<T, 3>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f
    }));

    armnn::TensorInfo input1TensorInfo({ 2, 3, 1 }, armnn::GetDataType<T>());
    auto input1 = MakeTensor<T, 3>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        7.0f,

        // Batch 0, Channel 1
        9.0f,

        // Batch 0, Channel 2
        11.0f,

        // Batch 1, Channel 0
        25.0f,

        // Batch 1, Channel 1
        27.0f,

        // Batch 1, Channel 2
        29.0f
    }));

    armnn::TensorInfo input2TensorInfo({ 2, 3, 3 }, armnn::GetDataType<T>());
    auto input2 = MakeTensor<T, 3>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        13.0f, 14.0f, 50.0f,

        // Batch 0, Channel 1
        15.0f, 16.0f, 51.0f,

        // Batch 0, Channel 2
        17.0f, 18.0f, 52.0f,

        // Batch 1, Channel 0
        31.0f, 32.0f, 53.0f,

        // Batch 1, Channel 1
        33.0f, 34.0f, 54.0f,

        // Batch 1, Channel 2
        35.0f, 36.0f, 55.0f,
    }));

    armnn::TensorInfo outputTensorInfo({ 2, 3, 6 }, armnn::GetDataType<T>());
    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate(workloadFactory,
        { input0TensorInfo, input1TensorInfo, input2TensorInfo },
        { input0.data(), input1.data(), input2.data() },
        outputTensorInfo,
        output.data(),
        2);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f, 7.0f, 13.0f, 14.0f, 50.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f, 9.0f, 15.0f, 16.0f, 51.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f, 11.0f, 17.0f, 18.0f, 52.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f, 25.0f, 31.0f, 32.0f, 53.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f, 27.0f, 33.0f, 34.0f, 54.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f, 29.0f, 35.0f, 36.0f, 55.0f,
    }));

    return result;
}

LayerTestResult<float, 3> Concatenation3dDim2DiffInputDimsTest(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim2DiffInputDimsTestImpl<float>(workloadFactory, 0.0f, 0);
}

LayerTestResult<float, 4> ResizeBilinearNopTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 4;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        1.0f, 2.0f, 3.0f, 4.0f,
        2.0f, 3.0f, 4.0f, 5.0f,
        3.0f, 4.0f, 5.0f, 6.0f,
        4.0f, 5.0f, 6.0f, 7.0f
    }));

    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = input;

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> SimpleResizeBilinearTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 2;
    constexpr unsigned int inputHeight = 2;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth / 2;
    constexpr unsigned int outputHeight = inputHeight / 2;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        1.0f, 255.0f,
        200.0f, 250.f,
    }));

    // The 'resize bilinear' operation projects the top-left corner of output texels into the input image,
    // then figures out the interpolants and weights. Note this is different to projecting the centre of the
    // output texel - and thus we'll expect the output 1x1 matrix to contain as its single element the value
    // that was at position (0,0) of the input matrix (rather than an average, which we would expect if projecting
    // the centre).
    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(outputTensorInfo, std::vector<float>({
        1.0f
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> ResizeBilinearSqMinTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 4;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth / 2;
    constexpr unsigned int outputHeight = inputHeight / 2;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        1.0f, 2.0f, 3.0f, 4.0f,
        2.0f, 3.0f, 4.0f, 5.0f,
        3.0f, 4.0f, 5.0f, 6.0f,
        4.0f, 5.0f, 6.0f, 7.0f
    }));

    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(outputTensorInfo, std::vector<float>({
        1.f, 3.f,
        3.f, 5.f
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> ResizeBilinearMinTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 5;
    constexpr unsigned int inputHeight = 3;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = 3;
    constexpr unsigned int outputHeight = 2;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
          1.0f,   2.0f,   3.0f,   5.0f,   8.0f,
         13.0f,  21.0f,  34.0f,  55.0f,  89.0f,
        144.0f, 233.0f, 377.0f, 610.0f, 987.0f
    }));

    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(outputTensorInfo, std::vector<float>({
        1.0f, 2.6666f, 6.0f,
        78.5f, 179.3333f, 401.f
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> ResizeBilinearMagTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 2;
    constexpr unsigned int inputHeight = 3;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = 5;
    constexpr unsigned int outputHeight = 3;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
          1.0f,   2.0f,
         13.0f,  21.0f,
        144.0f, 233.0f
    }));

    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(outputTensorInfo, std::vector<float>({
         1.0f,   1.4f,   1.8f,   2.f,   2.f,
         13.f,  16.2f,  19.4f,  21.f,  21.f,
        144.f, 179.6f, 215.2f, 233.f, 233.f
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 2> FakeQuantizationTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int width = 2;
    constexpr unsigned int height = 3;

    const armnn::TensorInfo tensorInfo({height, width },
        armnn::DataType::Float32);
    auto input = MakeTensor<float, 2>(tensorInfo, std::vector<float>({
       -10.0f,  -5.0f,
         0.0f,   5.0f,
        10.0f,  10.0f
    }));

    LayerTestResult<float, 2> ret(tensorInfo);

    std::unique_ptr<armnn::ITensorHandle> inputHandle  = workloadFactory.CreateTensorHandle(tensorInfo);

    std::unique_ptr<armnn::ITensorHandle> outputHandle  = workloadFactory.CreateTensorHandle(tensorInfo);

    armnn::FakeQuantizationQueueDescriptor data;
    armnn::WorkloadInfo info;

    AddInputToWorkload(data, info, tensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, tensorInfo, outputHandle.get());
    float min = -10.f;
    float max = 10.f;

    data.m_Parameters.m_Min = min;
    data.m_Parameters.m_Max = max;

    armnn::PassthroughCpuTensorHandle refHandle(tensorInfo, &ret.outputExpected[0][0]);
    armnn::FakeQuantizationQueueDescriptor refData = data;
    armnn::WorkloadInfo refInfo = info;
    SetWorkloadOutput(refData, refInfo, 0, tensorInfo, &refHandle);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateFakeQuantization(data, info);

    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0], outputHandle.get());

    ret.outputExpected = MakeTensor<float, 2>(tensorInfo, std::vector<float>({
        0.0f,     63.0f,
        128.0f,   191.0f,
        255.0f,   255.0f
    }));
    return ret;
}

LayerTestResult<float, 4> L2Normalization1dTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 1;
    constexpr unsigned int inputHeight = 1;
    constexpr unsigned int inputChannels = 10;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f
    }));

    const float approxInvL2Norm = 0.050964719f;
    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
         1.0f * approxInvL2Norm,
         2.0f * approxInvL2Norm,
         3.0f * approxInvL2Norm,
         4.0f * approxInvL2Norm,
         5.0f * approxInvL2Norm,
         6.0f * approxInvL2Norm,
         7.0f * approxInvL2Norm,
         8.0f * approxInvL2Norm,
         9.0f * approxInvL2Norm,
        10.0f * approxInvL2Norm
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::L2NormalizationQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateL2Normalization(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

namespace
{

float CalcInvL2Norm(std::initializer_list<float> elements)
{
    const float reduction = std::accumulate(elements.begin(), elements.end(), 0.0f,
        [](float acc, float element) { return acc + element * element; });
    return 1.0f / sqrtf(reduction);
}

}

LayerTestResult<float, 4> L2Normalization2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 5;
    constexpr unsigned int inputHeight = 1;
    constexpr unsigned int inputChannels = 2;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        1.0f, 3.0f, 5.0f, 7.0f,  9.0f,
        2.0f, 4.0f, 6.0f, 8.0f, 10.0f
    }));

    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
         1.0f * CalcInvL2Norm({ 1.0f, 2.0f }),
         3.0f * CalcInvL2Norm({ 3.0f, 4.0f }),
         5.0f * CalcInvL2Norm({ 5.0f, 6.0f }),
         7.0f * CalcInvL2Norm({ 7.0f, 8.0f }),
         9.0f * CalcInvL2Norm({ 9.0f, 10.0f }),

         2.0f * CalcInvL2Norm({ 1.0f, 2.0f }),
         4.0f * CalcInvL2Norm({ 3.0f, 4.0f }),
         6.0f * CalcInvL2Norm({ 5.0f, 6.0f }),
         8.0f * CalcInvL2Norm({ 7.0f, 8.0f }),
        10.0f * CalcInvL2Norm({ 9.0f, 10.0f })
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::L2NormalizationQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateL2Normalization(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> L2Normalization3dTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 3;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 2;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        // Channel 0
        119.0f,  21.0f, 150.0f,
        149.0f,  32.0f, 179.0f,
         15.0f, 227.0f, 141.0f,
        147.0f, 199.0f, 220.0f,

        // Channel 1
        110.0f, 140.0f,  73.0f,
        211.0f, 212.0f,  89.0f,
         24.0f, 138.0f, 188.0f,
        162.0f,  12.0f, 161.0f,
    }));

    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        119.0f * CalcInvL2Norm({ 119.0f, 110.0f }),
         21.0f * CalcInvL2Norm({  21.0f, 140.0f }),
        150.0f * CalcInvL2Norm({ 150.0f,  73.0f }),
        149.0f * CalcInvL2Norm({ 149.0f, 211.0f }),
         32.0f * CalcInvL2Norm({  32.0f, 212.0f }),
        179.0f * CalcInvL2Norm({ 179.0f,  89.0f }),
         15.0f * CalcInvL2Norm({  15.0f,  24.0f }),
        227.0f * CalcInvL2Norm({ 227.0f, 138.0f }),
        141.0f * CalcInvL2Norm({ 141.0f, 188.0f }),
        147.0f * CalcInvL2Norm({ 147.0f, 162.0f }),
        199.0f * CalcInvL2Norm({ 199.0f,  12.0f }),
        220.0f * CalcInvL2Norm({ 220.0f, 161.0f }),

        110.0f * CalcInvL2Norm({ 119.0f, 110.0f }),
        140.0f * CalcInvL2Norm({  21.0f, 140.0f }),
         73.0f * CalcInvL2Norm({ 150.0f,  73.0f }),
        211.0f * CalcInvL2Norm({ 149.0f, 211.0f }),
        212.0f * CalcInvL2Norm({  32.0f, 212.0f }),
         89.0f * CalcInvL2Norm({ 179.0f,  89.0f }),
         24.0f * CalcInvL2Norm({  15.0f,  24.0f }),
        138.0f * CalcInvL2Norm({ 227.0f, 138.0f }),
        188.0f * CalcInvL2Norm({ 141.0f, 188.0f }),
        162.0f * CalcInvL2Norm({ 147.0f, 162.0f }),
         12.0f * CalcInvL2Norm({ 199.0f,  12.0f }),
        161.0f * CalcInvL2Norm({ 220.0f, 161.0f }),
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::L2NormalizationQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateL2Normalization(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> L2Normalization4dTest(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 3;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 3;
    constexpr unsigned int inputBatchSize = 2;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    const armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::Float32);
    const armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::Float32);

    auto input = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({
        // Batch 0, Channel 0
        235.0f,  46.0f, 178.0f,
        100.0f, 123.0f,  19.0f,
        172.0f,  74.0f, 250.0f,
          6.0f, 195.0f,  80.0f,

        // Batch 0, Channel 1
        113.0f,  95.0f, 202.0f,
         77.0f, 114.0f,  71.0f,
        122.0f, 246.0f, 166.0f,
         82.0f,  28.0f,  37.0f,

        // Batch 0, Channel 2
         56.0f, 170.0f, 162.0f,
        194.0f,  89.0f, 254.0f,
         12.0f, 209.0f, 200.0f,
          1.0f,  64.0f,  54.0f,

        // Batch 1, Channel 0
         67.0f,  90.0f,  49.0f,
          7.0f, 163.0f,  18.0f,
         25.0f, 117.0f, 103.0f,
        247.0f,  59.0f, 189.0f,

        // Batch 1, Channel 1
        239.0f, 104.0f, 199.0f,
         17.0f, 124.0f, 153.0f,
        222.0f, 217.0f, 75.0f,
         32.0f, 126.0f, 21.0f,

        // Batch 1, Channel 2
         97.0f, 145.0f, 215.0f,
        115.0f, 116.0f, 238.0f,
        226.0f,  16.0f, 132.0f,
         92.0f, 125.0f,  88.0f,
    }));

    LayerTestResult<float, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<float, 4>(inputTensorInfo, std::vector<float>({

        // Batch 0, Channel 0
        235.0f * CalcInvL2Norm({ 235.0f, 113.0f,  56.0f }),
         46.0f * CalcInvL2Norm({  46.0f,  95.0f, 170.0f }),
        178.0f * CalcInvL2Norm({ 178.0f, 202.0F, 162.0f }),
        100.0f * CalcInvL2Norm({ 100.0f,  77.0f, 194.0f }),
        123.0f * CalcInvL2Norm({ 123.0f, 114.0f,  89.0f }),
         19.0f * CalcInvL2Norm({  19.0f,  71.0f, 254.0f }),
        172.0f * CalcInvL2Norm({ 172.0f, 122.0f,  12.0f }),
         74.0f * CalcInvL2Norm({  74.0f, 246.0f, 209.0f }),
        250.0f * CalcInvL2Norm({ 250.0f, 166.0f, 200.0f }),
          6.0f * CalcInvL2Norm({   6.0f,  82.0f,   1.0f }),
        195.0f * CalcInvL2Norm({ 195.0f,  28.0f,  64.0f }),
         80.0f * CalcInvL2Norm({  80.0f,  37.0f,  54.0f }),

        // Batch 0, Channel 1
        113.0f * CalcInvL2Norm({ 235.0f, 113.0f,  56.0f }),
         95.0f * CalcInvL2Norm({  46.0f,  95.0f, 170.0f }),
        202.0f * CalcInvL2Norm({ 178.0f, 202.0F, 162.0f }),
         77.0f * CalcInvL2Norm({ 100.0f,  77.0f, 194.0f }),
        114.0f * CalcInvL2Norm({ 123.0f, 114.0f,  89.0f }),
         71.0f * CalcInvL2Norm({  19.0f,  71.0f, 254.0f }),
        122.0f * CalcInvL2Norm({ 172.0f, 122.0f,  12.0f }),
        246.0f * CalcInvL2Norm({  74.0f, 246.0f, 209.0f }),
        166.0f * CalcInvL2Norm({ 250.0f, 166.0f, 200.0f }),
         82.0f * CalcInvL2Norm({   6.0f,  82.0f,   1.0f }),
         28.0f * CalcInvL2Norm({ 195.0f,  28.0f,  64.0f }),
         37.0f * CalcInvL2Norm({  80.0f,  37.0f,  54.0f }),

        // Batch 0, Channel 2
         56.0f * CalcInvL2Norm({ 235.0f, 113.0f,  56.0f }),
        170.0f * CalcInvL2Norm({  46.0f,  95.0f, 170.0f }),
        162.0f * CalcInvL2Norm({ 178.0f, 202.0F, 162.0f }),
        194.0f * CalcInvL2Norm({ 100.0f,  77.0f, 194.0f }),
         89.0f * CalcInvL2Norm({ 123.0f, 114.0f,  89.0f }),
        254.0f * CalcInvL2Norm({  19.0f,  71.0f, 254.0f }),
         12.0f * CalcInvL2Norm({ 172.0f, 122.0f,  12.0f }),
        209.0f * CalcInvL2Norm({  74.0f, 246.0f, 209.0f }),
        200.0f * CalcInvL2Norm({ 250.0f, 166.0f, 200.0f }),
          1.0f * CalcInvL2Norm({   6.0f,  82.0f,   1.0f }),
         64.0f * CalcInvL2Norm({ 195.0f,  28.0f,  64.0f }),
         54.0f * CalcInvL2Norm({  80.0f,  37.0f,  54.0f }),

        // Batch 1, Channel 0
         67.0f * CalcInvL2Norm({  67.0f, 239.0f,  97.0f }),
         90.0f * CalcInvL2Norm({  90.0f, 104.0f, 145.0f }),
         49.0f * CalcInvL2Norm({  49.0f, 199.0f, 215.0f }),
          7.0f * CalcInvL2Norm({   7.0f,  17.0f, 115.0f }),
        163.0f * CalcInvL2Norm({ 163.0f, 124.0f, 116.0f }),
         18.0f * CalcInvL2Norm({  18.0f, 153.0f, 238.0f }),
         25.0f * CalcInvL2Norm({  25.0f, 222.0f, 226.0f }),
        117.0f * CalcInvL2Norm({ 117.0f, 217.0f,  16.0f }),
        103.0f * CalcInvL2Norm({ 103.0f,  75.0f, 132.0f }),
        247.0f * CalcInvL2Norm({ 247.0f,  32.0f,  92.0f }),
         59.0f * CalcInvL2Norm({  59.0f, 126.0f, 125.0f }),
        189.0f * CalcInvL2Norm({ 189.0f,  21.0f,  88.0f }),

        // Batch 1, Channel 1
        239.0f * CalcInvL2Norm({  67.0f, 239.0f,  97.0f }),
        104.0f * CalcInvL2Norm({  90.0f, 104.0f, 145.0f }),
        199.0f * CalcInvL2Norm({  49.0f, 199.0f, 215.0f }),
         17.0f * CalcInvL2Norm({   7.0f,  17.0f, 115.0f }),
        124.0f * CalcInvL2Norm({ 163.0f, 124.0f, 116.0f }),
        153.0f * CalcInvL2Norm({  18.0f, 153.0f, 238.0f }),
        222.0f * CalcInvL2Norm({  25.0f, 222.0f, 226.0f }),
        217.0f * CalcInvL2Norm({ 117.0f, 217.0f,  16.0f }),
         75.0f * CalcInvL2Norm({ 103.0f,  75.0f, 132.0f }),
         32.0f * CalcInvL2Norm({ 247.0f,  32.0f,  92.0f }),
        126.0f * CalcInvL2Norm({  59.0f, 126.0f, 125.0f }),
         21.0f * CalcInvL2Norm({ 189.0f,  21.0f,  88.0f }),

        // Batch 1, Channel 2
         97.0f * CalcInvL2Norm({  67.0f, 239.0f,  97.0f }),
        145.0f * CalcInvL2Norm({  90.0f, 104.0f, 145.0f }),
        215.0f * CalcInvL2Norm({  49.0f, 199.0f, 215.0f }),
        115.0f * CalcInvL2Norm({   7.0f,  17.0f, 115.0f }),
        116.0f * CalcInvL2Norm({ 163.0f, 124.0f, 116.0f }),
        238.0f * CalcInvL2Norm({  18.0f, 153.0f, 238.0f }),
        226.0f * CalcInvL2Norm({  25.0f, 222.0f, 226.0f }),
         16.0f * CalcInvL2Norm({ 117.0f, 217.0f,  16.0f }),
        132.0f * CalcInvL2Norm({ 103.0f,  75.0f, 132.0f }),
         92.0f * CalcInvL2Norm({ 247.0f,  32.0f,  92.0f }),
        125.0f * CalcInvL2Norm({  59.0f, 126.0f, 125.0f }),
         88.0f * CalcInvL2Norm({ 189.0f,  21.0f,  88.0f }),
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::L2NormalizationQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateL2Normalization(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

template <typename T>
LayerTestResult<T, 4> ConstantTestImpl(armnn::IWorkloadFactory& workloadFactory,
    float qScale,
    int32_t qOffset)
{
    constexpr unsigned int inputWidth = 3;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 3;
    constexpr unsigned int inputBatchSize = 2;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::GetDataType<T>());

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::GetDataType<T>());

    // Set quantization parameters if the requested type is a quantized type.
    if(armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
    }

    auto input = MakeTensor<T, 4>(inputTensorInfo, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        235.0f,  46.0f, 178.0f,
        100.0f, 123.0f,  19.0f,
        172.0f,  74.0f, 250.0f,
          6.0f, 195.0f,  80.0f,

        // Batch 0, Channel 1
        113.0f,  95.0f, 202.0f,
         77.0f, 114.0f,  71.0f,
        122.0f, 246.0f, 166.0f,
         82.0f,  28.0f,  37.0f,

        // Batch 0, Channel 2
         56.0f, 170.0f, 162.0f,
        194.0f,  89.0f, 254.0f,
         12.0f, 209.0f, 200.0f,
          1.0f,  64.0f,  54.0f,

        // Batch 1, Channel 0
         67.0f,  90.0f,  49.0f,
          7.0f, 163.0f,  18.0f,
         25.0f, 117.0f, 103.0f,
        247.0f,  59.0f, 189.0f,

        // Batch 1, Channel 1
        239.0f, 104.0f, 199.0f,
         17.0f, 124.0f, 153.0f,
        222.0f, 217.0f, 75.0f,
         32.0f, 126.0f, 21.0f,

        // Batch 1, Channel 2
         97.0f, 145.0f, 215.0f,
        115.0f, 116.0f, 238.0f,
        226.0f,  16.0f, 132.0f,
         92.0f, 125.0f,  88.0f,
    })));

    LayerTestResult<T, 4> result(outputTensorInfo);
    result.outputExpected = input;

    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ScopedCpuTensorHandle constantTensor(inputTensorInfo);
    AllocateAndCopyDataToITensorHandle(&constantTensor, &input[0][0][0][0]);

    armnn::ConstantQueueDescriptor descriptor;
    descriptor.m_LayerOutput = &constantTensor;

    armnn::WorkloadInfo info;
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConstant(descriptor, info);

    outputHandle->Allocate();

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> ConstantTest(armnn::IWorkloadFactory& workloadFactory)
{
    return ConstantTestImpl<float>(workloadFactory, 0.0f, 0);
}

LayerTestResult<uint8_t, 4> ConstantTestUint8(armnn::IWorkloadFactory& workloadFactory)
{
    return ConstantTestImpl<uint8_t>(workloadFactory, 1.0f, 0);
}

LayerTestResult<uint8_t, 3> MergerUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    unsigned int outputWidth = 5;
    unsigned int outputHeight = 6;
    unsigned int outputChannels = 3;

    unsigned int inputWidth1 = 2;
    unsigned int inputHeight1 = 2;
    unsigned int inputChannels1 = 3;

    unsigned int inputWidth2 = 2;
    unsigned int inputHeight2 = 4;
    unsigned int inputChannels2 = 3;

    unsigned int inputWidth3 = 3;
    unsigned int inputHeight3 = 6;
    unsigned int inputChannels3 = 2;

    unsigned int inputWidth4 = 3;
    unsigned int inputHeight4 = 6;
    unsigned int inputChannels4 = 1;

    // Define the tensor descriptors
    armnn::TensorInfo outputTensorInfo({ outputChannels, outputHeight, outputWidth }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo1({ inputChannels1, inputHeight1, inputWidth1 }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo2({ inputChannels2, inputHeight2, inputWidth2 }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo3({ inputChannels3, inputHeight3, inputWidth3 }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo4({ inputChannels4, inputHeight4, inputWidth4 }, armnn::DataType::QuantisedAsymm8);

    // Arbitrary scale and offsets. They don't really matter as the merger operator doesn't dequantize/quantize
    const float scale = 0.13497836f;
    const int32_t offset = -7;

    outputTensorInfo.SetQuantizationScale(scale);
    outputTensorInfo.SetQuantizationOffset(offset);
    inputTensorInfo1.SetQuantizationScale(scale);
    inputTensorInfo1.SetQuantizationOffset(offset);
    inputTensorInfo2.SetQuantizationScale(scale);
    inputTensorInfo2.SetQuantizationOffset(offset);
    inputTensorInfo3.SetQuantizationScale(scale);
    inputTensorInfo3.SetQuantizationOffset(offset);
    inputTensorInfo4.SetQuantizationScale(scale);
    inputTensorInfo4.SetQuantizationOffset(offset);

    LayerTestResult<uint8_t, 3> ret(outputTensorInfo);

    ret.outputExpected = MakeTensor<uint8_t, 3>(outputTensorInfo, std::vector<uint8_t>(
    {
        1, 2, 3, 4, 5,
        6, 7, 8, 9, 10,
        11, 12, 13, 14, 15,
        16, 17, 18, 19, 20,
        21, 22, 23, 24, 25,
        26, 27, 28, 29, 30,

        31, 32, 33, 34, 35,
        36, 37, 38, 39, 40,
        41, 42, 43, 44, 45,
        46, 47, 48, 49, 50,
        51, 52, 53, 54, 55,
        56, 57, 58, 59, 60,

        61, 62, 63, 64, 65,
        66, 67, 68, 69, 70,
        71, 72, 73, 74, 75,
        76, 77, 78, 79, 80,
        81, 82, 83, 84, 85,
        86, 87, 88, 89, 90,
    })
    );


    auto input1 = MakeTensor<uint8_t, 3>(inputTensorInfo1, std::vector<uint8_t>(
    {
        1, 2,
        6, 7,

        31, 32,
        36, 37,

        61, 62,
        66, 67,
    })
    );

    auto input2 = MakeTensor<uint8_t, 3>(inputTensorInfo2, std::vector<uint8_t>(
    {
        11, 12,
        16, 17,
        21, 22,
        26, 27,

        41, 42,
        46, 47,
        51, 52,
        56, 57,

        71, 72,
        76, 77,
        81, 82,
        86, 87,
    })
    );

    auto input3 = MakeTensor<uint8_t, 3>(inputTensorInfo3, std::vector<uint8_t>(
    {
        3, 4, 5,
        8, 9, 10,
        13, 14, 15,
        18, 19, 20,
        23, 24, 25,
        28, 29, 30,

        33, 34, 35,
        38, 39, 40,
        43, 44, 45,
        48, 49, 50,
        53, 54, 55,
        58, 59, 60,
    })
    );


    auto input4 = MakeTensor<uint8_t, 3>(inputTensorInfo4, std::vector<uint8_t>(
    {
        63, 64, 65,
        68, 69, 70,
        73, 74, 75,
        78, 79, 80,
        83, 84, 85,
        88, 89, 90,
    })
    );

    std::vector<unsigned int> wOrigin1 = { 0, 0, 0 }; //extent of the window is defined by size of input[0]
    armnn::MergerQueueDescriptor::ViewOrigin window1(wOrigin1);

    std::vector<unsigned int> wOrigin2 = { 0, 2, 0 }; //extent of the window is defined by size of input[1]
    armnn::MergerQueueDescriptor::ViewOrigin window2(wOrigin2);

    std::vector<unsigned int> wOrigin3 = { 0, 0, 2 }; //extent of the window is defined by size of input[2]
    armnn::MergerQueueDescriptor::ViewOrigin window3(wOrigin3);

    std::vector<unsigned int> wOrigin4 = { 2, 0, 2 }; //extent of the window is defined by size of input[3]
    armnn::MergerQueueDescriptor::ViewOrigin window4(wOrigin4);


    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    bool subTensorsSupported = workloadFactory.SupportsSubTensors();

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo1.GetShape(), wOrigin1.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo1);

    std::unique_ptr<armnn::ITensorHandle> inputHandle2 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo2.GetShape(), wOrigin2.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo2);

    std::unique_ptr<armnn::ITensorHandle> inputHandle3 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo3.GetShape(), wOrigin3.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo3);

    std::unique_ptr<armnn::ITensorHandle> inputHandle4 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo4.GetShape(), wOrigin4.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo4);


    armnn::MergerQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddInputToWorkload(data, info, inputTensorInfo3, inputHandle3.get());
    AddInputToWorkload(data, info, inputTensorInfo4, inputHandle4.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_ViewOrigins.push_back(window1);
    data.m_ViewOrigins.push_back(window2);
    data.m_ViewOrigins.push_back(window3);
    data.m_ViewOrigins.push_back(window4);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateMerger(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    inputHandle3->Allocate();
    inputHandle4->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0]);
    CopyDataToITensorHandle(inputHandle3.get(), &input3[0][0][0]);
    CopyDataToITensorHandle(inputHandle4.get(), &input4[0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0], outputHandle.get());

    return ret;
}

LayerTestResult<uint8_t, 4> AdditionUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    unsigned int batchSize = 1;
    unsigned int channels = 2;
    unsigned int height = 2;
    unsigned int width = 3;

    const float scale = 7.0f;
    const int32_t offset = 3;

    armnn::TensorInfo inputTensorInfo1, inputTensorInfo2;
    armnn::TensorInfo outputTensorInfo;

    const unsigned int shape[] = { batchSize, channels, height, width };
    inputTensorInfo1 = armnn::TensorInfo(4, shape, armnn::DataType::QuantisedAsymm8);
    inputTensorInfo1.SetQuantizationScale(scale);
    inputTensorInfo1.SetQuantizationOffset(offset);

    inputTensorInfo2 = armnn::TensorInfo(4, shape, armnn::DataType::QuantisedAsymm8);
    inputTensorInfo2.SetQuantizationScale(scale);
    inputTensorInfo2.SetQuantizationOffset(offset);

    outputTensorInfo = armnn::TensorInfo(4, shape, armnn::DataType::QuantisedAsymm8);
    outputTensorInfo.SetQuantizationScale(scale);
    outputTensorInfo.SetQuantizationOffset(offset);

    // See dequantized values to the right
    auto input1 = MakeTensor<uint8_t, 4>(inputTensorInfo1, std::vector<uint8_t>(
    {
         63,  35,  77,  70,  56, 112, //  420, 224,  518,  469,  371, 763
        203,  28, 252, 168, 245,  91  // 1400, 175, 1743, 1155, 1694, 616
    }));

    // See dequantized values to the right
    auto input2 = MakeTensor<uint8_t, 4>(inputTensorInfo1, std::vector<uint8_t>(
    {
         21,   7, 175, 231, 175, 210, // 126,   28, 1204, 1596, 1204, 1449
        126, 161,  63,  21, 105, 126  // 861, 1106,  420,  126,  714,  861
    }));

    // See dequantized values to the right
    LayerTestResult<uint8_t, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<uint8_t, 4>(outputTensorInfo, std::vector<uint8_t>(
    {
         81,  39, 249, 255, 228, 255, //  546,  252, 1722, 2065(clamped), 1575, 2212(clamped)
        255, 186, 255, 186, 255, 214, // 2261(clamped), 1281, 2163(clamped), 1281, 2408(clamped), 1477
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> inputHandle2 = workloadFactory.CreateTensorHandle(inputTensorInfo2);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::AdditionQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateAddition(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());

    return result;
}

namespace
{
LayerTestResult<uint8_t, 4> MultiplicationUint8TestHelper(armnn::IWorkloadFactory& workloadFactory,
                                                          const unsigned int shape0[4],
                                                          const std::vector<uint8_t> & values0,
                                                          float scale0,
                                                          int32_t offset0,
                                                          const unsigned int shape1[4],
                                                          const std::vector<uint8_t> & values1,
                                                          float scale1,
                                                          int32_t offset1,
                                                          const unsigned int outShape[4],
                                                          const std::vector<uint8_t> & outValues,
                                                          float outScale,
                                                          int32_t outOffset)
{
    armnn::TensorInfo inputTensorInfo0(4, shape0, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo1(4, shape1, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo outputTensorInfo(4, outShape, armnn::DataType::QuantisedAsymm8);

    inputTensorInfo0.SetQuantizationScale(scale0);
    inputTensorInfo0.SetQuantizationOffset(offset0);

    inputTensorInfo1.SetQuantizationScale(scale1);
    inputTensorInfo1.SetQuantizationOffset(offset1);

    outputTensorInfo.SetQuantizationScale(outScale);
    outputTensorInfo.SetQuantizationOffset(outOffset);

    auto input0 = MakeTensor<uint8_t, 4>(inputTensorInfo0, values0);
    auto input1 = MakeTensor<uint8_t, 4>(inputTensorInfo1, values1);

    LayerTestResult<uint8_t, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<uint8_t, 4>(outputTensorInfo, outValues);

    std::unique_ptr<armnn::ITensorHandle> inputHandle0 = workloadFactory.CreateTensorHandle(inputTensorInfo0);
    std::unique_ptr<armnn::ITensorHandle> inputHandle1 = workloadFactory.CreateTensorHandle(inputTensorInfo1);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::MultiplicationQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data,  info, inputTensorInfo0, inputHandle0.get());
    AddInputToWorkload(data,  info, inputTensorInfo1, inputHandle1.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateMultiplication(data, info);

    inputHandle0->Allocate();
    inputHandle1->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle0.get(), &input0[0][0][0][0]);
    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());

    return result;
}
} // anonymous namespace

LayerTestResult<uint8_t, 4> MultiplicationUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    unsigned int batchSize = 1;
    unsigned int channels = 2;
    unsigned int height = 2;
    unsigned int width = 3;
    const unsigned int shape[] = { batchSize, channels, height, width };

    // See dequantized values to the right
    std::vector<uint8_t> input0({
         62,  37,   3, 172,  13, 111, // 244, 144,   8, 684,  48, 440,
        188,  20,  73,  31,  23,  31  // 748,  76, 288, 120,  88, 120
    });

    // See dequantized values to the right
    std::vector<uint8_t> input1({
        126, 240, 252, 183, 121, 247, // 384, 726, 762, 555, 369, 747,
         48, 115, 151,  79,  78,  97  // 150, 351, 459, 243, 240, 297
    });

    // See dequantized values to the right
    std::vector<uint8_t> output(
    {
         64,  72,   0, 255,   8, 236, //  93696, 104544, 6096(clamped), 379620(clamped), 17712, 328680,
         77,  15,  92,  16,  10,  21, // 112200,  26676,        132192,           29160, 21120,  35640
    });

    return MultiplicationUint8TestHelper(workloadFactory,
                                         shape,
                                         input0,
                                         4.0f,
                                         1,
                                         shape,
                                         input1,
                                         3.0f,
                                         -2,
                                         shape,
                                         output,
                                         1366.255f, // Scale/offset chosen to have output values out of range
                                         -5);
}

LayerTestResult<uint8_t, 4> MultiplicationBroadcast1ElementUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    const unsigned int shape0[] = { 1, 2, 2, 3 };
    const unsigned int shape1[] = { 1, 1, 1, 1 };

    std::vector<uint8_t> input0({
        1, 2, 3,    4,  5,  6,
        7, 8, 9,   10, 11, 12
    });

    std::vector<uint8_t> input1({2});

    std::vector<uint8_t> output({
        2,  4,   6,     8, 10, 12,
        14, 16, 18,    20, 22, 24
    });

    return MultiplicationUint8TestHelper(workloadFactory,
                                         shape0,
                                         input0,
                                         1.0f,
                                         0,
                                         shape1,
                                         input1,
                                         1.0f,
                                         0,
                                         shape0,
                                         output,
                                         1.0f,
                                         0);
}

LayerTestResult<uint8_t, 4> MultiplicationBroadcast1DVectorUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    const unsigned int shape0[] = { 1, 2, 2, 3 };
    const unsigned int shape1[] = { 1, 1, 1, 3 };

    std::vector<uint8_t> input0({
        1, 2, 3,    4,  5,  6,
        7, 8, 9,   10, 11, 12
    });

    std::vector<uint8_t> input1({1, 2, 3});

    std::vector<uint8_t> output({
        1,  4,   9,     4, 10, 18,
        7, 16,  27,    10, 22, 36
    });

    return MultiplicationUint8TestHelper(workloadFactory,
                                         shape0,
                                         input0,
                                         1.0f,
                                         0,
                                         shape1,
                                         input1,
                                         1.0f,
                                         0,
                                         shape0,
                                         output,
                                         1.0f,
                                         0);
}

LayerTestResult<uint8_t, 4> ResizeBilinearNopUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 4;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::QuantisedAsymm8);
    inputTensorInfo.SetQuantizationScale(1.5f);
    inputTensorInfo.SetQuantizationOffset(-3);

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::QuantisedAsymm8);
    outputTensorInfo.SetQuantizationScale(1.5f);
    outputTensorInfo.SetQuantizationOffset(-3);

    auto input = MakeTensor<uint8_t, 4>(inputTensorInfo, std::vector<uint8_t>({
        1, 2, 3, 4,
        2, 3, 4, 5,
        3, 4, 5, 6,
        4, 5, 6, 7
    }));

    LayerTestResult<uint8_t, 4> result(outputTensorInfo);
    result.outputExpected = input;

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<uint8_t, 4> SimpleResizeBilinearUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 2;
    constexpr unsigned int inputHeight = 2;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth / 2;
    constexpr unsigned int outputHeight = inputHeight / 2;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::QuantisedAsymm8);
    inputTensorInfo.SetQuantizationScale(0.1567f);
    inputTensorInfo.SetQuantizationOffset(1);

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::QuantisedAsymm8);
    outputTensorInfo.SetQuantizationScale(0.1567f);
    outputTensorInfo.SetQuantizationOffset(1);

    auto input = MakeTensor<uint8_t, 4>(inputTensorInfo, std::vector<uint8_t>({
        1, 255,
        200, 250
    }));

    // The 'resize bilinear' operation projects the top-left corner of output texels into the input image,
    // then figures out the interpolants and weights. Note this is different to projecting the centre of the
    // output texel - and thus we'll expect the output 1x1 matrix to contain as its single element the value
    // that was at position (0,0) of the input matrix (rather than an average, which we would expect if projecting
    // the centre).
    LayerTestResult<uint8_t, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<uint8_t, 4>(outputTensorInfo, std::vector<uint8_t>({
        1
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<uint8_t, 4> ResizeBilinearSqMinUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 4;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = inputWidth / 2;
    constexpr unsigned int outputHeight = inputHeight / 2;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::QuantisedAsymm8);
    inputTensorInfo.SetQuantizationScale(3.141592f);
    inputTensorInfo.SetQuantizationOffset(3);

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::QuantisedAsymm8);
    outputTensorInfo.SetQuantizationScale(3.141592f);
    outputTensorInfo.SetQuantizationOffset(3);

    auto input = MakeTensor<uint8_t, 4>(inputTensorInfo, std::vector<uint8_t>({
        1, 2, 3, 4,
        2, 3, 4, 5,
        3, 4, 5, 6,
        4, 5, 6, 7
    }));

    LayerTestResult<uint8_t, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<uint8_t, 4>(outputTensorInfo, std::vector<uint8_t>({
        1, 3,
        3, 5
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<uint8_t, 4> ResizeBilinearMinUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 3;
    constexpr unsigned int inputHeight = 2;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = 2;
    constexpr unsigned int outputHeight = 1;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::QuantisedAsymm8);
    inputTensorInfo.SetQuantizationScale(1.5f);
    inputTensorInfo.SetQuantizationOffset(-1);

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::QuantisedAsymm8);
    outputTensorInfo.SetQuantizationScale(1.5f);
    outputTensorInfo.SetQuantizationOffset(-1);

    auto input = MakeTensor<uint8_t, 4>(inputTensorInfo, std::vector<uint8_t>({
        1,  2,  3, // 3.0, 4.5, 6.0
        5,  8, 13  // 9.0, 13.5, 21.0
    }));

    LayerTestResult<uint8_t, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<uint8_t, 4>(outputTensorInfo, std::vector<uint8_t>({
        1, 3 // 3.0, 5.25
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<uint8_t, 4> ResizeBilinearMagUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    constexpr unsigned int inputWidth = 2;
    constexpr unsigned int inputHeight = 3;
    constexpr unsigned int inputChannels = 1;
    constexpr unsigned int inputBatchSize = 1;

    constexpr unsigned int outputWidth = 5;
    constexpr unsigned int outputHeight = 3;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::DataType::QuantisedAsymm8);
    inputTensorInfo.SetQuantizationScale(0.010765f);
    inputTensorInfo.SetQuantizationOffset(7);

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::DataType::QuantisedAsymm8);
    outputTensorInfo.SetQuantizationScale(0.010132f);
    outputTensorInfo.SetQuantizationOffset(-18);

    auto input = MakeTensor<uint8_t, 4>(inputTensorInfo, std::vector<uint8_t>({
         24, 228, // 0.183005, 2.379065,
        105, 128, // 1.05497, 1.302565
        230,  71  // 2.400595, 0.68896
    }));

    LayerTestResult<uint8_t, 4> result(outputTensorInfo);
    result.outputExpected = MakeTensor<uint8_t, 4>(outputTensorInfo, std::vector<uint8_t>({
          0,  87, 173, 217, 217, // 0.18300501, 1.06142902, 1.93985295, 2.37906504, 2.37906504
         86,  96, 106, 111, 111, // 1.05497003, 1.15400803, 1.25304604, 1.30256498, 1.30256498
        219, 151,  84,  50,  50  // 2.40059495, 1.71594095, 1.03128707, 0.68896002, 0.68896002
    }));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ResizeBilinearQueueDescriptor descriptor;
    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateResizeBilinear(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();
    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

LayerTestResult<float, 4> BatchNormTest(armnn::IWorkloadFactory& workloadFactory)
{
    auto ret = BatchNormTestImpl<float>(workloadFactory, 0.f, 0);
    return ret;
}

LayerTestResult<uint8_t, 4> BatchNormUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    auto ret = BatchNormTestImpl<uint8_t>(workloadFactory, 1.f/20.f, 50);
    return ret;
}

LayerTestResult<uint8_t, 4> ConstantUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return ConstantTestImpl<uint8_t>(workloadFactory, 2e-6f, 1);
}

LayerTestResult<uint8_t, 1> Concatenation1dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation1dTestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concatenation2dDim0Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim0TestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concatenation2dDim1Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim1TestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concatenation2dDim0DiffInputDimsUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim0DiffInputDimsTestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concatenation2dDim1DiffInputDimsUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation2dDim1DiffInputDimsTestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concatenation3dDim0Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim0TestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concatenation3dDim1Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim1TestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concatenation3dDim2Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim2TestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concatenation3dDim0DiffInputDimsUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim0TestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concatenation3dDim1DiffInputDimsUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim1DiffInputDimsTestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concatenation3dDim2DiffInputDimsUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return Concatenation3dDim2DiffInputDimsTestImpl<uint8_t>(workloadFactory, 0.5f, -1);
}

LayerTestResult<float, 4> SimpleMaxPooling2dSize2x2Stride2x2Test(armnn::IWorkloadFactory& workloadFactory,
                                                                 bool forceNoPadding)
{
    return SimpleMaxPooling2dSize2x2Stride2x2TestCommon<float>(workloadFactory, forceNoPadding);
}

LayerTestResult<uint8_t, 4> SimpleMaxPooling2dSize2x2Stride2x2Uint8Test(armnn::IWorkloadFactory& workloadFactory,
                                                                        bool forceNoPadding)
{
    return SimpleMaxPooling2dSize2x2Stride2x2TestCommon<uint8_t>(workloadFactory, forceNoPadding, 3.0f, -5);
}

LayerTestResult<float, 4> SimpleMaxPooling2dSize3x3Stride2x4Test(armnn::IWorkloadFactory& workloadFactory,
                                                                 bool forceNoPadding)
{
    return SimpleMaxPooling2dSize3x3Stride2x4TestCommon<float>(workloadFactory, forceNoPadding);
}

LayerTestResult<uint8_t, 4> SimpleMaxPooling2dSize3x3Stride2x4Uint8Test(armnn::IWorkloadFactory& workloadFactory,
                                                                        bool forceNoPadding)
{
    return SimpleMaxPooling2dSize3x3Stride2x4TestCommon<uint8_t>(workloadFactory, forceNoPadding, 0.1f, 128);
}

LayerTestResult<float, 4> SimpleAveragePooling2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return SimpleAveragePooling2dTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> SimpleAveragePooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return SimpleAveragePooling2dTestCommon<uint8_t>(workloadFactory, 0.5, -1);
}

LayerTestResult<float, 4> IgnorePaddingAveragePooling2dSize3x2Stride2x2Test(armnn::IWorkloadFactory& workloadFactory,
                                                                            bool forceNoPadding)
{
    return IgnorePaddingAveragePooling2dSize3x2Stride2x2TestCommon<float>(workloadFactory, forceNoPadding);
}

LayerTestResult<float, 4> LargeTensorsAveragePooling2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return LargeTensorsAveragePooling2dTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> LargeTensorsAveragePooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return LargeTensorsAveragePooling2dTestCommon<uint8_t>(workloadFactory, 0.5, -1);
}

LayerTestResult<float, 4> SimpleL2Pooling2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return SimpleL2Pooling2dTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> SimpleL2Pooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return SimpleL2Pooling2dTestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> L2Pooling2dSize3Stride1Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize3Stride1TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> L2Pooling2dSize3Stride1Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize3Stride1TestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> L2Pooling2dSize3Stride3Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize3Stride3TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> L2Pooling2dSize3Stride3Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize3Stride3TestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> L2Pooling2dSize3Stride4Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize3Stride4TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> L2Pooling2dSize3Stride4Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize3Stride4TestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> L2Pooling2dSize7Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize7TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> L2Pooling2dSize7Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize7TestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> L2Pooling2dSize9Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize9TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> L2Pooling2dSize9Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return L2Pooling2dSize9TestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> AsymmetricNonSquarePooling2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return AsymmetricNonSquarePooling2dTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> AsymmetricNonSquarePooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return AsymmetricNonSquarePooling2dTestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> ComparePooling2dTest(armnn::IWorkloadFactory& workloadFactory,
                                               armnn::IWorkloadFactory& refWorkloadFactory,
                                               armnn::PoolingAlgorithm  poolingType)
{
    return ComparePooling2dTestCommon<float>(workloadFactory, refWorkloadFactory, poolingType);
}

LayerTestResult<uint8_t, 4> ComparePooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory,
                                                      armnn::IWorkloadFactory& refWorkloadFactory,
                                                      armnn::PoolingAlgorithm  poolingType)
{
    return ComparePooling2dTestCommon<uint8_t>(workloadFactory, refWorkloadFactory, poolingType, 0.1f, 128);
}

LayerTestResult<float, 2> FullyConnectedLargeTest(armnn::IWorkloadFactory& workloadFactory,
                                                  bool transposeWeights)
{
    return FullyConnectedLargeTestCommon<float>(workloadFactory, transposeWeights);
}

LayerTestResult<float, 4> IgnorePaddingSimpleMaxPooling2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleMaxPooling2dTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> IgnorePaddingSimpleMaxPooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleMaxPooling2dTestCommon<uint8_t>(workloadFactory, 1.0f, -5);
}

LayerTestResult<float, 4> IgnorePaddingMaxPooling2dSize3Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingMaxPooling2dSize3TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> IgnorePaddingMaxPooling2dSize3Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingMaxPooling2dSize3TestCommon<uint8_t>(workloadFactory, 1.0f, -5);
}

LayerTestResult<float, 4> IgnorePaddingSimpleAveragePooling2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleAveragePooling2dTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> IgnorePaddingSimpleAveragePooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleAveragePooling2dTestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> IgnorePaddingSimpleAveragePooling2dNoPaddingTest(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleAveragePooling2dNoPaddingTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> IgnorePaddingSimpleAveragePooling2dNoPaddingUint8Test(
    armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleAveragePooling2dNoPaddingTestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> IgnorePaddingAveragePooling2dSize3Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingAveragePooling2dSize3TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> IgnorePaddingAveragePooling2dSize3Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingAveragePooling2dSize3TestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> IgnorePaddingSimpleL2Pooling2dTest(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleL2Pooling2dTestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> IgnorePaddingSimpleL2Pooling2dUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingSimpleL2Pooling2dTestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> IgnorePaddingL2Pooling2dSize3Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingL2Pooling2dSize3TestCommon<float>(workloadFactory);
}

LayerTestResult<uint8_t, 4> IgnorePaddingL2Pooling2dSize3Uint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return IgnorePaddingL2Pooling2dSize3TestCommon<uint8_t>(workloadFactory);
}

LayerTestResult<float, 4> SimplePermuteFloat32Test(armnn::IWorkloadFactory& workloadFactory)
{
    return SimplePermuteFloat32TestCommon(workloadFactory);
};

LayerTestResult<uint8_t, 4> SimplePermuteUint8Test(armnn::IWorkloadFactory& workloadFactory)
{
    return SimplePermuteUint8TestCommon(workloadFactory);
};

LayerTestResult<float, 4> PermuteFloat32ValueSet1Test(armnn::IWorkloadFactory& workloadFactory)
{
    return PermuteFloat32ValueSet1TestCommon(workloadFactory);
};

LayerTestResult<float, 4> PermuteFloat32ValueSet2Test(armnn::IWorkloadFactory& workloadFactory)
{
    return PermuteFloat32ValueSet2TestCommon(workloadFactory);
};

LayerTestResult<float, 4> PermuteFloat32ValueSet3Test(armnn::IWorkloadFactory& workloadFactory)
{
    return PermuteFloat32ValueSet3TestCommon(workloadFactory);
};
