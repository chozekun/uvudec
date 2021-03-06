/*
UVNet Universal Decompiler (uvudec)
Copyright 2010 John McMaster <JohnDMcMaster@gmail.com>
Licensed under the terms of the LGPL V3 or later, see COPYING for details
*/

#ifndef UVDASM_ARCHITECTURE_H
#define UVDASM_ARCHITECTURE_H

#include "uvd/architecture/architecture.h"
#include "uvd/core/std_iterator.h"
#include "uvdasm/util.h"
#include "uvdasm/opcode_table.h"
#include "uvdasm/config_symbol.h"

/*
Configuration file based architecture
*/
class UVDConfigSection;
class UVDDisasmArchitecture : public UVDArchitecture
{
public:
	UVDDisasmArchitecture();
	virtual ~UVDDisasmArchitecture();

	virtual uv_err_t init();	
	uv_err_t init_config();
	//uv_err_t opcodeDeinit();
	uv_err_t init_misc(UVDConfigSection *misc_section);
	uv_err_t init_memory(UVDConfigSection *mem_section);
	uv_err_t init_reg(UVDConfigSection *reg_section);
	uv_err_t init_prefix(UVDConfigSection *pre_section);
	uv_err_t init_vectors(UVDConfigSection *section);
	virtual uv_err_t deinit();	

	//Uses standard iterator model
	virtual uv_err_t parseCurrentInstruction(UVDASInstructionIterator &out);
	//Think this was old architecture
	virtual uv_err_t getInstruction(UVDInstruction **out);

	virtual uv_err_t getAddresssSpaceNames(std::vector<std::string> &names);

	void updateCache(uint32_t address, const UVDVariableMap &analysisResult);
	uv_err_t readCache(uint32_t address, UVDVariableMap &analysisResult);

public:	
	std::map<uint32_t, UVDVariableMap> m_analysisCache;

	//TODO: move to UVDCPU
	//Lookup table for opcodes
	//This in theory could be shared between multiple engines for the same arch
	//But not much of an issue since only one instance is expected per run
	UVDDisasmOpcodeLookupTable *m_opcodeTable;
	//For special modifiers mostly for now (functions)
	//Allows special mapping of addresses and others
	UVDSymbolMap *m_symMap;
	
	//Used for advanced analysis
	UVDConfigExpressionInterpreter *m_interpreter;
		
	/*
	Architecture hint
	Necessary for raw binary images
	If an ELF file or similar that contains architecture info in it is given, 
	it may be possible to determine this information dynamically
	FIXME: don't know what this really is for, it should probably be removed
	*/
	std::string m_architecture;
	
	std::string m_architectureFileName;

	//Registers, mapped by name
	std::map<std::string, UVDRegisterShared *> m_registers;

protected:
	//Segmented memory view instead of flat m_data view
	//Most use of m_data should eventually be switched over to use this
	//m_data really only works for simple embedded archs
	//UVDSegmentedMemory m_segmentedMemory;
};

#endif

