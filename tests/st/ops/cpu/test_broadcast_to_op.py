# Copyright 2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

import numpy as np
import pytest

import mindspore.context as context
from mindspore.ops import operations as P
from mindspore import Tensor
from mindspore.nn import Cell


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_broadcast():
    context.set_context(mode=context.GRAPH_MODE, device_target='CPU')

    shape = (4, 5, 2, 3, 4, 5, 6)
    x_np = np.random.rand(2, 3, 1, 5, 1).astype(np.float32)
    output = P.BroadcastTo(shape)(Tensor(x_np))
    expect = np.broadcast_to(x_np, shape)
    assert np.allclose(output.asnumpy(), expect)

    shape = (3, 5, 7, 4, 5, 6)
    x_np = np.arange(20).reshape((4, 5, 1)).astype(np.int32)
    output = P.BroadcastTo(shape)(Tensor(x_np))
    expect = np.broadcast_to(x_np, shape)
    assert np.allclose(output.asnumpy(), expect)

    shape = (8, 5, 7, 4, 5, 6)
    x_np = np.arange(24).reshape((1, 4, 1, 6)).astype(np.bool)
    output = P.BroadcastTo(shape)(Tensor(x_np))
    expect = np.broadcast_to(x_np, shape)
    assert np.allclose(output.asnumpy(), expect)

    shape = (3, 4, 5, 2, 3, 4, 5, 7)
    x_np = np.random.rand(2, 3, 1, 5, 1).astype(np.float16)
    output = P.BroadcastTo(shape)(Tensor(x_np))
    expect = np.broadcast_to(x_np, shape)
    assert np.allclose(output.asnumpy(), expect)

    shape = (3, 4, 5, 6)
    x_np = np.random.rand(3, 1, 5, 1).astype(np.float32)
    output = P.BroadcastTo(shape)(Tensor(x_np))
    expect = np.broadcast_to(x_np, shape)
    assert np.allclose(output.asnumpy(), expect)

    x1_np = np.random.rand(3, 1, 5, 1).astype(np.float16)
    output = P.BroadcastTo(shape)(Tensor(x1_np))
    expect = np.broadcast_to(x1_np, shape)
    assert np.allclose(output.asnumpy(), expect)

    shape = (2, 3, 4, 5)
    x1_np = np.random.rand(4, 5).astype(np.float32)
    output = P.BroadcastTo(shape)(Tensor(x1_np))
    expect = np.broadcast_to(x1_np, shape)
    assert np.allclose(output.asnumpy(), expect)

    shape = (4, 5)
    x1_np = np.ones((1,)).astype(np.bool_)
    output = P.BroadcastTo(shape)(Tensor(x1_np))
    expect = np.broadcast_to(x1_np, shape)
    assert np.allclose(output.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_broadcast_dyn_init():
    """
    Test running the op with -1's in the init shape to support varied inputs.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target='CPU')

    ms_shape = (-1, 4, 5, 6)
    np_shape = (3, 4, 5, 6)
    x_np = np.random.rand(3, 1, 5, 1).astype(np.float32)
    output = P.BroadcastTo(ms_shape)(Tensor(x_np))
    expect = np.broadcast_to(x_np, np_shape)
    assert np.allclose(output.asnumpy(), expect)

    x1_np = np.random.rand(3, 1, 5, 1).astype(np.float16)
    output = P.BroadcastTo(ms_shape)(Tensor(x1_np))
    expect = np.broadcast_to(x1_np, np_shape)
    assert np.allclose(output.asnumpy(), expect)

    ms_shape = (2, 3, -1, 5)
    np_shape = (2, 3, 4, 5)
    x1_np = np.random.rand(4, 5).astype(np.float32)
    output = P.BroadcastTo(ms_shape)(Tensor(x1_np))
    expect = np.broadcast_to(x1_np, np_shape)
    assert np.allclose(output.asnumpy(), expect)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_broadcast_dyn_invalid_init():
    """
    Test running the op with -1's in the init shape in incorrect positions.
    Expected to fail.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target='CPU')
    ms_shape = (2, -1, 4, 5)
    x_np = np.random.rand(4, 5).astype(np.float32)
    with pytest.raises(ValueError) or pytest.raises(RuntimeError):
        P.BroadcastTo(ms_shape)(Tensor(x_np))


class BroadcastToDynamicShapeNetMS(Cell):
    """
    Construct of dynamic input for BroadcastTo.
    """
    def __init__(self, shape):
        super().__init__()
        self.unique = P.Unique()
        self.gather = P.Gather()
        self.broadcastto = P.BroadcastTo(shape)

    def construct(self, input_x, indices):
        unique_indices, _ = self.unique(indices)
        x = self.gather(input_x, unique_indices, 0)
        return self.broadcastto(x)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_broadcast_to_dynamic_shape():
    """
    Feature: Test dynamic shape of BroadcastTo operator
    Description: dynamic input
    Expectation: success.
    """
    shape = (2, 2, 3)
    input_x_np = np.random.randn(4, 3).astype(np.float32)
    input_x = Tensor(input_x_np)
    unique_indices = Tensor(np.array([0, 1, 0, 1]))
    broadcast_to_net = BroadcastToDynamicShapeNetMS(shape)
    broadcast_to_net(input_x, unique_indices)
