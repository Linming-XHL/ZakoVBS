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
./vbs 脚本.vbs [参数...]
```

## 支持的 VBScript 语法

- **变量与常量**：`Dim`（含 `Dim arr(10)` 数组声明）、`Const`、`Set`、`Option Explicit`、`ReDim Preserve`、`Erase`
- **运算符**：`+ - * / \ Mod ^ &`、比较运算、`And Or Not Xor`、`Is`（对象引用比较）
- **控制流**：`If / ElseIf / Else / End If`、`Select Case`、`For / Next`（含 `Step`）、`For Each / Next`、`Do While/Until / Loop`、`While / Wend`、`With / End With`
- **函数与过程**：`Function` / `Sub`（支持递归、无括号调用、无参省略括号、`Exit Function/Sub`、`Call`）
- **类**：`Class / End Class`、成员变量、方法、`Property Get/Let/Set`、`Class_Initialize` 构造函数、`New` 实例化
- **数组**：`Array()`、`Split`、一维/多维下标访问与赋值、`UBound` / `LBound`
- **错误处理**：`On Error Resume Next`、`On Error GoTo label`、`On Error GoTo 0`、`Err` 对象
- **跳转**：`GoTo` / 标签（`label:`）
- **类型声明**：`Type / End Type`（解析兼容）
- **参数**：`WScript.Arguments` 访问命令行参数

## 内置函数

- **对话框**：`MsgBox`、`InputBox`（GTK3 实现）
- **字符串**：`Len Mid Left Right Trim LTrim RTrim LCase UCase InStr InStrRev Replace Split Join Asc Chr Space String StrComp StrReverse Filter`
- **数学**：`Abs Int Fix Round Sqr Sgn Rnd Sin Cos Tan Atn Log Exp Hex Oct`
- **类型转换**：`CStr CInt CLng CDbl CSng CBool`
- **日期时间**：`Date Time Now Day Month Year Hour Minute Second Weekday DateSerial TimeSerial DateAdd DateDiff DatePart Timer IsDate FormatDateTime FormatNumber FormatCurrency FormatPercent`
- **判断**：`IsNull IsEmpty IsNumeric IsObject IsArray IsDate TypeName VarType`

## 内置对象

- **WScript**：`Echo`、`Quit`、`Sleep`、`CreateObject`、`Arguments`
- **Scripting.FileSystemObject / TextStream / Folder / File / Drive**：文件创建、打开、读写（`ReadAll`、`ReadLine`、`Write`、`WriteLine`、`AtEndOfStream`）、复制/移动/删除（`CopyFile`、`MoveFile`、`DeleteFile`、`CreateFolder`、`DeleteFolder`、`FolderExists`）、目录枚举（`GetFolder`、`Files`、`SubFolders`）、文件信息（`GetFile`、`Size`、`DateCreated`）
- **Scripting.Dictionary**：`Add`、`Item`、`Exists`、`Remove`、`RemoveAll`、`Keys`、`Items`、`Count`
- **VBScript.RegExp**：`Pattern`、`Global`、`IgnoreCase`、`Test`、`Replace`、`Execute`（支持 `\d \w \s` 等简写）
- **WScript.Shell**：`Run`、`Popup`、`ExpandEnvironmentStrings`、`SendKeys`、`AppActivate`
- **Err**：`Number`、`Description`、`Clear`、`Raise`

## 常量

内置常用 VBScript 常量，如 `vbOKOnly`、`vbYesNo`、`vbCritical`、`vbOK`、`vbCancel`、`vbCrLf`、`vbNewLine`、`vbTab`、`ForReading`、`ForWriting`、`ForAppending` 等。

## 示例

`test.vbs` 演示了 MsgBox 对话框、变量运算、循环、内置函数、文件操作、Dictionary、Select Case 等。

## 实现说明

- `vbs.h`：类型定义（值系统、AST 节点、环境、解释器）
- `lexer.c`：词法分析器（字符串、注释、多词关键字如 `End If`、`Exit Do`）
- `parser.c`：递归下降解析器（表达式优先级、控制流、函数/过程/类定义、词法器回溯）
- `interp.c`：树遍历解释器（作用域、数组引用计数、用户函数/类注册、GoTo/错误处理）
- `builtin.c`：内置函数库、对象（WScript/FSO/Dictionary/RegExp/WshShell/Err）及 GTK3 对话框
- `main.c`：入口点

## 限制

- 未实现 COM 组件创建、`ExecuteGlobal`、`Eval` 等动态执行功能
- 日期类型以字符串表示，日期运算（`DateAdd`/`DateDiff`）基于简化实现
- 正则表达式通过 POSIX `regex` 转译，不支持完整 PCRE 语法
