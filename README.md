# ZakoVBS — Linux 平台 VBScript 运行器

用 C 语言编写的 VBScript 解释器，运行于 Linux，编译产出单个二进制 `vbs`。基于 GTK3 实现 `MsgBox` / `InputBox` 对话框。

## 依赖

- gcc / make
- GTK3 开发库（Ubuntu/Debian: `sudo apt install libgtk-3-dev`）

## 编译

```sh
make
```

生成二进制 `vbs`。清除产物：`make clean`。

## 用法

```sh
./vbs 脚本.vbs
```

## 支持的 VBScript 语法

- **变量与常量**：`Dim`、`Const`、`Set`、`Option Explicit`、`ReDim`、`Erase`
- **运算符**：`+ - * / \ Mod ^ &`、比较运算、`And Or Not Xor`
- **控制流**：`If / ElseIf / Else / End If`、`Select Case`、`For / Next`（含 `Step`）、`Do While/Until / Loop`、`While / Wend`、`With / End With`
- **函数与过程**：`Function` / `Sub`（支持递归、无括号调用、`Exit Function/Sub`、`Call`）
- **数组**：`Array()`、`Split`、下标访问、`UBound` / `LBound`
- **错误处理**：`On Error Resume Next`

## 内置函数

- **对话框**：`MsgBox`、`InputBox`（GTK3 实现）
- **字符串**：`Len Mid Left Right Trim LTrim RTrim LCase UCase InStr InStrRev Replace Split Join Asc Chr Space String StrComp StrReverse`
- **数学**：`Abs Int Fix Round Sqr Sgn Rnd Sin Cos Tan Atn Log Exp Hex Oct`
- **类型转换**：`CStr CInt CLng CDbl CSng CBool`
- **日期时间**：`Date Time Now Day Month Year Hour Minute Second Weekday DateSerial TimeSerial`
- **判断**：`IsNull IsEmpty IsNumeric IsObject IsArray TypeName VarType`

## 内置对象

- **WScript**：`Echo`、`Quit`、`Sleep`、`CreateObject`
- **Scripting.FileSystemObject / TextStream**：文件创建、打开、读写（`ReadAll`、`ReadLine`、`Write`、`WriteLine`、`AtEndOfStream`）、路径操作
- **Scripting.Dictionary**：`Add`、`Item`、`Exists`、`Remove`、`RemoveAll`、`Keys`、`Items`、`Count`

## 常量

内置常用 VBScript 常量，如 `vbOKOnly`、`vbYesNo`、`vbCritical`、`vbOK`、`vbCancel`、`vbCrLf`、`vbNewLine`、`vbTab`、`ForReading`、`ForWriting`、`ForAppending` 等。

## 示例

`test.vbs` 演示了 MsgBox 对话框、变量运算、循环、内置函数、文件操作、Dictionary、Select Case 等。

## 实现说明

- `vbs.h`：类型定义（值系统、AST 节点、环境、解释器）
- `lexer.c`：词法分析器（字符串、注释、多词关键字如 `End If`、`Exit Do`）
- `parser.c`：递归下降解析器（表达式优先级、控制流、函数/过程定义）
- `interp.c`：树遍历解释器（作用域、数组引用计数、用户函数注册）
- `builtin.c`：内置函数库及 GTK3 对话框
- `main.c`：入口点

## 限制

- 未实现 COM 组件创建、`Class` 定义、正则表达式对象等高级特性
- 不支持 `ExecuteGlobal`、`Eval` 等动态执行功能
- 日期类型以字符串表示，不支持日期运算
