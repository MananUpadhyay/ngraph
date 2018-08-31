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

#include <memory>
#include <typeindex>
#include <typeinfo>

#include "ngraph/node.hpp"
#include "ngraph/op/result.hpp"

using namespace std;
using namespace ngraph;

op::Result::Result(const shared_ptr<Node>& arg)
    : Op("Result", check_single_output_args({arg}))
{
    constructor_validate_and_infer_types();
}

void op::Result::validate_and_infer_types()
{
    if (get_input_size() != 1)
    {
        throw ngraph_error("Result expected a single-output argument");
    }

    // always borrow the placement conf even the default one
    set_placement(get_argument(0)->get_placement());
    set_output_type(0, get_input_element_type(0), get_input_shape(0));
}

shared_ptr<Node> op::Result::copy_with_new_args(const NodeVector& new_args) const
{
    if (new_args.size() != 1)
    {
        throw ngraph_error("Incorrect number of new arguments");
    }

    if (new_args.at(0)->get_outputs().size() != 1)
    {
        throw ngraph_error("Result::copy_with_new_args expected a single-output argument");
    }

    auto res = make_shared<Result>(new_args.at(0));
    if (res)
    {
        res->set_needs_copy(m_needs_copy);
        res->set_needs_default_layout(m_needs_default_layout);
    }
    return res;
}

void op::Result::generate_adjoints(autodiff::Adjoints& adjoints, const NodeVector& deltas)
{
    auto delta = deltas.at(0);

    adjoints.add_delta(get_argument(0), delta);
}
