#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

#include "stream-helpers.h"

struct Symbol {
  Symbol(const char* string = g_null_string);
  static Symbol Load(std::istream& stream);
  bool operator==(const Symbol& that) const;
  const char* Str() const;
  static void PrintSymTab();
private:
  const char* value;

  static std::unordered_set<std::string> g_symbol_table;
  static const char* g_null_string;
};
namespace std
{
    template<> struct hash<Symbol>
    {
        std::size_t operator()(Symbol const& s) const noexcept
        {
            return (size_t)s.Str();
        }
    };
}

enum class DataType : uint32_t { 
  INT = 0x00,
  FLOAT = 0x01,
  VARIABLE = 0x02,
  FUNC = 0x03,
  OBJECT = 0x04,
  SYMBOL = 0x05,
  EMPTY = 0x06,
  IFDEF = 0x07,
  ELSE = 0x08,
  ENDIF = 0x09,
  ARRAY = 0x10,
  COMMAND = 0x11,
  STRING = 0x12,
  OBJECT_PROP_REF = 0x13,
  GLOB = 0x14,
  DEFINE = 0x20,
  INCLUDE = 0x21,
  MERGE = 0x22,
  IFNDEF = 0x23,
  AUTORUN = 0x24,
  UNDEF = 0x25
};
struct DataNode;

struct DataArray {
  ~DataArray();
  DataArray(){}
  DataArray(int16_t count) : count(count) {
    content = std::vector<DataNode>(count);
  }
  DataArray(const char* str) {
    int strLen = strlen(str);
    if (strLen > 65534 || strLen < 0) {
      throw std::exception("String is too large");
    }
    count = -strLen - 1;
    content = std::string(str);
  }
  DataArray(const DataArray& other);
  void Load(std::istream& stream);
  void Save(std::ostream& stream) const;
  void SaveGlob(std::ostream& stream) const;
  void LoadGlob(std::istream& stream, bool isGlob);
  void Print(std::ostream& stream, int indent = 0) const;
  void Resize(short count);
  void PushBack(const DataNode& node);
  DataNode& Node(int idx);
  DataNode Execute();

  // Returns the named array
  std::shared_ptr<DataArray> FindArray(const std::string& name);
  // Returns the named int
  int FindInt(const std::string& name);
  // Returns the named float
  float FindFloat(const std::string& name);
  // Returns the named string / symbol
  std::string FindStr(const std::string& name);
  // Returns the named symbol
  Symbol FindSym(const std::string& name);

  // gets the string value.
  const std::string& string() const { return std::get<std::string>(content); }
  // gets the array value.
  const std::vector<DataNode>& nodes() const { return std::get<std::vector<DataNode>>(content); }
  std::variant<std::vector<DataNode>, std::string> content;
  Symbol file{};
  int16_t count{0}, line_num{0}, unknown{0};
};

std::shared_ptr<DataArray> DataReadStream(std::istream& stream);

typedef DataNode (*DataFuncType)(DataArray* args);

struct DataNode {
  struct empty_type{};
  ~DataNode() {}
  DataNode(){}
  DataNode(const DataNode& other) {
    this->val = other.val;
    this->type = other.type;
  }
  DataNode(Symbol s) {
    this->val = s;
    this->type = DataType::SYMBOL;
  }
  DataNode(std::shared_ptr<DataArray> a, DataType t) {
    this->val = a;
    this->type = t;
  }
  DataNode(float f) {
    this->val = f;
    this->type = DataType::FLOAT;
  }
  DataNode(int i) {
    this->val = i;
    this->type = DataType::INT;
  }
  DataNode(empty_type) {
    this->val = 0;
    this->type = DataType::EMPTY;
  }
  void operator=(const DataNode& other);
  void Load(std::istream& stream);
  void Save(std::ostream& stream) const;
  void Print(std::ostream& stream, int indent = 0, bool escape = true) const;
  bool NotNull() const;

  DataNode Evaluate() const;
  // Typed accessors
  int32_t Int() const { return std::get<int32_t>(Evaluate().val); }
  float Float() const {
    auto tmp = Evaluate();
    if (std::holds_alternative<float>(tmp.val))
      return std::get<float>(tmp.val);
    return (float)std::get<int32_t>(tmp.val);
  }
  std::shared_ptr<DataArray> Array() const { return std::get<std::shared_ptr<DataArray>>(Evaluate().val); }
  const char* String() const { return Evaluate().Array()->string().c_str(); }
  Symbol Sym() const { return std::get<Symbol>(Evaluate().val); }
  Symbol LiteralSym() const { return std::get<Symbol>(val); }
  DataFuncType Func() const { return std::get<DataFuncType>(val); }
  DataNode* Var() const { return std::get<DataNode*>(val); }
  
  std::variant<std::shared_ptr<DataArray>, Symbol, DataNode*, DataFuncType, float, int32_t> val;
  DataType type{ DataType::INT };
};

struct DataContext {
  static DataContext global;
  std::unordered_map<Symbol, DataFuncType> Funcs;
  std::unordered_map<Symbol, DataNode> Variables;
  std::unordered_map<Symbol, DataArray> Macros;
};

void DataInitFuncs(void);
DataNode* DataVariable(const Symbol&);
const char* DataVarName(DataNode*);

#define DATA_FUNC(c_name, data_name, body) DataNode Data##c_name(DataArray* args);
#include "DataFuncs.inc"
#undef DATA_FUNC