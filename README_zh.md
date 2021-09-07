# 方舟公共组件<a name="ZH-CN_TOPIC_0000001138850082"></a>

-   [简介](#section11660541593)
-   [目录](#section161941989596)
-   [使用说明](#section1312121216216)
-   [相关仓](#section1371113476307)

## 简介<a name="section11660541593"></a>

Runtime组件是方舟运行时的公共组件，主要包括一些语言无关的基础运行库，包含承载字节码以及执行字节码所需要相关信息的ARK File、支持Debugger的Tooling、负责对应系统调用的ARK Base等。

## 目录<a name="section161941989596"></a>

```
/ark/runtime_core
├── assembler             # 汇编器，将文本格式的方舟字节码文件(*.pa)转换为二进制格式的字节码文件(*.abc)，具体格式见：docs/assembly_format.md和docs/file_format.md
├── cmake                 # cmake脚本，包含工具链文件和用于定义构建和测试目标的常用cmake函数
├── CMakeLists.txt        # cmake主入口文件
├── disassembler          # 反汇编器，将二进制格式的方舟字节码文件(*.abc)转换为文本格式的方舟字节码文件(*.pa)
├── docs                  # 包含语言前端、方舟文件格式和运行时的设计文档。
├── dprof                 # 用于ARK运行时搜集profile数据
├── gn                    # GN模板和配置文件
├── isa                   # 字节码ISA描述文件YAML，和ruby脚本和模板
├── ldscripts             # 包含链接器脚本，用于在非PIE可执行文件中放置4GB以上的ELF section。
├── libpandabase          # ARK运行时基本库，包含：日志、同步原语、公共数据结构等
├── libpandafile          # 二进制格式的方舟字节码文件(*.abc)源码仓
├── libziparchive         # 提供读取和使用miniz的ZIP压缩文件的API。
├── panda                 # CLI工具，用于执行方舟字节码文件（*.abc）文件
├── pandastdlib           # 通过方舟汇编编写的标准库
├── resources             # CI jobs描述文件
├── runtime               # ARK运行时公共组件
├── scripts               # CI脚本
├── templates             # ruby模板和脚本，处理包括：命令行选项、记录器组件、错误消息、事件等
├── tests                 # UT用例
└── verification          # 字节码验证器，具体可以参考 docs/bc_verification

```

## 使用说明<a name="section1312121216216"></a>

汇编器工具概述

工具名称为ark\_asm，用于将文本格式的方舟字节码文件转换为二进制格式的方舟字节码文件。

命令行格式：

```
ark_asm [选项] 输入文件 输出文件
```

<a name="table11141827153017"></a>
<table><thead align="left"><tr id="row101462717303"><th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.1"><p id="p51552743010"><a name="p51552743010"></a><a name="p51552743010"></a>选项</p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.2"><p id="p11592710304"><a name="p11592710304"></a><a name="p11592710304"></a>描述</p>
</th>
</tr>
</thead>
<tbody><tr id="row2015172763014"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p171592710306"><a name="p171592710306"></a><a name="p171592710306"></a>--dump-scopes</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p13151527133011"><a name="p13151527133011"></a><a name="p13151527133011"></a>将结果保存到json文件中，以支持在VS Code中的debug模式</p>
</td>
</tr>
<tr id="row1015527173015"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p1615182712308"><a name="p1615182712308"></a><a name="p1615182712308"></a>--help</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p9556101593120"><a name="p9556101593120"></a><a name="p9556101593120"></a>帮助提示</p>
</td>
</tr>
<tr id="row1015112763020"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p1815182733012"><a name="p1815182733012"></a><a name="p1815182733012"></a>--log-file</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1615627173019"><a name="p1615627173019"></a><a name="p1615627173019"></a>使能log打印后，指定log文件输出路径</p>
</td>
</tr>
<tr id="row1815112753020"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p2151927193015"><a name="p2151927193015"></a><a name="p2151927193015"></a>--size-stat</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1715312588115"><a name="p1715312588115"></a><a name="p1715312588115"></a>统计并打印出转换后方舟字节码信息</p>
</td>
</tr>
<tr id="row1915182703012"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p17151527133017"><a name="p17151527133017"></a><a name="p17151527133017"></a>--verbose</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p15761152983113"><a name="p15761152983113"></a><a name="p15761152983113"></a>使能log打印</p>
</td>
</tr>
</tbody>
</table>

输入文件：文本格式的方舟字节码

输出文件：二进制格式的方舟字节码

反汇编器工具概述

工具名称为ark\_dissam，用于将二进制格式的方舟字节码文件转换为可读的文本格式的方舟字节码文件。

命令行格式：

```
ark_dissam [选项] 输入文件 输出文件
```

<a name="table125062517328"></a>
<table><thead align="left"><tr id="row125182553217"><th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.1"><p id="p175162514327"><a name="p175162514327"></a><a name="p175162514327"></a>选项</p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.2"><p id="p6512255324"><a name="p6512255324"></a><a name="p6512255324"></a>描述</p>
</th>
</tr>
</thead>
<tbody><tr id="row5511825103218"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p45172513326"><a name="p45172513326"></a><a name="p45172513326"></a>--debug</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1245695053215"><a name="p1245695053215"></a><a name="p1245695053215"></a>使能调试信息</p>
</td>
</tr>
<tr id="row951112515321"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p451192515323"><a name="p451192515323"></a><a name="p451192515323"></a>--debug-file</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p175142583210"><a name="p175142583210"></a><a name="p175142583210"></a>调试信息输出文件路径，默认为std::cout</p>
</td>
</tr>
<tr id="row45116253325"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p85116259328"><a name="p85116259328"></a><a name="p85116259328"></a>--help</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1348135833214"><a name="p1348135833214"></a><a name="p1348135833214"></a>帮助提示</p>
</td>
</tr>
<tr id="row194197407327"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p154205401325"><a name="p154205401325"></a><a name="p154205401325"></a>--verbose</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p369871173312"><a name="p369871173312"></a><a name="p369871173312"></a>增加输出汇编文件的注释信息</p>
</td>
</tr>
</tbody>
</table>

输入文件：二进制格式的方舟字节码

输出文件：文本格式的方舟字节码

## 相关仓<a name="section1371113476307"></a>

[方舟运行时子系统](https://gitee.com/openharmony/ark_js_runtime/blob/master/docs/%E6%96%B9%E8%88%9F%E8%BF%90%E8%A1%8C%E6%97%B6%E5%AD%90%E7%B3%BB%E7%BB%9F.md)

**[ark/runtime\_core](README_zh.md)**

[ark/js\_runtime](https://gitee.com/openharmony/ark_js_runtime/blob/master/README_zh.md)

[ark/ts2abc](https://gitee.com/openharmony/ark_ts2abc/blob/master/README_zh.md)

