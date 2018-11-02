//*****************************************************************************
// Copyright 2017-2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************
//*******************************************************************************
//  Copyright 2016 The TensorFlow Authors. All Rights Reserved.
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//==============================================================================

#pragma once

#include <limits>
#include <vector>
#include "ngraph/node.hpp"
#include "ngraph/op/abs.hpp"
#include "ngraph/op/add.hpp"
#include "ngraph/op/constant.hpp"
#include "ngraph/op/divide.hpp"
#include "ngraph/op/maximum.hpp"
#include "ngraph/op/minimum.hpp"
#include "ngraph/op/multiply.hpp"
#include "ngraph/op/subtract.hpp"
#include "ngraph/util.hpp"

namespace ngraph
{
    namespace builder
    {
        namespace quantization_util
        {
            std::shared_ptr<Node> max_abs(std::shared_ptr<Node> a, std::shared_ptr<Node> b)
            {
                auto abs_a = std::make_shared<op::Abs>(a);
                auto abs_b = std::make_shared<op::Abs>(b);
                return std::make_shared<op::Maximum>(abs_a, abs_b);
            }

            std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>> quantization_range_for_multiplication(
                std::shared_ptr<Node> min_a, std::shared_ptr<Node> max_a, std::shared_ptr<Node> min_b, std::shared_ptr<Node> max_b)
            {
                /*
                // begin code copied and pasted (and modified) from
                // github.com/tensorflow/tensorflow/blob/master/tensorflow/core/kernels/quantization_utils.h
                float a_one_quant_level = (max_a - min_a) / (std::numeric_limits<T1>::max() -
                                                             std::numeric_limits<T1>::min());
                float b_one_quant_level = (max_b - min_b) / (std::numeric_limits<T2>::max() -
                                                             std::numeric_limits<T2>::min());
                float c_one_quant_level = a_one_quant_level * b_one_quant_level;
                *min_c = c_one_quant_level * std::numeric_limits<T3>::min();
                *max_c = c_one_quant_level * std::numeric_limits<T3>::max();
                // end code copied and pasted (and modified) from
                // github.com/tensorflow/tensorflow/blob/master/tensorflow/core/kernels/quantization_utils.h
                */

                // TODO: assert all same shape
                auto shape = min_a->get_shape();
                auto type = min_a->get_element_type();

                auto u8_range = op::Constant::create(type, shape, {std::numeric_limits<uint8_t>::max() - std::numeric_limits<uint8_t>::min()});
                auto i8_range = op::Constant::create(type, shape, {std::numeric_limits<int8_t>::max() - std::numeric_limits<int8_t>::min()});

                auto a_one_quant_level = (max_a - min_a) / u8_range;
                auto b_one_quant_level = (max_b - min_b) / i8_range;
                auto c_one_quant_level = a_one_quant_level * b_one_quant_level;

                auto i32_min = op::Constant::create(type, shape, {std::numeric_limits<int32_t>::min()});
                auto i32_max = op::Constant::create(type, shape, {std::numeric_limits<int32_t>::max()});

                auto min_c = c_one_quant_level * i32_min;
                auto max_c = c_one_quant_level * i32_max;
                return std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>(min_c, max_c);

            }

            std::shared_ptr<Node> get_scale(std::shared_ptr<Node> min_input,
                                            std::shared_ptr<Node> max_input,
                                            std::shared_ptr<Node> min_filter,
                                            std::shared_ptr<Node> max_filter,
                                            std::shared_ptr<Node> min_freezed_output,
                                            std::shared_ptr<Node> max_freezed_output)
            {
                /*
                auto min_input_const_op = std::static_pointer_cast<ngraph::op::Constant>(min_input);
                auto max_input_const_op = std::static_pointer_cast<ngraph::op::Constant>(max_input);
                auto min_filter_const_op =
                    std::static_pointer_cast<ngraph::op::Constant>(min_filter);
                auto max_filter_const_op =
                    std::static_pointer_cast<ngraph::op::Constant>(max_filter);
                auto min_freezed_output_const_op =
                    std::static_pointer_cast<ngraph::op::Constant>(min_freezed_output);
                auto max_freezed_output_const_op =
                    std::static_pointer_cast<ngraph::op::Constant>(max_freezed_output);
                auto input_min = min_input_const_op->get_vector<float>();
                auto input_max = max_input_const_op->get_vector<float>();
                auto filter_min = min_filter_const_op->get_vector<float>();
                auto filter_max = max_filter_const_op->get_vector<float>();
                auto output_min = min_freezed_output_const_op->get_vector<float>();
                auto output_max = max_freezed_output_const_op->get_vector<float>();

                float min_out_value;
                float max_out_value;
                quantization_range_for_multiplication<uint8_t, int8_t, int32_t>(input_min[0],
                                                                                input_max[0],
                                                                                filter_min[0],
                                                                                filter_max[0],
                                                                                &min_out_value,
                                                                                &max_out_value);

                
                const float max_abs32 = std::max(std::abs(min_out_value), std::abs(max_out_value));
                const float max_abs8 = std::max(std::abs(output_min[0]), std::abs(output_max[0]));
                // Output is signed int.
                // s32 = f32 * std::pow(2, 31)/ max_abs32;
                // s8 = f32 * std::pow(2, 7)/ max_abs8;
                // s8 = s32 * std::pow(2, -24) * max_abs32 / max_abs8;
                const float scale = static_cast<float>(
                    (std::pow(2, -24) * static_cast<double>(max_abs32 / max_abs8)));
                return op::Constant::create(element::f32, Shape{1}, {scale});
                */

                // TODO: assert all same shape
                auto shape = min_input->get_shape();
                auto type = min_input->get_element_type();
                
                auto x = quantization_range_for_multiplication(min_input,
                                                                max_input,
                                                                min_filter,
                                                                max_filter);

                auto min_out_value = x.first;
                auto max_out_value = x.second;

                auto max_abs32 = max_abs(min_out_value, max_out_value);
                auto max_abs8 = max_abs(min_freezed_output, max_freezed_output);

                // Output is signed int.
                // s32 = f32 * std::pow(2, 31)/ max_abs32;
                // s8 = f32 * std::pow(2, 7)/ max_abs8;
                // s8 = s32 * std::pow(2, -24) * max_abs32 / max_abs8;
                
                auto y = op::Constant::create(type, shape, {std::pow(2, -24)}); 
                return y * (max_abs32 / max_abs8);
            }

            std::shared_ptr<Node>
                get_quantization_scale(std::shared_ptr<Node> input_min_range,
                                       std::shared_ptr<Node> input_max_range,
                                       const ngraph::element::Type& quant_type,
                                       bool bump_by_eps = false)
            {
                auto type = input_min_range->get_element_type();
                if (type != input_max_range->get_element_type())
                {
                    throw ngraph_error("get_quantization_scale: min and max must have same type");
                }

                auto shape = input_min_range->get_shape();
                if (shape != input_max_range->get_shape())
                {
                    throw ngraph_error("get_quantization_scale: min and max must have same shape");
                }

                auto min_range = input_min_range;
                auto max_range = input_max_range;

                if (bump_by_eps) // TODO: always true?
                {
                    auto zero = op::Constant::create(type, shape, {0}); //TODO: broadcast
                    min_range = std::make_shared<op::Minimum>(zero, input_min_range);

                    auto abs_input_min_range = std::make_shared<op::Abs>(input_min_range);
                    auto abs_input_max_range = std::make_shared<op::Abs>(input_max_range);
                    auto max_abs_input_range =
                        std::make_shared<op::Maximum>(abs_input_min_range, abs_input_max_range);

                    auto one = op::Constant::create(type, shape, {1}); //TODO: broadcast
                    auto hundred = op::Constant::create(type, shape, {100}); //TODO: broadcast
                    auto epsilon =
                        std::make_shared<op::Maximum>(one, max_abs_input_range) / hundred;

                    max_range = std::make_shared<op::Maximum>(input_max_range, min_range + epsilon);
                    max_range = std::make_shared<op::Maximum>(zero, max_range);
                }

                // TODO: need to calculated quantize_type.max() - quant_type.min()
                size_t bw = quant_type.bitwidth();
                float range = static_cast<float>(
                    (quant_type.is_signed() ? std::pow(2, (bw - 1)) : std::pow(2, bw)) - 1);

                auto target_range = op::Constant::create(type, shape, {range});

                auto abs_min_range = std::make_shared<op::Abs>(min_range);
                auto abs_max_range = std::make_shared<op::Abs>(max_range);
                auto max_abs_range = std::make_shared<op::Maximum>(abs_min_range, abs_max_range);

                return max_abs_range / target_range;
            }
        }
    }
}
