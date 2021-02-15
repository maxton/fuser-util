#include "Data.h"

#include <map>
#include <sstream>
#include <stack>

std::unordered_set<std::string> Symbol::g_symbol_table;
const char* Symbol::g_null_string = "";

Symbol::Symbol(const char* string) {
  if (!string || string == g_null_string || string[0] == '\0') {
    value = g_null_string;
  } else {
    auto it = g_symbol_table.find(string);
    if (it != g_symbol_table.end()) {
      value = it->data();
    } else {
      value = g_symbol_table.emplace(string).first->c_str();
    }
  }
}
Symbol Symbol::Load(std::istream& stream) {
  auto str = read_symbol(stream);
  auto ret = Symbol(str.c_str());
  return ret;
}
bool Symbol::operator==(const Symbol& that) const {
  return this->value == that.value;
}
const char* Symbol::Str() const {
  return this->value;
}
void Symbol::PrintSymTab() {
  for(const auto& str: g_symbol_table) {
    std::cout << str << std::endl;
  }
}

void DataNode::operator=(const DataNode& other) {
  this->val = other.val;
  this->type = other.type;
}
void DataNode::Load(std::istream& stream) {
  type = static_cast<DataType>(read<uint32_t>(stream));
  switch(type) {
    case DataType::INT:
    case DataType::EMPTY:
    case DataType::ELSE:
    case DataType::ENDIF:
    case DataType::AUTORUN:
      val = read<int32_t>(stream);
      break;
    case DataType::FLOAT:
      val = read<float>(stream);
      break;
    case DataType::SYMBOL:
    case DataType::IFDEF:
    case DataType::DEFINE:
    case DataType::INCLUDE:
    case DataType::MERGE:
    case DataType::IFNDEF:
    case DataType::UNDEF:
      val = Symbol::Load(stream);
      break;
    case DataType::ARRAY:
    case DataType::COMMAND:
    case DataType::OBJECT_PROP_REF:
      val = std::make_shared<DataArray>();
      std::get<std::shared_ptr<DataArray>>(val)->Load(stream);
      break;
    case DataType::STRING:
    case DataType::GLOB:
      val = std::make_shared<DataArray>();
      std::get<std::shared_ptr<DataArray>>(val)->LoadGlob(stream, type == DataType::GLOB);
      break;
    case DataType::VARIABLE:
    case DataType::FUNC:
    case DataType::OBJECT:
    default: {
      std::stringstream ss;
      ss << "Unhandled type " << (int)type << " at 0x" << std::hex << stream.tellg();
      throw std::exception(ss.str().c_str());
    } break;
  }
}
void DataNode::Save(std::ostream& stream) const {
  write(stream, static_cast<uint32_t>(type));
  switch (type) {
    case DataType::INT:
    case DataType::EMPTY:
    case DataType::ELSE:
    case DataType::ENDIF:
    case DataType::AUTORUN:
      write(stream, Int());
      break;
    case DataType::FLOAT:
      write(stream, Float());
      break;
    case DataType::SYMBOL:
    case DataType::IFDEF:
    case DataType::DEFINE:
    case DataType::INCLUDE:
    case DataType::MERGE:
    case DataType::IFNDEF:
    case DataType::UNDEF:
      write_symbol(stream, Sym().Str());
      break;
    case DataType::ARRAY:
    case DataType::COMMAND:
    case DataType::OBJECT_PROP_REF:
      Array()->Save(stream);
      break;
    case DataType::STRING:
    case DataType::GLOB:
      Array()->SaveGlob(stream);
      break;
    case DataType::VARIABLE:
    case DataType::FUNC:
    case DataType::OBJECT:
    default:
      throw std::exception("Unhandled type, sorry");
      break;
  }
}
void DataNode::Print(std::ostream& stream, int indent, bool escape) const {
  switch(type) {
    case DataType::INT:
      stream << std::get<int32_t>(val);
      break;
    case DataType::FLOAT:
      stream << std::get<float>(val);
      break;
    case DataType::SYMBOL:
      if (escape) stream << "'";
      stream << std::get<Symbol>(val).Str();
      if (escape) stream << "'";
      break;
    case DataType::STRING: {
      auto string = std::get<std::shared_ptr<DataArray>>(val);
      if (escape) string->Print(stream, indent);
      else stream << string->string();
    } break;
    case DataType::ARRAY:
      stream << '(';
      std::get<std::shared_ptr<DataArray>>(val)->Print(stream, indent + 1);
      stream << ')';
      break;
    case DataType::COMMAND:
      stream << '{';
      std::get<std::shared_ptr<DataArray>>(val)->Print(stream, indent + 1);
      stream << '}';
      break;
    case DataType::OBJECT_PROP_REF:
      stream << '[';
      std::get<std::shared_ptr<DataArray>>(val)->Print(stream, indent + 1);
      stream << ']';
      break;
    case DataType::EMPTY:
      break;
    case DataType::VARIABLE:
      stream << DataVarName(Var());
      break;
    default:
      stream << "<unknown>";
      break;
  }
}
bool DataNode::NotNull() const {
  auto e = Evaluate();
  switch(e.type) {
    case DataType::SYMBOL:
      return e.Sym().Str()[0] != 0;
    case DataType::STRING:
      return e.String()[0] != 0;
    case DataType::FLOAT:
      return e.Float() != 0;
    case DataType::INT:
      return e.Int() != 0;
    default:
      return true;
  }
}
DataNode DataNode::Evaluate() const {
  switch (type) {
    case DataType::VARIABLE:
      return *std::get<DataNode*>(val);
    case DataType::COMMAND:
      return std::get<std::shared_ptr<DataArray>>(val)->Execute();
    case DataType::OBJECT_PROP_REF:
      return *this;
    default:
      return *this;
  }
}


DataArray::~DataArray()  {}
DataArray::DataArray(const DataArray& other) {
  count = other.count;
  content = other.content;
  line_num = other.line_num;
}
void DataArray::Load(std::istream& stream) {
  auto node_id = read<int32_t>(stream);
  count = read<int16_t>(stream);
  line_num = read<int16_t>(stream);
  auto& n = std::get<std::vector<DataNode>>(content);
  n.resize(count);
  for(int i = 0; i < count; i++) {
    n[i].Load(stream);
  }
}
void DataArray::Save(std::ostream& stream) const {
  write<uint32_t>(stream, 1U);
  write(stream, count);
  write(stream, line_num);
  for (int i = 0; i < count; i++) {
    nodes()[i].Save(stream);
  }
}
void DataArray::SaveGlob(std::ostream& stream) const {
  write_symbol(stream, string());
}
void DataArray::LoadGlob(std::istream& stream, bool isGlob) {
  if (isGlob) {
    throw std::exception("Globs aren't supported, sorry");
  }
  auto string = read_symbol(stream);
  if (string.size() > 65534 || string.size() < 0) {
    throw std::exception("String is too large");
  }
  count = -(short)string.size() - 1;
  this->content = string;
}
std::string escape(const std::string& s) {
  std::stringstream ss;
  for (const auto& c : s) {
    if (c == '\\') ss << "\\\\";
    else if (c == '\"') ss << "\\q";
    else if (c == '\n') ss << "\\n";
    else ss << c;
  }
  return ss.str();
}
void DataArray::Print(std::ostream& stream, int indent) const {
  if (count < 0) {
    stream << '"' << escape(string()) << '"';
    return;
  }
  for (int i = 0; i < count; i++) {
    if (i != 0 && count > 2) stream << std::endl << std::string(indent * 3, ' ');
    else if (i != 0) stream << ' ';
    nodes()[i].Print(stream, indent);
  }
}
void DataArray::Resize(short new_count) {
  if (count < 0) { throw std::exception("Resize not supported on strings"); }
  std::get<std::vector<DataNode>>(content).resize(new_count);
  count = new_count;
}
void DataArray::PushBack(const DataNode& node) {
  if (count < 0) { throw std::exception("Cannot push to a string"); }
  auto& nodes = std::get<std::vector<DataNode>>(content);
  nodes.push_back(node);
  count = (short)nodes.size();
}
DataNode& DataArray::Node(int idx) {
  auto& nodes = std::get<std::vector<DataNode>>(content);
  if (idx >= nodes.size()) {
    throw std::exception("Attempt to read outside array bounds");
  }
  return nodes[idx];
}
DataNode DataArray::Execute() {
  DataNode fun = Node(0).Evaluate();
  switch (fun.type) {
    case DataType::FUNC:
      return (fun.Func())(this);
    case DataType::SYMBOL: {
      const auto sym = fun.Sym();
      if (DataContext::global.Funcs.find(sym) != DataContext::global.Funcs.end()) {
        return DataContext::global.Funcs[fun.Sym()](this);
      }
      std::stringstream ss;
      ss << "Undefined function " << sym.Str();
      throw std::exception(ss.str().c_str());
    }
    default:
      return 0;
  }
}

std::shared_ptr<DataArray> DataArray::FindArray(const std::string& name) {
  for (const auto& node : nodes()) {
    if (node.type == DataType::ARRAY) {
      auto array = node.Array();
      if (array->count > 0
       && array->Node(1).type == DataType::SYMBOL
       && array->Node(1).String() == name) {
         return array;
      }
    }
  }
  throw std::exception("Could not find named array");
}
int DataArray::FindInt(const std::string& name) {
  return FindArray(name)->Node(1).Int();
}
float DataArray::FindFloat(const std::string& name) {
  return FindArray(name)->Node(1).Float();
}
std::string DataArray::FindStr(const std::string& name) {
  return FindArray(name)->Node(1).String();
}
Symbol DataArray::FindSym(const std::string& name) {
  return FindArray(name)->Node(1).Sym();
}

// Tokenizes the whole stream
struct SourceToken {
  std::string value;
  int16_t line;
};
std::vector<SourceToken> Tokenize(std::istream& stream) {
  enum class TokenizerState {
    whitespace,
    quoted_symbol,
    string,
    literal,
    comment
  } state = TokenizerState::whitespace;
  int16_t line = 1;
  char c;
  std::vector<SourceToken> tokens;
  while(stream.get(c)) {
    if (c == '\n') {
      if (line == INT16_MAX) {
        throw std::exception("Too many lines of data :(");
      }
      line++;
      if (state == TokenizerState::comment) {
        state = TokenizerState::whitespace;
      }
    }
    switch (state) {
      case TokenizerState::comment:
        continue;
      case TokenizerState::whitespace:
        switch (c) {
          case '\t':
          case ' ':
          case '\r':
          case '\n':
            continue;
          case '(':
          case '{':
          case '[':
          case ')':
          case '}':
          case ']':
            tokens.emplace_back(std::string(1,c), line);
            break;
          case '"':
            state = TokenizerState::string;
            tokens.emplace_back("\"", line);
            break;
          case '\'':
            state = TokenizerState::quoted_symbol;
            tokens.emplace_back("\'", line);
            break;
          case ';':
            state = TokenizerState::comment;
            break;
          default:
            tokens.emplace_back(std::string(1, c), line);
            state = TokenizerState::literal;
            break;
        }
        break;
      case TokenizerState::literal:
        switch(c) {
          case '\t':
          case ' ':
          case '\r':
          case '\n':
            state = TokenizerState::whitespace;
            continue;
          case '(':
          case '{':
          case '[':
          case ')':
          case '}':
          case ']':
            tokens.emplace_back(std::string(1,c), line);
            state = TokenizerState::whitespace;
            break;
          case '"':
            state = TokenizerState::string;
            tokens.emplace_back("\"", line);
            break;
          case '\'':
            state = TokenizerState::quoted_symbol;
            tokens.emplace_back("\'", line);
            break;
          case ';':
            state = TokenizerState::comment;
            break;
          default:
            tokens.back().value.append(1, c);
            break;
        }
        break;
      case TokenizerState::quoted_symbol:
        tokens.back().value.append(1, c);
        if (c == '\'') state = TokenizerState::whitespace;
        break;
      case TokenizerState::string:
        tokens.back().value.append(1, c);
        if (c == '"') state = TokenizerState::whitespace;
        break;
    }
  }
  return tokens;
}

DataNode ParseLiteral(const std::string& str) {
  // integer: -?[0-9]+
  // float: -?[0-9]+.?[0-9]+
  // symbol: otherwise
  enum class FsmState {
    maybe_negative,
    maybe_int,
    maybe_float,
    symbol
  } state = FsmState::maybe_negative;
  for(const auto& c : str) {
    switch (state) {
      case FsmState::maybe_negative:
        if (c == '-' || c >= '0' && c <= '9') {
          state = FsmState::maybe_int;
        } else {
          state = FsmState::symbol;
        }
        break;
      case FsmState::maybe_int:
        if (c == '.') {
          state = FsmState::maybe_float;
        } else if (c < '0' || c > '9') {
          state = FsmState::symbol;
        }
        break;
      case FsmState::maybe_float:
        if (c < '0' || c > '9') {
          state = FsmState::symbol;
        }
        break;
      case FsmState::symbol:
        break;
    }
  }
  switch(state) {
    case FsmState::maybe_int:
      return DataNode(atoi(str.c_str()));
    case FsmState::maybe_float:
      return DataNode((float)atof(str.c_str()));
    case FsmState::symbol:
      return DataNode(Symbol(str.c_str()));
    default:
      throw std::exception("Unable to parse literal");  
  }
}
std::string unescape(const std::string& string_literal) {
  std::stringstream ss;
  // Skip leading and trailing ""
  for (int i = 1; i < string_literal.size() - 1; i++) {
    auto c = string_literal[i];
    if (c == '\\') {
      if (i < string_literal.size() - 2) {
        switch (string_literal[i + 1]) {
          case 'q':
            ss << '"';
            i++;
            continue;
          case '\\':
            ss << '\\';
            i++;
            continue;
          case 'n':
            ss << '\n';
            i++;
            continue;
        }
      }
    }
    ss << c;
  }
  return ss.str();
}
std::shared_ptr<DataArray> DataReadStream(std::istream& stream) {
  auto ret = std::make_shared<DataArray>();
  auto tokens = Tokenize(stream);
  std::map<char, DataType> array_types = {
    {'(', DataType::ARRAY}, {')', DataType::ARRAY},
    {'[', DataType::OBJECT_PROP_REF}, {']', DataType::OBJECT_PROP_REF},
    {'{', DataType::COMMAND}, {'}', DataType::COMMAND}
  };
  std::stack<std::shared_ptr<DataArray>> arrays;
  std::stack<DataType> types;
  arrays.push(ret);
  types.push(DataType::ARRAY);
  std::shared_ptr<DataArray> temp;
  for(const auto& token : tokens) {
    switch(token.value[0]) {
      case '"':
        temp = std::make_shared<DataArray>(unescape(token.value).c_str());
        temp->line_num = token.line;
        arrays.top()->PushBack(DataNode(temp, DataType::STRING));
        break;
      case '(':
      case '{':
      case '[':
        arrays.push(std::make_shared<DataArray>());
        arrays.top()->line_num = token.line;
        types.push(array_types[token.value[0]]);
        break;
      case ')':
      case '}':
      case ']':
        if (types.size() == 1) {
          std::stringstream s;
          s << "Extra closing bracket at line " << token.line;
          throw std::exception(s.str().c_str());
        }
        if (types.top() != array_types[token.value[0]]) {
          std::stringstream s;
          s << "Mismatched bracket type at line " << token.line;
          throw std::exception(s.str().c_str());
        }
        temp = arrays.top();
        arrays.pop();
        arrays.top()->PushBack(DataNode(temp, types.top()));
        types.pop();
        break;
      default: {
        DataNode value;
        if (token.value[0] == '\'') {
         value = DataNode{Symbol(token.value.substr(1, token.value.size() - 2).c_str())};
        } else {
          value = ParseLiteral(token.value);
        }
        if (value.type == DataType::SYMBOL && value.LiteralSym().Str()[0] == '$') {
          value.type = DataType::VARIABLE;
          value.val = DataVariable(value.LiteralSym());
        }
        arrays.top()->PushBack(value);
      } break;
    }
  }
  if (arrays.size() != 1 || types.size() != 1) {
    throw std::exception("Missing closing bracket(s)");
  }
  return ret;
}

DataContext DataContext::global{};

#define DATA_FUNC(c_name, data_name, body) DataNode Data##c_name(DataArray* args) body
#include "DataFuncs.inc"
#undef DATA_FUNC

void DataInitFuncs(void) {
  #define DATA_FUNC(c_name, data_name, body) {\
    Symbol c_name##name(#data_name); \
    DataContext::global.Funcs[c_name##name] = &Data##c_name; \
  }
  #include "DataFuncs.inc"
  #undef DATA_FUNC
}
DataNode* DataVariable(const Symbol& sym) {
  if (DataContext::global.Variables.find(sym) == DataContext::global.Variables.end()) {
    DataContext::global.Variables.insert({sym, DataNode{0}});
  }
  return &DataContext::global.Variables[sym];
}
const char* DataVarName(DataNode* node) {
  for (const auto&[key, value] : DataContext::global.Variables) {
    if (&value == node) {
      return key.Str();
    }
  }
  return "<error>";
}