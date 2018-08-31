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

#include "ngraph/op/broadcast.hpp"
#include "ngraph/op/sum.hpp"

using namespace std;
using namespace ngraph;

op::Broadcast::Broadcast(const std::string& name,
                         const NodeVector& args,
                         const Shape& shape,
                         const AxisSet& broadcast_axes)
    : Op(name, check_single_output_args(args))
    , m_shape(shape)
    , m_broadcast_axes(broadcast_axes)
{
    constructor_validate_and_infer_types();
}

op::Broadcast::Broadcast(const shared_ptr<Node>& arg,
                         const Shape& shape,
                         const AxisSet& broadcast_axes)
    : Broadcast("Broadcast", {arg}, shape, broadcast_axes)
{
}

void op::Broadcast::validate_and_infer_types()
{
    infer_shape();
    Shape target_shape = m_shape;
    for (auto i = m_broadcast_axes.rbegin(); i != m_broadcast_axes.rend(); ++i)
    {
        if (*i >= target_shape.size())
        {
            throw ngraph_error("Broadcast axis exceeds target shape rank");
        }
        target_shape.erase(target_shape.begin() + *i);
    }
    if (Shape{target_shape} != get_input_shape(0))
    {
        throw ngraph_error("Broadcast arg, shape, and axes are incompatible");
    }
    set_output_type(0, get_input_element_type(0), m_shape);
}

shared_ptr<Node> op::Broadcast::copy_with_new_args(const NodeVector& new_args) const
{
    if (new_args.size() != 1)
    {
        throw ngraph_error("Incorrect number of new arguments");
    }
    return make_shared<Broadcast>(new_args.at(0), m_shape, m_broadcast_axes);
}

void op::Broadcast::generate_adjoints(autodiff::Adjoints& adjoints, const NodeVector& deltas)
{
    auto delta = deltas.at(0);

    auto x = get_argument(0);

    adjoints.add_delta(x, make_shared<op::Sum>(delta, m_broadcast_axes));
}

op::BroadcastLike::BroadcastLike(const std::shared_ptr<Node>& arg,
                                 const std::shared_ptr<Node>& like_arg,
                                 const AxisSet& broadcast_axes)
    : Broadcast("BroadcastLike", {arg, like_arg}, {}, broadcast_axes)
{
    constructor_validate_and_infer_types();
}

shared_ptr<Node> op::BroadcastLike::copy_with_new_args(const NodeVector& new_args) const
{
    if (new_args.size() != 2)
    {
        throw ngraph_error("Incorrect number of new arguments");
    }
    return make_shared<BroadcastLike>(new_args.at(0), new_args.at(1), m_broadcast_axes);
}

void op::BroadcastLike::infer_shape()
{
    const Shape& in_shape = get_input_shape(0);
    m_shape = get_input_shape(1);
    if (m_broadcast_axes.size() == 0)
    {
        for (size_t i = in_shape.size(); i < m_shape.size(); ++i)
        {
            m_broadcast_axes.insert(i);
        }
    }
}
