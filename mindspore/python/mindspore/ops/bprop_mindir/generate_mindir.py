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
"""Generate the mindir for bprop"""
from __future__ import absolute_import
import os
import shutil
import argparse
import logging

from mindspore.ops import operations as P
from mindspore.ops import _constants
import mindspore.ops._grad as g
from mindspore.ops.operations import _grad_ops as G
from mindspore.ops.operations import _inner_ops as inner
from mindspore._c_expression import _export_bprop_mindir

serializable_bprop_ops = [P.ReLU, P.Identity, inner.Range, P.OnesLike, P.ZerosLike, P.Argmax, P.Argmin, P.Broadcast,
                          P.AssignAdd, P.AssignSub, P.IsFinite, P.ApproximateEqual, P.Sign, P.LogicalNot, P.Round,
                          P.LinSpace, P.DropoutGenMask, P.OneHot, P.Assign, P.IOU, P.BNTrainingReduce, P.Equal,
                          P.NotEqual, P.Greater, P.GreaterEqual, P.Less, P.LessEqual, P.LogicalAnd, P.LogicalOr,
                          P.ReduceAll, P.ReduceAny, P.DropoutDoMask, P.DType, P.Shape, P.DynamicShape, P.Rank,
                          P.Select, P.ScatterMax, P.ScatterMin, P.ScatterUpdate, G.ReluGrad, _constants.kTupleGetItem,
                          P.FloorDiv, P.TruncateDiv, P.Minimum, P.Maximum, P.IsNan, P.IsInf, P.ReLUV2, "Depend",
                          "stop_gradient", "Switch", "UpdateState", "Load"]

logging.getLogger().setLevel(logging.INFO)


def run_generate():
    for op in serializable_bprop_ops:
        if not isinstance(op, str):
            op = op.__name__
        _export_bprop_mindir(op)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Bprop generator")
    parser.add_argument('--mindspore_path', type=str, default=None,
                        help="The absolute path of the mindspore root directory where the bprop source files has been \
                        modified. If not specified, it will find the bprop source files in your mindspore installed \
                        path. Default: None.")

    args_opt = parser.parse_args()
    # mindspore/ops/_grad/__init__.py
    BPROP_PATH = g.__file__
    bprop_installed_dir = BPROP_PATH[: BPROP_PATH.rindex('/')]
    bprop_mindir_export_dir = os.path.join(bprop_installed_dir, "..", "bprop_mindir")

    mindspore_path = args_opt.mindspore_path
    bprop_src_dir = None
    bprop_mindir_src_dir = None
    if mindspore_path is not None:
        mindspore_path = mindspore_path.rstrip('/')
        python_ops_dir = os.path.join(mindspore_path, "mindspore", "python", "mindspore", "ops")
        bprop_src_dir = os.path.join(python_ops_dir, "_grad")
        bprop_mindir_src_dir = os.path.join(python_ops_dir, "bprop_mindir")

    copy_flag = bprop_src_dir is not None and bprop_src_dir != bprop_installed_dir
    # If the specified bprop source directory is not on the mindspore installed path,
    # copy the bprop source files to the installed path.
    BACKUP_SUFFIX = "_generate_bak"
    if copy_flag:
        shutil.rmtree(bprop_installed_dir + BACKUP_SUFFIX, ignore_errors=True)
        os.rename(bprop_installed_dir, bprop_installed_dir + BACKUP_SUFFIX)
        os.mkdir(bprop_installed_dir)
        ls = os.listdir(bprop_src_dir)
        for line in ls:
            file_path = os.path.join(bprop_src_dir, line)
            if os.path.isfile(file_path):
                shutil.copy(file_path, bprop_installed_dir)
                logging.info("copied: %s", file_path)

    run_generate()

    # If the specified bprop source directory is not on the mindspore installed path,
    # copy the generated mindir files to the mindir directory relative to the specified path.
    if copy_flag:
        shutil.rmtree(bprop_installed_dir)
        os.rename(bprop_installed_dir + BACKUP_SUFFIX, bprop_installed_dir)
        ls = os.listdir(bprop_mindir_export_dir)
        for line in ls:
            file_path = os.path.join(bprop_mindir_export_dir, line)
            if file_path.endswith(".mindir") and os.path.isfile(file_path):
                os.chmod(file_path, 0o664)
                shutil.copy(file_path, bprop_mindir_src_dir)
                logging.info("copied: %s", file_path)

        logging.info("The new bprop mindir files has been generated in the path \"%s\"", bprop_mindir_src_dir)
    else:
        logging.info("The new bprop mindir files has been generated in the path \"%s\", "
                     "copy the *.mindir to your mindspore path or PYTHONPATH if necessary.", bprop_mindir_export_dir)
