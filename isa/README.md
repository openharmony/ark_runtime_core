Panda Bytecode Description (aka ISA)

Files:

- isa.yaml
	The main file that contains the whole data about Panda Bytecode.
	It is in a machine-readable format and intended to be the single source of
	Bytecode information. It also aims hardcode avoidance in interpreter, compiler
	and tools, faster changes (which is important on early stages of development),
	consistency between components.
- schema.json
	Schema for isa.yaml validation
- templates/
	Directory with example template files which show how could one generate needed
	files from isa.yaml using standard Ruby ERB templates.
	From <name>.<extension>.erb template <name>.<extension> file would be generated.
	(You also need to register your template in CMakeLists.txt)
- isapi.rb
	API for querying parsed yaml data which could be used for template generation.
	In a template you have access to all Ruby core libraries and to 'Panda' module.
	Please refer to the file itself for more details.
- gen.rb
	Driver for template generation. Run './gen.rb --help' for more details.
- CMakeLists.txt
	Build system for ISA
