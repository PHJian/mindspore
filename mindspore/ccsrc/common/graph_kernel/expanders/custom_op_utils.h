/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MINDSPORE_CCSRC_COMMON_GRAPH_KERNEL_EXPANDERS_CUS_UTILS_H_
#define MINDSPORE_CCSRC_COMMON_GRAPH_KERNEL_EXPANDERS_CUS_UTILS_H_

#include <string>
#include <map>

namespace mindspore::graphkernel::expanders {
constexpr int64_t kBlock = 16;

const char kFuncType[] = "hybrid";
const char kTrsmName[] = "trsm";

const std::map<std::string, std::string> kTrsmFuncStrMap = {
  {"trsmL_N_D",
   "def trsmL(a, b):\n"
   "    inverse_0 = allocate(b.shape, b.dtype)\n"
   "    tmp = allocate((b.shape[1], ), b.dtype)\n"
   "    row = b.shape[0]\n"
   "    col = b.shape[1]\n"
   "    for i in range(row):\n"
   "        for j in range(i):\n"
   "            for l in parallel(col // 16):\n"
   "                for k in vectorize(16):\n"
   "                    inverse_0[i, l * 16 + k] = a[i, j] * b[j, l * 16 + k]\n"
   "                    b[i, l * 16 + k] = b[i, l * 16 + k] - inverse_0[i, l * 16 + k]\n"
   "        for l in parallel(col // 16):\n"
   "            for k in vectorize(16):\n"
   "                tmp[l * 16 + k] = a[i, i]\n"
   "                b[i, l * 16 + k] = b[i, l * 16 + k] / tmp[l * 16 + k]\n"
   "    return b\n"},
  {"trsmL_N_U",
   "def trsmL_off_diag(a, b):\n"
   "    inverse_0 = allocate(b.shape, b.dtype)\n"
   "    row = b.shape[0]\n"
   "    col = b.shape[1]\n"
   "    for l in parallel(col // 16):\n"
   "        for i in range(row):\n"
   "            for j in range(i):\n"
   "                for k in vectorize(16):\n"
   "                    inverse_0[i, l * 16 + k] = a[i, j] * b[j, l * 16 + k]\n"
   "                    b[i, l * 16 + k] = b[i, l * 16 + k] - inverse_0[i, l * 16 + k]\n"
   "    return b\n"}};

const char kTrsmLAttrs[] =
  "{\"pragma_enable_reschedule\": false,"
  " \"enable_hoist_cond_write\": false,"
  " \"enable_approximate_read\": true,"
  " \"enable_post_poly_loop_partition\": false,"
  " \"enable_polytops\": \"always\"}";
}  // namespace mindspore::graphkernel::expanders
#endif  // MINDSPORE_CCSRC_COMMON_GRAPH_KERNEL_EXPANDERS_CUS_UTILS_H_
