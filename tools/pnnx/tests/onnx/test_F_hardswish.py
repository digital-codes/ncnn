# Tencent is pleased to support the open source community by making ncnn available.
#
# Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
# in compliance with the License. You may obtain a copy of the License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import torch
import torch.nn as nn
import torch.nn.functional as F
from packaging import version

def hardswish_forward_0(x):
    return x * F.hardsigmoid(x)

def hardswish_forward_1(x):
    return x * F.hardtanh(x + 3, 0., 6.) / 6.

def hardswish_forward_2(x):
    out = F.relu6(x + 3., True) / 6.
    return out * x

def hardswish_forward_3(x):
    return x * F.relu6(x + 3, inplace=True) / 6

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

    def forward(self, x, y, z, w):
        x = x * 2 - 1
        y = y * 2 - 1
        z = z * 2 - 1
        w = w * 2 - 1
        x = F.hardswish(x)
        y = hardswish_forward_0(y)
        z = hardswish_forward_1(z)
        w = hardswish_forward_2(w)
        w = hardswish_forward_3(w)
        return x, y, z, w

def test():
    if version.parse(torch.__version__) < version.parse('1.10'):
        return True

    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 16)
    y = torch.rand(12, 2, 16)
    z = torch.rand(1, 3, 12, 16)
    w = torch.rand(1, 5, 7, 9, 11)

    a = net(x, y, z, w)

    # export onnx
    torch.onnx.export(net, (x, y, z, w), "test_F_hardswish.onnx")

    # onnx to pnnx
    import os
    os.system("../../src/pnnx test_F_hardswish.onnx inputshape=[1,16],[12,2,16],[1,3,12,16],[1,5,7,9,11]")

    # pnnx inference
    import test_F_hardswish_pnnx
    b = test_F_hardswish_pnnx.test_inference()

    for a0, b0 in zip(a, b):
        if not torch.allclose(a0, b0, 1e-4, 1e-4):
            return False
    return True

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
