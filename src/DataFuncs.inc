DATA_FUNC(Add, +, {
  float sum = args->Node(1).Float();
  for (int i = 2; i < args->nodes().size(); i++) {
    sum += args->Node(i).Float();
  }
  return sum;
})
DATA_FUNC(Exit, exit, {
  exit(0);
})
DATA_FUNC(GetElem, get_elem, {
  auto idx = args->Node(2).Int();
  auto arr = args->Node(1).Array();
  return arr->Node(idx);
})
DATA_FUNC(If, if, {
  if (args->Node(1).NotNull()) {
    for(int i = 2; i < args->nodes().size(); i++) {
      args->Node(i).Evaluate();
    }
  }
  return 0;
})
DATA_FUNC(IfElse, if_else, {
  auto test = args->Node(1).Evaluate();
  if (test.Float() != 0) {
    return args->Node(2).Evaluate();
  }
  return args->Node(3).Evaluate();
})
DATA_FUNC(Print, print, {
  for(int i = 1; i < args->nodes().size(); i++) {
    args->Node(i).Evaluate().Print(std::cout, 0, false);
  }
  return DataNode::empty_type{};
})
DATA_FUNC(PushBack, push_back, {
  auto arr = args->Node(1).Array();
  arr->PushBack(args->Node(2).Evaluate());
  return 0;
})
DATA_FUNC(Set, set, {
  auto* var = args->Node(1).Var();
  *var = args->Node(2).Evaluate();
  return *var;
})
DATA_FUNC(SetElem, set_elem, {
  auto arr = args->Node(1).Array();
  auto idx = args->Node(2).Int();
  auto val = args->Node(3).Evaluate();
  arr->Node(idx) = val;
  return val;
})
DATA_FUNC(Size, size, {
  return args->Node(1).Array()->count;
})
DATA_FUNC(PrintSymTab, print_sym_tab, {
  Symbol::PrintSymTab();
  return DataNode::empty_type{};
})