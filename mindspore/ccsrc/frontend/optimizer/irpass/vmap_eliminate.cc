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

#include "frontend/optimizer/irpass/vmap_eliminate.h"

#include <string>
#include <vector>
#include "ir/func_graph_cloner.h"
#include "frontend/optimizer/irpass/gradient_eliminate.h"
#include "frontend/parallel/step_parallel_utils.h"
#include "pipeline/pynative/pynative_execute.h"
#include "frontend/operator/composite/vmap.h"

namespace mindspore {
namespace opt {
namespace irpass {
constexpr int kInvalidAxisSize = -1;
namespace internal {
// White list of primitives consistent before and after transformation.
const mindspore::HashSet<std::string> throughtout_op{prim::kPrimMakeTuple->name(),   prim::kPrimMakeList->name(),
                                                     prim::kPrimDepend->name(),      prim::kPrimReturn->name(),
                                                     prim::kPrimUpdateState->name(), prim::kPrimStopGradient->name()};
CNodePtr BuildBindInAxisTupleInput(const AnfNodePtr &input, const ValuePtr &in_axis, const FuncGraphPtr &fg) {
  auto input_abs_elements = dyn_cast<abstract::AbstractTuple>(input->abstract());
  ValueSequencePtr in_axis_value_sequence = nullptr;
  if (in_axis->isa<ValueSequence>()) {
    in_axis_value_sequence = dyn_cast<ValueSequence>(in_axis);
    if (input_abs_elements->size() != in_axis_value_sequence->size()) {
      MS_LOG(EXCEPTION) << "The length of input and in_axis should be the same but got input length: "
                        << input_abs_elements->size() << ", in_axis length: " << in_axis_value_sequence->size() << ".";
    }
  }
  std::vector<AnfNodePtr> ret_inputs;
  (void)ret_inputs.emplace_back(NewValueNode(prim::kPrimMakeTuple));
  for (unsigned int i = 0; i < input_abs_elements->size(); ++i) {
    std::vector<AnfNodePtr> tuple_getitem_cnode_inputs;
    (void)tuple_getitem_cnode_inputs.emplace_back(NewValueNode(prim::kPrimTupleGetItem));
    (void)tuple_getitem_cnode_inputs.emplace_back(input);
    (void)tuple_getitem_cnode_inputs.emplace_back(NewValueNode(static_cast<int64_t>(i)));
    auto tuple_getitem_cnode = fg->NewCNode(tuple_getitem_cnode_inputs);
    auto input_abs_element = (*input_abs_elements)[i];
    auto in_axis_value = in_axis_value_sequence == nullptr ? in_axis : (*in_axis_value_sequence)[i];
    CNodePtr cur_make_tuple = nullptr;
    if (input_abs_element->isa<abstract::AbstractTuple>()) {
      tuple_getitem_cnode->set_abstract(input_abs_element);
      cur_make_tuple = BuildBindInAxisTupleInput(tuple_getitem_cnode, in_axis_value, fg);
    } else {
      std::vector<AnfNodePtr> cur_make_tuple_inputs;
      (void)cur_make_tuple_inputs.emplace_back(NewValueNode(prim::kPrimMakeTuple));
      (void)cur_make_tuple_inputs.emplace_back(tuple_getitem_cnode);
      (void)cur_make_tuple_inputs.emplace_back(NewValueNode(in_axis_value));
      cur_make_tuple = fg->NewCNode(cur_make_tuple_inputs);
    }
    (void)ret_inputs.emplace_back(cur_make_tuple);
  }
  return fg->NewCNode(ret_inputs);
}

AnfNodePtr BindInAxis(const CNodePtr &vmap_app, const ValuePtr &in_axes, size_t *u_monad_offset) {
  FuncGraphPtr vmap_fg = vmap_app->func_graph();
  bool is_in_axes_value_sequence = in_axes->isa<ValueSequence>();
  ValueSequencePtr in_axes_to_value_sequence = dyn_cast<ValueSequence>(in_axes);

  auto inputs = vmap_app->inputs();
  auto inputs_size = inputs.size();
  if (inputs_size == 0) {
    MS_LOG(EXCEPTION) << "The inputs number of CNode: " << vmap_app->DebugString()
                      << " should be positive but got : " << inputs_size << ".";
  }

  // Check the last two (if exists) is monad input.
  size_t io_monad_offset = 0;
  constexpr size_t max_monad_input_num = 2;
  if (HasAbstractMonad(inputs[inputs_size - 1])) {
    if (HasAbstractUMonad(inputs[inputs_size - 1])) {
      *u_monad_offset = 1;
    } else if (inputs_size >= max_monad_input_num && HasAbstractUMonad(inputs[inputs_size - max_monad_input_num])) {
      io_monad_offset++;
      *u_monad_offset = io_monad_offset + 1;
    } else {
      io_monad_offset++;
    }
  }

  size_t abstract_monad_count = *u_monad_offset > io_monad_offset ? *u_monad_offset : io_monad_offset;
  auto real_params_size = inputs_size - abstract_monad_count;
  if (is_in_axes_value_sequence && real_params_size - 1 != in_axes_to_value_sequence->size()) {
    MS_LOG(EXCEPTION) << "The length of vmap_app inputs (except primitive input and monad input) is: "
                      << real_params_size - 1 << " and the length of in_axis is: " << in_axes_to_value_sequence->size()
                      << ". These two numbers should be equal.";
  }

  std::vector<AnfNodePtr> outputs;
  outputs.push_back(vmap_app->input(0));
  for (unsigned int i = 1; i < real_params_size; ++i) {
    auto input = inputs[i];
    auto in_axis = is_in_axes_value_sequence ? (*in_axes_to_value_sequence)[i - 1] : in_axes;
    auto input_abs = input->abstract();
    CNodePtr cur_make_tuple_cnode = nullptr;
    if (input_abs->isa<abstract::AbstractTuple>()) {
      cur_make_tuple_cnode = BuildBindInAxisTupleInput(input, in_axis, vmap_fg);
    } else {
      cur_make_tuple_cnode = vmap_fg->NewCNode({NewValueNode(prim::kPrimMakeTuple), input, NewValueNode(in_axis)});
    }
    (void)outputs.emplace_back(cur_make_tuple_cnode);
  }

  if (abstract_monad_count == 1) {
    (void)outputs.emplace_back(inputs.back());
  } else if (IntToSize(abstract_monad_count) == max_monad_input_num) {
    (void)outputs.emplace_back(inputs[inputs_size - max_monad_input_num]);
    (void)outputs.emplace_back(inputs.back());
  }
  return vmap_fg->NewCNode(outputs);
}

ValueSequencePtr GetInAxesSeq(const ValuePtr &in_axes, size_t parameters_size) {
  auto in_axes_seq = dyn_cast<ValueSequeue>(in_axes);
  if (in_axes_seq != nullptr || parameters_size <= 1) {
    return in_axes_seq;
  }

  // Even if the input parameter matches the same negative axis index, it may correspond to different positive indexes.
  // eg. ([A, B, C], -1) equivalent to ([A, B, C], 2), but ([A, B, C, D], -1) equivalent to ([A, B, C, D], 3) in vmap.
  // Therefore, when the in_axes is a negative integer, with multiple inputs, 'ValuePtr' need to be copied multi-times
  // to carry different positive index later.
  auto in_axes_int = dyn_cast<Int64Imm>(in_axes);

  // sub in_axes maybe a 'None'
  if (in_axes_int == nullptr) {
    return nullptr;
  }
  auto axis_nb = in_axes_int->value();
  if (axis_nb >= 0) {
    return nullptr;
  }
  std::vector<ValuePtr> elements;
  for (size_t i = 0; i < parameters_size; i++) {
    ValuePtr in_axis_copy = std::make_shared<Int64Imm>(axis_nb);
    elements.push_back(in_axis_copy);
  }
  return std::make_shared<ValueSequence>(elements);
}

void GetSubAxisSize(const AbstractBasePtr &sub_abs, ValuePtr *const sub_in_axes, int *axis_size,
                    std::vector<ValuePtr> *corrected_in_axes) {
  int sub_axis_size = GetAxisSizeByAbs(sub_abs, sub_in_axes);
  corrected_in_axes->push_back(*sub_in_axes);
  if (sub_axis_size == kInvalidAxisSize) {
    return;
  }
  if (*axis_size == kInvalidAxisSize) {
    *axis_size = sub_axis_size;
  } else if (*axis_size != sub_axis_size) {
    MS_LOG(EXCEPTION) << "The 'axis_size' of each argument in the scope of 'vmap' should be equal, but got "
                      << *axis_size << " and " << sub_axis_size << ".";
  }
}

int GetAxisSizeByAbs(const AbstractBasePtr &abs, ValuePtr *const in_axes) {
  MS_EXCEPTION_IF_NULL(abs);
  MS_EXCEPTION_IF_NULL(*in_axes);
  int axis_size = kInvalidAxisSize;
  auto abs_sequence = dyn_cast<abstract::AbstractSequence>(abs);
  if (abs_sequence != nullptr) {
    std::vector<ValuePtr> corrected_in_axes;
    AbstractBasePtrList abs_list = abs_sequence->elements();
    size_t parameters_size = abs_sequence->size();
    auto in_axes_seq = GetInAxesSeq(*in_axes, parameters_size);
    int index = 0;
    for (auto sub_abs : abs_list) {
      if (sub_abs->isa<abstract::AbstractMonad>()) {
        break;
      }
      ValuePtr sub_in_axes = in_axes_seq != nullptr ? (*in_axes_seq)[index] : *in_axes;
      GetSubAxisSize(sub_abs, &sub_in_axes, &axis_size, &corrected_in_axes);
      index++;
    }
    *in_axes = std::make_shared<ValueSequence>(corrected_in_axes);
    return axis_size;
  }

  auto in_axes_int = dyn_cast<Int64Imm>(*in_axes);
  if (in_axes_int != nullptr) {
    int64_t axis = in_axes_int->value();
    ShapeVector orig_shape = dyn_cast<abstract::Shape>(abs->BuildShape())->shape();
    int64_t shape_len = SizeToLong(orig_shape.size());
    if (axis < -shape_len || axis >= shape_len) {
      MS_LOG(EXCEPTION) << "ValueError: axis " << axis << " is out of bounds for array of dimension [" << -shape_len
                        << "," << shape_len << ").";
    }
    axis = axis < 0 ? shape_len + axis : axis;
    *in_axes = std::make_shared<Int64Imm>(axis);
    axis_size = LongToInt(orig_shape[LongToSize(axis)]);
    return axis_size;
  }
  return axis_size;
}

// get the axis size of currently vmap scope, at the same time, the negative indexes in in_axes are converted to
// corresponding positive indexes.
int GetAxisSize(const CNodePtr &cnode, ValuePtr *const in_axes) {
  MS_EXCEPTION_IF_NULL(cnode);
  // `axis_size` is unique within the scope of vmap, so we just need to get one of them.
  int axis_size = kInvalidAxisSize;
  size_t parameters_size = cnode->size() - 1;
  auto in_axes_seq = GetInAxesSeq(*in_axes, parameters_size);
  std::vector<ValuePtr> corrected_in_axes;
  for (size_t i = 0; i < parameters_size; ++i) {
    auto sub_abs = cnode->input(i + 1)->abstract();
    if (sub_abs->isa<abstract::AbstractMonad>()) {
      break;
    }
    ValuePtr sub_in_axes = in_axes_seq != nullptr ? (*in_axes_seq)[i] : *in_axes;
    GetSubAxisSize(sub_abs, &sub_in_axes, &axis_size, &corrected_in_axes);
  }
  *in_axes = std::make_shared<ValueSequence>(corrected_in_axes);
  return axis_size;
}

AnfNodePtr MatchOutAxis(const AnfNodePtr &expanded_vmap_node, int parameters_size, size_t u_monad_offset, int axis_size,
                        const ValuePtr &out_axes) {
  FuncGraphPtr vmap_post_fg = std::make_shared<FuncGraph>();
  std::vector<AnfNodePtr> exec_node;
  exec_node.push_back(expanded_vmap_node);
  AnfNodePtr u_monad_node = nullptr;
  int offset = SizeToInt(u_monad_offset);
  int u_monad_index = parameters_size > offset ? parameters_size - offset : parameters_size;
  for (int i = 0; i < parameters_size; ++i) {
    if (i == u_monad_index) {
      u_monad_node = vmap_post_fg->add_parameter();
      exec_node.push_back(u_monad_node);
      continue;
    }
    exec_node.push_back(vmap_post_fg->add_parameter());
  }
  auto vmap_outputs = vmap_post_fg->NewCNode(exec_node);

  if (u_monad_node != nullptr) {
    auto update_state_prim = NewValueNode(prim::kPrimUpdateState);
    auto update_state_cnode = vmap_post_fg->NewCNode({update_state_prim, u_monad_node, vmap_outputs});
    update_state_cnode->set_abstract(u_monad_node->abstract());
    u_monad_node = update_state_cnode;
  }

  auto match_out_axis_app =
    vmap_post_fg->NewCNode({NewValueNode(std::make_shared<prim::VmapMatchOutAxis>("VmapMatchOutAxis")), vmap_outputs,
                            NewValueNode(out_axes), NewValueNode(static_cast<int64_t>(axis_size))});

  if (u_monad_node != nullptr) {
    auto depend_prim = NewValueNode(prim::kPrimDepend);
    auto state_depend = vmap_post_fg->NewCNode({depend_prim, match_out_axis_app, u_monad_node});
    state_depend->set_abstract(match_out_axis_app->abstract());
    vmap_post_fg->set_output(state_depend);
    return NewValueNode(vmap_post_fg);
  }
  vmap_post_fg->set_output(match_out_axis_app);

  return NewValueNode(vmap_post_fg);
}

AnfNodePtr GetVmapRule(const PrimitivePtr &prim, const pipeline::ResourceBasePtr &resource, int axis_size) {
  // Set a child scope named "vmap_'PrimitiveName'" for the vmap rule function,
  // and add "VmapRule" to the front.
  constexpr char vmap_rule_scope[] = "VmapRule/";
  constexpr char vmap_op_child_scope_prefix[] = "/vmap_";
  MS_EXCEPTION_IF_NULL(prim);
  auto scope = std::make_shared<Scope>(vmap_rule_scope + ScopeManager::GetInstance().GetCurrentScope()->name() +
                                       vmap_op_child_scope_prefix + prim->name());
  ScopeGuard scope_guard(scope);

  // Firstly we parse the python VmapRules function registered for specific primitive. If failed, get
  // the vmap general rule.
  FuncGraphPtr vmap_rule_fg = nullptr;
  AnfNodePtr vmap_rule_node = nullptr;
  py::function vmap_rule_fn;
  bool is_side_effect = false;
  if (GetPrimitiveFlag(prim, GRAPH_FLAG_SIDE_EFFECT_MEM)) {
    is_side_effect = true;
  } else if (GetPrimitiveFlag(prim, GRAPH_FLAG_SIDE_EFFECT_IO) && prim->name() != prim::kPrimPrint->name()) {
    MS_LOG(EXCEPTION) << prim->name() << " is a GRAPH_FLAG_SIDE_EFFECT_IO prim, vmap dont support currently.";
  }

  // Get vmap rule for specific primitive.
  if (prim->is_base()) {
    vmap_rule_fn = GetVmapRuleFunction(prim->name(), axis_size);
  } else {
    vmap_rule_fn = prim->cast<PrimitivePyPtr>()->GetVmapRuleFunction(is_side_effect, axis_size);
    if (py::isinstance<py::none>(vmap_rule_fn)) {
      vmap_rule_fn = GetVmapRuleFunction(prim->name(), axis_size);
    }
  }

  // If vmap rule for specific primitive not found, get vmap general rule.
  if (!vmap_rule_fn || py::isinstance<py::none>(vmap_rule_fn)) {
    MS_LOG(DEBUG) << "Fail to find vmap rule function for " << prim->name() << ", try to get the general vmap rule.";
    if (is_side_effect) {
      vmap_rule_fn = python_adapter::GetPyFn("mindspore.ops._vmap", "vmap_monad_rule")(prim->name(), axis_size);
    } else {
      vmap_rule_node =
        NewValueNode(std::make_shared<prim::VmapGeneralRule>("VmapGeneralRule", prim, static_cast<int64_t>(axis_size)));
    }
  }

  if (vmap_rule_node == nullptr) {
    vmap_rule_fg = parse::ParsePythonCode(vmap_rule_fn);
    if (vmap_rule_fg == nullptr) {
      MS_LOG(EXCEPTION) << "Fail to parse vmap rule function for " << prim->name() << ".";
    }
    auto vmap_rule_flag = GetPrimitiveFlag(prim, GRAPH_FLAG_SIDE_EFFECT_PROPAGATE);
    if (vmap_rule_flag) {
      vmap_rule_fg->set_flag(mindspore::kFuncGraphFlagReAutoMonad, true);
    }
    pipeline::ResourceBasePtr res = (resource != nullptr) ? resource : std::make_shared<pipeline::Resource>();
    (void)parse::ResolveFuncGraph(vmap_rule_fg, res);
    vmap_rule_node = NewValueNode(vmap_rule_fg);
  }

  return vmap_rule_node;
}

AnfNodePtr ExpandVmapPrimitive(const AnfNodePtr &vnode, const pipeline::ResourceBasePtr &resource, int axis_size) {
  MS_EXCEPTION_IF_NULL(vnode);
  if (!IsValueNode<Primitive>(vnode)) {
    MS_LOG(EXCEPTION) << "Primitive node is not valid.";
  }
  auto prim = GetValueNode<PrimitivePtr>(vnode);
  MS_LOG(DEBUG) << "Overloading Primitive node " << vnode->DebugString() << ".";
  if (throughtout_op.count(prim->name()) > 0) {
    return vnode;
  }
  AnfNodePtr prim_vmap_rule = GetVmapRule(prim, resource, axis_size);
  if (prim_vmap_rule == nullptr) {
    MS_LOG(EXCEPTION) << "Primitive " << prim->name() << " transform to VmapRule failed. NodeInfo: "
                      << trace::GetDebugInfo(prim_vmap_rule->debug_info()) << ".";
  }
  return prim_vmap_rule;
}

AnfNodePtr CopyNodeToVmap(const AnfNodePtr &node, const FuncGraphPtr &func_graph, const FuncGraphManagerPtr &mng) {
  MS_EXCEPTION_IF_NULL(node);
  auto &node_user_map = mng->node_users();
  auto user = node_user_map.find(node);
  if (user != node_user_map.end() && !user->second.empty()) {
    auto user_set = user->second;
    if (user_set.size() > 1) {
      MS_LOG(DEBUG) << "The " << node->DebugString() << " is used in more than one place.";
      bool need_copy = false;
      // We assume that the nodes used in the unified graph are continuous in most cases, therefore, checking the
      // head and tail nodes can pick up the most scenes of that the ValueNode are used by multiple graphs, otherwise,
      // traverse the entire set.
      if (user_set.front().first->func_graph() != func_graph || user_set.back().first->func_graph() != func_graph) {
        need_copy = true;
      } else {
        for (auto pair : user_set) {
          if (pair.first->func_graph() != func_graph) {
            need_copy = true;
            break;
          }
        }
      }
      if (need_copy) {
        MS_LOG(DEBUG) << "Copy the " << node->DebugString() << " so that it can only be used in this graph.";
        auto value_node = dyn_cast<ValueNode>(node);
        MS_EXCEPTION_IF_NULL(value_node);
        auto value = value_node->value();
        MS_EXCEPTION_IF_NULL(value);
        auto copy_node = NewValueNode(value);
        for (auto pair : user_set) {
          if (pair.first->func_graph() == func_graph) {
            auto user_node = pair.first->cast<CNodePtr>();
            mng->SetEdge(user_node, pair.second, copy_node);
          }
        }
        return copy_node;
      }
    }
  }
  return node;
}

void BindNoneAxis(const AnfNodePtr &node, const FuncGraphPtr &func_graph, const FuncGraphManagerPtr &mng) {
  MS_EXCEPTION_IF_NULL(node);
  auto &node_user_map = mng->node_users();
  auto user = node_user_map.find(node);
  if (user != node_user_map.end() && !user->second.empty()) {
    auto make_tuple = NewValueNode(prim::kPrimMakeTuple);
    auto replace_node = func_graph->NewCNode({make_tuple, node, NewValueNode(kNone)});
    auto user_set = user->second;
    for (auto pair : user_set) {
      if (pair.first->func_graph() == func_graph) {
        auto user_node = pair.first->cast<CNodePtr>();
        mng->SetEdge(user_node, pair.second, replace_node);
      }
    }
  }
}

void ExpandVmapValueNode(const FuncGraphPtr &vmap_fg, const pipeline::ResourceBasePtr &resource,
                         mindspore::HashSet<FuncGraphPtr> *visited_graph, mindspore::HashSet<AnfNodePtr> *visited_node,
                         int axis_size) {
  // Map ValueNode.
  auto manager = resource->manager();
  MS_EXCEPTION_IF_NULL(manager);
  auto value_nodes = vmap_fg->value_nodes();
  for (const auto &value_pair : value_nodes) {
    auto node = value_pair.first;
    // ValueNode may have been transformed when other graphs are expanded.
    if (visited_node->count(node) > 0) {
      MS_LOG(DEBUG) << node->DebugString() << " has been transformed.";
      continue;
    }
    node = CopyNodeToVmap(node, vmap_fg, manager);
    if (IsValueNode<FuncGraph>(node)) {
      MS_LOG(DEBUG) << "Map FuncGraph node " << node->DebugString() << ".";
      (void)visited_node->insert(node);
      auto sub_func_graph = GetValueNode<FuncGraphPtr>(node);
      if (visited_graph->count(sub_func_graph) > 0) {
        continue;
      }
      (void)visited_graph->insert(sub_func_graph);
      auto transformed_fg = ExpandVmapFunctor(sub_func_graph, resource, visited_graph, visited_node, axis_size);
      auto replace_node = NewValueNode(transformed_fg);
      (void)visited_node->insert(replace_node);
      (void)manager->Replace(node, replace_node);
    } else if (IsValueNode<Primitive>(node)) {
      auto replace_node = ExpandVmapPrimitive(node, resource, axis_size);
      MS_EXCEPTION_IF_NULL(replace_node);
      (void)visited_node->insert(replace_node);
      (void)manager->Replace(node, replace_node);
    } else if (IsValueNode<Scalar>(node) || IsValueNode<tensor::Tensor>(node) || IsValueNode<None>(node) ||
               IsValueNode<ValueTuple>(node) || IsValueNode<Type>(node) || IsValueNode<StringImm>(node)) {
      auto value_node_ptr = node->cast<ValueNodePtr>();
      ValuePtr node_value = value_node_ptr->value();
      std::vector<ValuePtr> elements;
      elements.push_back(node_value);
      elements.push_back(kNone);
      auto replace_value = std::make_shared<ValueTuple>(elements);
      auto replace_node = NewValueNode(replace_value);
      (void)visited_node->insert(replace_node);
      (void)manager->Replace(node, replace_node);
    } else if (IsValueNode<Monad>(node)) {
      continue;
    } else {
      MS_LOG(EXCEPTION) << "vmap do not support transform " << node->DebugString() << " right now.";
    }
  }
}

void ExpandVmapFreeVariable(const FuncGraphPtr &vmap_fg, const FuncGraphManagerPtr &manager,
                            const mindspore::HashSet<AnfNodePtr> &visited_node) {
  // Map free variable.
  auto free_variables_nodes = vmap_fg->free_variables_nodes();
  for (auto &node : free_variables_nodes) {
    if (visited_node.count(node) > 0 || node->isa<CNode>()) {
      MS_LOG(DEBUG) << node->DebugString() << " has been transformed.";
    } else if (node->isa<Parameter>() || IsValueNode<Scalar>(node) || IsValueNode<tensor::Tensor>(node) ||
               IsValueNode<None>(node) || IsValueNode<ValueTuple>(node) || IsValueNode<Type>(node)) {
      BindNoneAxis(node, vmap_fg, manager);
    } else {
      MS_LOG(EXCEPTION) << "vmap do not support transform " << node->DebugString() << " right now.";
    }
  }
}

FuncGraphPtr ExpandVmapFunctor(const FuncGraphPtr &vmap_fg, const pipeline::ResourceBasePtr &resource,
                               mindspore::HashSet<FuncGraphPtr> *visited_graph,
                               mindspore::HashSet<AnfNodePtr> *visited_node, int axis_size) {
  MS_EXCEPTION_IF_NULL(vmap_fg);
  auto manager = resource->manager();
  MS_EXCEPTION_IF_NULL(manager);
  manager->AddFuncGraph(vmap_fg);

  // The parameters of the current graph will be transformed in the upper graph, and recorded in
  // `visited_node` to avoid being repeatedly transformed refer as a free variable in other graph.
  auto parameter_nodes = vmap_fg->parameters();
  for (auto &node : parameter_nodes) {
    MS_LOG(DEBUG) << "parameter_nodes" << node->DebugString() << ".";
    (void)visited_node->insert(node);
  }

  ExpandVmapValueNode(vmap_fg, resource, visited_graph, visited_node, axis_size);
  ExpandVmapFreeVariable(vmap_fg, manager, *visited_node);

  return vmap_fg;
}

// Entry to perform Vmap transformation.
AnfNodePtr ExpandVmap(const ValueNodePtr &vnode, const pipeline::ResourceBasePtr &resource, int axis_size) {
  MS_EXCEPTION_IF_NULL(vnode);
  if (IsValueNode<FuncGraph>(vnode)) {
    ScopeGuard scope_guard(vnode->scope());
    auto func_graph = GetValueNode<FuncGraphPtr>(vnode);
    MS_EXCEPTION_IF_NULL(func_graph);
    MS_LOG(DEBUG) << "Funcgraph: " << func_graph->ToString() << " will perform the Vmap transformation.";

    // Record transformed FuncGraphs and other nodes to avoid repeatedly expanding and transforming.
    // Whose lifecycle is limited to the current extension.
    mindspore::HashSet<FuncGraphPtr> visited_graph;
    mindspore::HashSet<AnfNodePtr> visited_node;
    (void)visited_graph.insert(func_graph);
    (void)visited_node.insert(vnode);

    auto tf_fg = ExpandVmapFunctor(func_graph, resource, &visited_graph, &visited_node, axis_size);
    visited_node.clear();

    return NewValueNode(tf_fg);
  }
  MS_LOG(EXCEPTION) << "Currently, the first argument in F.vmap only supports Cell, Python defined "
                       "function or @ms_function decorated function.";
}
}  // namespace internal

bool ExpandVmapPrim::operator()(const FuncGraphPtr &, const OptimizerPtr &optimizer) {
  // Expand vmap nodes that don't have embed j or vmap nodes.
  bool change = false;
  auto manager = optimizer->manager();
  MS_EXCEPTION_IF_NULL(manager);
  for (auto &vmap_node : prim_nodes_) {
    auto VmapPrim = GetValueNode<PrimitivePtr>(vmap_node->input(0));
    MS_EXCEPTION_IF_NULL(VmapPrim);
    ValuePtr in_axes = VmapPrim->GetAttr("in_axes");
    MS_EXCEPTION_IF_NULL(in_axes);
    ValuePtr out_axes = VmapPrim->GetAttr("out_axes");
    MS_EXCEPTION_IF_NULL(out_axes);

    auto vmap_fn_node = vmap_node->input(1);
    auto vmap_fg = GetValueNode<FuncGraphPtr>(vmap_fn_node);
    auto &fn_users = manager->node_users()[vmap_fn_node];
    size_t fn_users_size = fn_users.size();

    auto users = manager->node_users()[vmap_node];
    if (users.size() < 1) {
      MS_LOG(EXCEPTION) << "vmap_node could used by at least one CNode, but got users.size() = " << users.size() << ".";
    }
    size_t user_nb = 0;
    size_t user_size = users.size();
    for (auto &user : users) {
      user_nb++;

      // When `vmap_node` has more than one user or `fn` has more than one user, the original function graph
      // cannot be modified directly.
      if ((user_size > 1 && user_nb != user_size) || fn_users_size > 1) {
        MS_LOG(DEBUG) << "Funcgraph: " << vmap_fg->ToString() << " is also used outside the scope of vmap.";
        auto vmap_fg_copy = BasicClone(vmap_fg, true);
        auto manager_ptr = optimizer->resource()->manager();
        manager_ptr->AddFuncGraph(vmap_fg_copy);
        vmap_fn_node = NewValueNode(vmap_fg_copy);
      } else {
        vmap_fn_node = NewValueNode(vmap_fg);
      }

      if (parallel::IsPynativeParallel()) {
        auto func_graph = GetValueNode<FuncGraphPtr>(vmap_fn_node);
        func_graph->set_flag(FUNC_GRAPH_FLAG_DEFER_INLINE, false);
      }

      // get axis size, simultaneous correction the negative in_axes.
      auto vmap_app = user.first->cast<CNodePtr>();
      int user_index = user.second;
      int parameters_size = SizeToInt(vmap_app->size() - 1);
      int axis_size = internal::GetAxisSize(vmap_app, &in_axes);
      if (axis_size == kInvalidAxisSize) {
        MS_LOG(EXCEPTION) << "Failed to get 'axis_size' within the scope of vmap.";
      }
      MS_LOG(DEBUG) << "The axis size corresponding to the current level vmap scope is " << axis_size << ".";

      // Step1: Bind the inputs with the corresponding in_axes.
      size_t u_monad_offset = 0;
      auto bind_axes_node = internal::BindInAxis(vmap_app, in_axes, &u_monad_offset);
      MS_EXCEPTION_IF_NULL(bind_axes_node);
      (void)manager->Replace(vmap_app, bind_axes_node);

      // Step2: Bind the variables with the corresponding axis, and overload the original
      // operation with the VmapRule operation meanwhile transfer the axis information.
      auto expanded_vmap = internal::ExpandVmap(vmap_fn_node->cast<ValueNodePtr>(), optimizer->resource(), axis_size);
      MS_EXCEPTION_IF_NULL(expanded_vmap);

      // Step3: Convert the outputs according to the out_axes to the specified physical perspective.
      auto match_out_axis = internal::MatchOutAxis(expanded_vmap, parameters_size, u_monad_offset, axis_size, out_axes);
      MS_EXCEPTION_IF_NULL(match_out_axis);
      manager->SetEdge(bind_axes_node, user_index, match_out_axis);
    }
    change = true;
  }
  return change;
}
}  // namespace irpass
}  // namespace opt
}  // namespace mindspore
