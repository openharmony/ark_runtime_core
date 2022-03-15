# Runtime Core<a name="EN-US_TOPIC_0000001138850082"></a>

- [Runtime Core<a name="EN-US_TOPIC_0000001138850082"></a>](#runtime-core)
  - [Introduction<a name="section11660541593"></a>](#introduction)
  - [Directory Structure<a name="section161941989596"></a>](#directory-structure)
  - [Usage Guidelines<a name="section1312121216216"></a>](#usage-guidelines)
    - [Assembler ark\_asm](#assembler-ark_asm)
    - [Disassembler ark\_disasm](#disassembler-ark_disasm)
  - [Repositories Involved<a name="section1371113476307"></a>](#repositories-involved)

## Introduction<a name="section11660541593"></a>

As a common module of ARK runtime, Runtime Core consists of some basic language-irrelevant runtime libraries, including ARK File, Tooling, and ARK Base. ARK File provides bytecodes and information required for executing bytecodes. Tooling supports Debugger. ARK Base is responsible for implementing platform related utilities.

For more information, see: [ARK Runtime Subsystem](https://gitee.com/openharmony/docs/blob/master/en/readme/ARK-Runtime-Subsystem.md).

## Directory Structure<a name="section161941989596"></a>

```
/ark/runtime_core
├── assembler             # Assembler that converts an ARK bytecode file (*.pa) in text format into a bytecode file (*.abc) in binary format. For details about the format, see docs/assembly_format.md and docs/file_format.md.
├── cmake                 # cmake script that contains the toolchain files and common cmake functions used to define the build and test targets.
├── CMakeLists.txt        # cmake main entry file.
├── disassembler          # Disassembler that converts an ARK bytecode file (*.abc) in binary format into an ARK bytecode file (*.pa) in text format.
├── docs                  # Language frontend, ARK file format, and runtime design documents.
├── dprof                 # Data used to collect the profiling data for ARK runtime.
├── gn                    # GN templates and configuration files.
├── isa                   # Bytecode ISA description file YAML, and Ruby scripts and templates.
├── ldscripts             # Linker scripts used to place ELF sections larger than 4 GB in a non-PIE executable file.
├── libpandabase          # Basic ARK runtime library, including logs, synchronization primitives, and common data structure.
├── libpandafile          # Source code repository of ARK bytecode files (*.abc) in binary format.
├── libziparchive         # provides APIs for reading and using zip files implemented by miniz.
├── panda                 # CLI tool used to execute ARK bytecode files (*.abc).
├── pandastdlib           # Standard libraries wrote by the ARK assembler.
├── runtime               # ARK runtime command module.
├── scripts               # CI scripts.
├── templates             # Ruby templates and scripts used to process command line options, loggers, error messages, and events.
├── tests                 # UT test cases.
└── verification          # Bytecode verifier. See docs/bc_verification.

```

## Usage Guidelines<a name="section1312121216216"></a>

### Assembler ark\_asm

The ark\_asm assembler converts the text ARK bytecode file into a bytecode file in binary format.

Command:

```
ark_asm [Option] Input file Output file
```

<a name="table11141827153017"></a>
<table><thead align="left"><tr id="row101462717303"><th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.1"><p id="p51552743010"><a name="p51552743010"></a><a name="p51552743010"></a>Option</p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.2"><p id="p11592710304"><a name="p11592710304"></a><a name="p11592710304"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row2015172763014"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p171592710306"><a name="p171592710306"></a><a name="p171592710306"></a>--dump-scopes</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p13151527133011"><a name="p13151527133011"></a><a name="p13151527133011"></a>Saves the result to a JSON file to support the debug mode in Visual Studio Code.</p>
</td>
</tr>
<tr id="row1015527173015"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p1615182712308"><a name="p1615182712308"></a><a name="p1615182712308"></a>--help</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p9556101593120"><a name="p9556101593120"></a><a name="p9556101593120"></a>Displays help information.</p>
</td>
</tr>
<tr id="row1015112763020"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p1815182733012"><a name="p1815182733012"></a><a name="p1815182733012"></a>--log-file</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1615627173019"><a name="p1615627173019"></a><a name="p1615627173019"></a>Specifies the log file output path after log printing is enabled.</p>
</td>
</tr>
<tr id="row131515277307"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p111572716304"><a name="p111572716304"></a><a name="p111572716304"></a>--optimize</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p25842312319"><a name="p25842312319"></a><a name="p25842312319"></a>Enables compilation optimization.</p>
</td>
</tr>
<tr id="row1815112753020"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p2151927193015"><a name="p2151927193015"></a><a name="p2151927193015"></a>--size-stat</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1715312588115"><a name="p1715312588115"></a><a name="p1715312588115"></a>Collects statistics on and prints ARK bytecode information after conversion.</p>
</td>
</tr>
<tr id="row1915182703012"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p17151527133017"><a name="p17151527133017"></a><a name="p17151527133017"></a>--verbose</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p15761152983113"><a name="p15761152983113"></a><a name="p15761152983113"></a>Enables log printing.</p>
</td>
</tr>
</tbody>
</table>

Input file: ARK bytecodes in text format

Output file: ARK bytecodes in binary format

### Disassembler ark\_disasm

The ark\_disasm disassembler converts binary ARK bytecodes into readable text ARK bytecodes.

Command:

```
ark_disasm [Option] Input file Output file
```

<a name="table125062517328"></a>
<table><thead align="left"><tr id="row125182553217"><th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.1"><p id="p175162514327"><a name="p175162514327"></a><a name="p175162514327"></a>Option</p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="mcps1.1.3.1.2"><p id="p6512255324"><a name="p6512255324"></a><a name="p6512255324"></a>Description</p>
</th>
</tr>
</thead>
<tbody><tr id="row5511825103218"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p45172513326"><a name="p45172513326"></a><a name="p45172513326"></a>--debug</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1245695053215"><a name="p1245695053215"></a><a name="p1245695053215"></a>Enables the function for printing debug information.</p>
</td>
</tr>
<tr id="row951112515321"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p451192515323"><a name="p451192515323"></a><a name="p451192515323"></a>--debug-file</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p175142583210"><a name="p175142583210"></a><a name="p175142583210"></a>Specifies the path of the debug information output file. The default value is <strong id="b1486165094613"><a name="b1486165094613"></a><a name="b1486165094613"></a>std::cout</strong>.</p>
</td>
</tr>
<tr id="row45116253325"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p85116259328"><a name="p85116259328"></a><a name="p85116259328"></a>--help</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p1348135833214"><a name="p1348135833214"></a><a name="p1348135833214"></a>Displays help information.</p>
</td>
</tr>
<tr id="row194197407327"><td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.1 "><p id="p154205401325"><a name="p154205401325"></a><a name="p154205401325"></a>--verbose</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.1.3.1.2 "><p id="p369871173312"><a name="p369871173312"></a><a name="p369871173312"></a>Outputs the comments of the output file.</p>
</td>
</tr>
</tbody>
</table>

Input file: ARK bytecodes in binary format

Output file: ARK bytecodes in text format


For more information, please see: [ARK Runtime Usage Guide](https://gitee.com/openharmony/ark_js_runtime/blob/master/docs/ARK-Runtime-Usage-Guide.md).

## Repositories Involved<a name="section1371113476307"></a>

**[ark\_runtime\_core](https://gitee.com/openharmony/ark_runtime_core)**

[ark\_js\_runtime](https://gitee.com/openharmony/ark_js_runtime)

[ark\_ts2abc](https://gitee.com/openharmony/ark_ts2abc)
