#include <iostream>
#include "macro.h"

// =====================================================================
//                         辅助函数
// =====================================================================

// 解析单行 #define 指令文本（已去掉 "#define" 前缀）
Macro parseDefine(const std::string &defText) {
  std::vector<Token> dt = tokenize(defText);
  Macro m;
  if (dt.empty())
    return m;
  m.name = dt[0].text;

  if (dt.size() > 1 && dt[1].text == "(") {
    // 函数式宏
    m.isFunctionLike = true;
    size_t j = 2;
    while (j < dt.size() && dt[j].text != ")") {
      if (dt[j].text != ",")
        m.params.push_back(dt[j].text);
      j++;
    }
    j++; // 跳过 ')'
    for (; j < dt.size(); j++)
      m.replacement.push_back(dt[j]);
  } else {
    // 对象式宏
    for (size_t j = 1; j < dt.size(); j++)
      m.replacement.push_back(dt[j]);
  }
  return m;
}

/*
 * 预处理：逐行扫描源代码
 *   - #define → 注册宏
 *   - #undef  → 注销宏
 *   - 其他指令 → 跳过
 *   - 代码行 → 词法分析 + 宏展开 → 输出
 */
std::string preprocess(const std::string &source) {
  std::map<std::string, Macro> table;
  std::vector<Token> outputTokens;

  std::istringstream iss(source);
  std::string line;
  while (std::getline(iss, line)) {
    // 去除行首空白
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;
    std::string trimmed = line.substr(start);

    if (trimmed.rfind("#define", 0) == 0) {
      table[/*will set*/ ""] = {}; // placeholder removed below
      Macro m = parseDefine(trimmed.substr(7));
      table[m.name] = m;
      table.erase("");
    } else if (trimmed.rfind("#undef", 0) == 0) {
      std::vector<Token> dt = tokenize(trimmed.substr(6));
      if (!dt.empty())
        table.erase(dt[0].text);
    } else if (trimmed[0] == '#') {
      continue; // 其他预处理指令，跳过
    } else {
      // 代码行：词法分析 + 宏展开
      std::vector<Token> toks = tokenize(line);
      MacroExpander expander(table);
      std::vector<Token> expanded = expander.expand(toks);
      for (auto &t : expanded)
        outputTokens.push_back(t);
    }
  }
  return tokensToString(outputTokens);
}

// =====================================================================
//                         测试用例
// =====================================================================

struct TestCase {
  std::string name;
  std::string source;   // 含 #define 和代码
  std::string code;     // 仅用于显示
  std::string expected; // 期望输出
};

void runAllTests() {
  std::vector<TestCase> tests = {
    {"对象式宏", "#define PI 3.14\ndouble x = PI;", "double x = PI;",
    "double x = 3.14 ;"},
    {"函数式宏", "#define SQUARE(x) ((x)*(x))\nint y = SQUARE(5);",
    "int y = SQUARE(5);", "int y = ( ( 5 ) * ( 5 ) ) ;"},
    {"嵌套对象式宏", "#define A B\n#define B C\nA", "A", "C"},
    {"自引用宏（扩展终止）", "#define A A\nA", "A", "A"},
    {"互引用宏（扩展终止）", "#define A B\n#define B A\nA", "A", "A"},
    {"字符串化 #", "#define STR(x) #x\nSTR(hello)", "STR(hello)",
    "\"hello\""},
    {"字符串化表达式", "#define STR(x) #x\nSTR(a + b)", "STR(a + b)",
    "\"a + b\""},
    {"标记粘贴 ##", "#define CONCAT(a,b) a##b\nCONCAT(foo, bar)",
    "CONCAT(foo, bar)", "foobar"},
    {"嵌套函数式宏", "#define PI 3.14\n#define AREA(r) PI*r*r\nAREA(2)",
    "AREA(2)", "3.14 * 2 * 2"},
    {"实参含宏（重扫描展开）",
    "#define DBL(x) ((x)*2)\n#define VAL 5\nDBL(VAL)", "DBL(VAL)",
    "( ( 5 ) * 2 )"},
    {"链式标记粘贴", "#define CAT3(a,b,c) a##b##c\nCAT3(x, y, z)",
    "CAT3(x, y, z)", "xyz"},
    {"多参数函数宏", "#define ADD(a,b) a+b\nADD(3, 4)", "ADD(3, 4)", "3 + 4"}
  };

  int passed = 0;
  std::cout << "========== 宏展开算法测试 ==========\n\n";
  for (const auto &tc : tests) {
    std::string actual = preprocess(tc.source);
    bool ok = (actual == tc.expected);
    if (ok)
      passed++;

    std::cout << "[测试] " << tc.name << "\n";
    std::cout << "  输入: " << tc.code << "\n";
    std::cout << "  期望: " << tc.expected << "\n";
    std::cout << "  实际: " << actual << "\n";
    std::cout << "  结果: " << (ok ? "PASS" : "FAIL") << "\n\n";
  }
  std::cout << "=====================================\n";
  std::cout << "通过: " << passed << "/" << tests.size() << "\n";
}

int main() {
  runAllTests();
  return 0;
}
