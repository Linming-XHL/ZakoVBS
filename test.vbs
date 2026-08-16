' 测试MsgBox
Dim result
result = MsgBox("这是一个测试消息", 0, "测试")
WScript.Echo "MsgBox结果:", result

' 测试变量和运算
Dim a, b, c
a = 10
b = 20
c = a + b
WScript.Echo "a + b =", c

' 测试字符串
Dim s
s = "Hello" & " " & "World"
WScript.Echo s

' 测试If语句
Dim x
x = 15
If x > 10 Then
    WScript.Echo "x大于10"
Else
    WScript.Echo "x小于等于10"
End If

' 测试For循环
Dim i
For i = 1 To 5
    WScript.Echo "循环:", i
Next

' 测试Do循环
Dim count
count = 0
Do While count < 3
    WScript.Echo "计数:", count
    count = count + 1
Loop

' 测试内置函数
WScript.Echo "Len(Hello):", Len("Hello")
WScript.Echo "Left:", Left("Hello World", 5)
WScript.Echo "UCase:", UCase("hello")
WScript.Echo "Abs:", Abs(-10)
WScript.Echo "Now:", Now()
WScript.Echo "Date:", Date()
WScript.Echo "Time:", Time()

' 测试数组
Dim arr
arr = Array(10, 20, 30, 40, 50)
WScript.Echo "UBound:", UBound(arr)

' 测试文件操作
Dim fso, file
Set fso = CreateObject("Scripting.FileSystemObject")
Set file = fso.CreateTextFile("test_output.txt", True)
file.WriteLine "这是测试文件内容"
file.WriteLine "第二行"
file.Close
WScript.Echo "文件已创建"

' 测试Dictionary
Dim dict
Set dict = CreateObject("Scripting.Dictionary")
dict.Add "name", "张三"
dict.Add "age", "25"
WScript.Echo "name:", dict.Item("name")
WScript.Echo "Count:", dict.Count

' 测试Select Case
Dim score
score = 85
Select Case score
    Case 90, 100
        WScript.Echo "优秀"
    Case 80, 89
        WScript.Echo "良好"
    Case 70, 79
        WScript.Echo "中等"
    Case Else
        WScript.Echo "不及格"
End Select

WScript.Echo "测试完成!"
WScript.Quit 0