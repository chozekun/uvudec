/*
UVNet Universal Decompiler (uvudec)
Copyright 2008 John McMaster <JohnDMcMaster@gmail.com>
Licensed under the terms of the LGPL V3 or later, see COPYING for details
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "uvd/assembly/address.h"
#include "uvd/assembly/cpu_vector.h"
#include "uvd/assembly/instruction.h"
#include "uvd/core/analysis.h"
#include "uvd/core/runtime.h"
#include "uvd/core/std_iterator.h"
#include "uvd/core/uvd.h"
#include "uvd/data/data.h"
#include "uvd/language/format.h"
#include "uvd/string/engine.h"
#include "uvd/string/string.h"
#include "uvd/util/benchmark.h"
#include "uvd/util/debug.h"
#include "uvd/util/error.h"
#include "uvd/util/types.h"
#include "uvd/util/util.h"

/*
Make it print nicely for output
Any non-printable characters should be converted to some "nicer" form
*/
static std::string stringTableStringFormat(const std::string &s)
{
	std::string sRet;
	
	for( std::string::size_type i = 0; i < s.size(); ++i )
	{
		char c = s[i];
		
		if( isprint(c) )
		{
			sRet += c;
		}
		else
		{
			char buff[64];
		
			//<0x0A> form
			snprintf(buff, 64, "<0x%.2X>", (unsigned int)(unsigned char)c);
			sRet += buff;
		}
	}
	return sRet;
}

/*
UVDStdPrintIterator
*/

UVDStdPrintIterator::UVDStdPrintIterator()
{
	m_positionIndex = 0;
	//eh this doesn't make sense...suprised this compiled before I added operator
	//m_iter = NULL;
}

UVDStdPrintIterator::~UVDStdPrintIterator()
{
}

uv_err_t UVDStdPrintIterator::init(UVD *uvd, UVDAddress address, uint32_t index)
{
	//uv_assert_err_ret(uvd->m_runtime->m_architecture->getInstructionIterator(&m_iter));
	uv_assert_err_ret(uvd->instructionBeginByAddress(address, m_iter));
	uv_assert_err_ret(m_iter.check());

	//uv_assert_ret(m_iter);
	//uv_assert_err_ret(m_iter.init(uvd, address));
	//Initially set to 0 to trigger next()
	m_positionIndex = 0;
	uv_assert_err_ret(parseCurrentLocation());
	m_positionIndex = index;
	
	//End is expressed in terms of sub spaces so this should always remain true
	//uv_assert_ret( m_positionIndex >= m_indexBuffer.size() );

	uv_assert_err_ret(check());

	return UV_ERR_OK;
}

/*
UVDStdPrintIterator UVDStdPrintIterator::operator++()
{
	//FIXME: consider not returning the copy, makes for messy cleanup if we implement destructor
	next();
	return *this;
}

bool UVDStdPrintIterator::operator==(const UVDStdPrintIterator &other) const
{
	return *m_iter == *other.m_iter
			&& m_positionIndex == other.m_positionIndex;
}

bool UVDStdPrintIterator::operator!=(const UVDStdPrintIterator &other) const
{
	return !operator==(other);
}
*/

int UVDStdPrintIterator::compare(const UVDAbstractPrintIterator &otherIn) const
{
	int delta = 0;
	const UVDStdPrintIterator *other = dynamic_cast<const UVDStdPrintIterator *>(&otherIn);
	
	uv_assert_ret(other);
	uv_assert_ret(m_iter.m_iter);
	uv_assert_ret(other->m_iter.m_iter);
	delta = m_iter.compare(other->m_iter);
	//printf("print compare: delta=%d, this pos: %d, other pos: %d\n", delta, m_positionIndex, other->m_positionIndex);
	if( delta )
	{
		return delta;
	}
	
	//At end (of any address space) we should have no instruction and both should have no buffered print
	//consequently the position index should also be 0
	return m_positionIndex - other->m_positionIndex;
}

uv_err_t UVDStdPrintIterator::copy(UVDAbstractPrintIterator **out) const {
	UVDStdPrintIterator *ret = NULL;
	
	uv_assert_ret(m_iter.m_iter);
	
	ret = new UVDStdPrintIterator();
	uv_assert_ret(ret);
	//nothing special needed yet
	*ret = *this;
	uv_assert_ret(ret->m_iter.m_iter);
	uv_assert_ret(out);
	*out = ret;

	return UV_ERR_OK;
}
	
uv_err_t UVDStdPrintIterator::getCurrent(std::string &s)
{
	//New object not yet initialized?
	//printf("getCurrent: position index: %d, index buffer size: %d\n", m_positionIndex, m_indexBuffer.size());
	//uv_assert_ret(!m_isEnd);

	if( m_positionIndex >= m_indexBuffer.size() )
	{
		printf_error("Index out of bounds, m_positionIndex %d >= m_indexBuffer.size() %d\n", m_positionIndex, m_indexBuffer.size());
		return UV_DEBUG(UV_ERR_GENERAL);
	}
	s = m_indexBuffer[m_positionIndex];
	//printf("got: <%s>\n", s.c_str());
	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::initialProcessHeader()
{
	//Program info
	/*
	Just to be clear: I don't own the copyright for the generated file,
	I'm inserting information to help the user track tool versions
	and to advertise
	uvudec was more started in 2009, but some source files date back to 2008 from uvelf from which it was forked
	*/
	uv_assert_err_ret(addComment(UVDSprintf("Generated by uvudec version %s", UVUDEC_VER_STRING)));
	uv_assert_err_ret(addComment(UVDSprintf("uvudec copyright 2009-2011 John McMaster <JohnDMcMaster@gmail.com>")));
	m_indexBuffer.push_back("");
	m_indexBuffer.push_back("");

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::initialProcessStringTable()
{
	UVDAnalyzer *analyzer = g_uvd->m_analyzer;

	m_indexBuffer.push_back("# String table:");

	//uv_assert_ret(m_iter.m_addressSpace);
	//uv_assert_ret(m_iter.m_addressSpace->m_data);

	for( std::vector<UVDString>::iterator iter = analyzer->m_stringEngine->m_strings.begin();
			iter != analyzer->m_stringEngine->m_strings.end(); ++iter )
	{
		UVDString uvdString = *iter;
		std::string string;
		std::vector<std::string> lines;
		
		//Read a string
		uv_assert_err_ret(uvdString.readString(string));

		lines = split(string, '\n', true);		
		if( lines.size() == 1 )
		{
			m_indexBuffer.push_back(UVDSprintf("# 0x%.8X: %s", uvdString.m_addressRange.m_min_addr, stringTableStringFormat(lines[0]).c_str()));				
		}
		else
		{
			m_indexBuffer.push_back(UVDSprintf("# 0x%.8X:", uvdString.m_addressRange.m_min_addr));				
			for( std::vector<std::string>::size_type i = 0; i < lines.size(); ++i )
			{
				m_indexBuffer.push_back(UVDSprintf("#\t%s", stringTableStringFormat(lines[i]).c_str()));				
			}
		}
	}
	m_indexBuffer.push_back("");
	m_indexBuffer.push_back("");

	/*
	printf("index buffer size: %d\n", m_indexBuffer.size());
	for( unsigned int i = 0; i < m_indexBuffer.size(); ++i )
	{
	printf("%s\n", m_indexBuffer[i].c_str());
	}
	exit(1);
	*/
	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::initialProcess()
{
	//printf("initial process\n");
	UVDConfig *config = NULL;
	UVDAnalyzer *analyzer = NULL;
		
	//uv_assert_ret(g_uvd);
	config = g_uvd->m_config;
	uv_assert_ret(config);

	uv_assert_ret(g_uvd);
	analyzer = g_uvd->m_analyzer;
	
	if( config->m_print_header )
	{
		uv_assert_err_ret(initialProcessHeader());
	}

	//TODO: add a plugin header supression option
	for( std::map<std::string, UVDPlugin *>::iterator iter = g_uvd->m_config->m_plugin.m_pluginEngine.m_loadedPlugins.begin();
		iter != g_uvd->m_config->m_plugin.m_pluginEngine.m_loadedPlugins.end(); ++iter )
	{
		UVDPlugin *plugin = (*iter).second;
		std::vector<std::string> headerLines;
		
		uv_assert_ret(plugin);
		if( UV_FAILED(plugin->outputHeader(headerLines)) )
		{
			printf_warn("plugin %s failed to print header\n", (*iter).first.c_str());
		}
		m_indexBuffer.insert(m_indexBuffer.begin(), headerLines.begin(), headerLines.end());
	}

	//String table
	if( config->m_print_string_table )
	{
		uv_assert_err_ret(initialProcessStringTable());
	}

	//printf("initial process done, lines: %d\n", m_indexBuffer.size());
	//Don't include instructions in the first batch if we printed stuff
	if( !m_indexBuffer.empty() )
	{
		return UV_ERR_DONE;
	}
	else
	{
		return UV_ERR_OK;
	}
}

uv_err_t UVDStdPrintIterator::clearBuffers()
{
	m_indexBuffer.clear();
	m_positionIndex = 0;
	return UV_ERR_OK;
}

/*
uv_err_t UVDStdPrintIterator::getEnd(UVD *uvd, UVDStdPrintIterator &iter)
{
	uv_assert_ret(uvd);
	uv_assert_err_ret(uvd->m_runtime->m_architecture->getInstructionIterator(&iter.m_iter));
	uv_assert_ret(iter.m_iter);
	iter.g_uvd = uvd;
	//iter.m_data = uvd->m_data;
	uv_assert_err_ret(iter.makeEnd());
	return UV_ERR_OK;
}
*/

uv_err_t UVDStdPrintIterator::getEnd(UVD *uvd, UVDAddressSpace *addressSpace, UVDStdPrintIterator **out) {
	UVDStdPrintIterator *iter = NULL;
	UVDInstructionIteratorFactory *instFactory = NULL;
		
	iter = new UVDStdPrintIterator();
	uv_assert_ret(iter);
	
	instFactory = g_uvd->m_runtime->m_architecture->m_instructionIteratorFactory;
	uv_assert_err_ret(instFactory->instructionIteratorEndByAddressSpace(&iter->m_iter, addressSpace));
	
	uv_assert_ret(out);
	*out = iter;
	
	return UV_ERR_OK;
	
	/*
	UVDStdPrintIterator *iter = NULL;
	UVDStdInstructionIterator *instIter = NULL;
	
	
	iter = new UVDStdPrintIterator();
	uv_assert_ret(iter);
	
	uv_assert_err_ret(UVDStdInstructionIterator::getEnd(uvd, addressSpace, &instIter));
	uv_assert_ret(instIter);
	iter->m_iter.m_iter = instIter;
	
	iter->m_positionIndex = 0;
	iter->m_indexBuffer.clear();
	
	uv_assert_ret(out);
	*out = iter;
	
	return UV_ERR_OK;
	*/
}

#if 0
uv_err_t UVDStdPrintIterator::makeEnd()
{
	/*
	//Like almost at end
	uv_assert_err_ret(m_iter.makeEnd());
	//But without the buffered data to flush
	//XXX: I don't think this matters anymore, the positions are now synced
	m_indexBuffer.clear();
	m_positionIndex = 0;
	//To try to fix some errors I'm having
	//m_instruction = UVDInstruction();
	*/
	UVDStdInstructionIterator *iter = dynamic_cast<UVDStdInstructionIterator *>(m_iter.m_iter);
	
	uv_assert_ret(iter);
	
	//Make sure our iter is at the end
	uv_assert_err_ret(UVDStdInstructionIterator::getEndFromExisting(iter->m_uvd, iter->m_iter.m_address.m_space, iter));
	//And make sure local objects are also at end status
	m_indexBuffer.clear();
	m_positionIndex = UINT_MAX;
	return UV_ERR_OK;
}
#endif
/*
uv_err_t UVDStdPrintIterator::makeNextEnd()
{
	uv_assert_err_ret(m_iter.makeNextEnd());
	m_positionIndex = 0;
	return UV_ERR_OK;
}
*/

/*
void UVDStdPrintIterator::debugPrint() const
{
	printf_debug("post next, offset: 0x%.8X, index: %d, buffer size: %d\n", m_curPosition, m_positionIndex, m_indexBuffer.size()); 
}
*/

uv_err_t UVDStdPrintIterator::previous()
{
	//FIXME: support backtracking from end()
	//actually, this probably is transparent to us if taken care of in UVDInstructionIterator
	//XXX: might be nice to get a !begin() assert here

	//Simple case: we can backrack buffer
	if( m_positionIndex > 0 )
	{
		--m_positionIndex;
		return UV_ERR_OK;
	}
	
	//Okay, now the f my life case
	//Fortunatly, its not too bad from the print perspective
	uv_assert_err_ret(m_iter.previous());

	//Finally, adjust our iterator to the end of the list
	uv_assert_ret(!m_indexBuffer.empty());
	m_positionIndex = m_indexBuffer.size() - 1;
	
	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::prime()
{
	/*
	FIXME:
	The way that this is done makes going backwards different than begin() for the same address
	Fix later if important
	Otherwise we are going to have to be always checking if at the special "begin" address
	Need to create a hash map of special addresses to check each time maybe?
	Don't want a hack for a single address
	*/

	/*
	For printing, there are certain things which may need to be added before the first line
	In the longer term, this should be replaced with checking a special action list indexed by address
	That way there is a clean way to print function starts and such as well
	This should be done with a virtual function like checkSpecialActions() or something
	*/
	uv_err_t rcInitialProcess = UV_ERR_GENERAL;

	rcInitialProcess = initialProcess();
	uv_assert_err_ret(rcInitialProcess);	
	/*
	//We may have had all prelim prints disabled
	if( rcInitialProcess == UV_ERR_DONE )
	{
		printf("initial parse did stuff\n");
		//eliminated state var, don't do this
		//return UV_ERR_OK;
	}
	else
	{
		printf("initial process inconclusive, continuing\n");
	}
	*/

	uv_assert_err_ret(parseCurrentLocation());

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::next()
{
	//Shouldn't this be after the initial process check?
	++m_positionIndex;

	//printf("next(), position index: %d, index buffer size: %d\n", m_positionIndex, m_indexBuffer.size());
	
	//We may have buffered data leftover form last parse
	//Check this before seeing if we are at end
	//Take buffered element if availible
	if( m_positionIndex < m_indexBuffer.size() )
	{
		//printf("still have room\n");
		return UV_ERR_OK;
	}
	
	//Advance to next instruction location
	//printf("advancing instruction\n");
	uv_assert_err_ret(m_iter.next());

	//Do the actual parsing
	uv_assert_err_ret(parseCurrentLocation());
	//printf("next done, m_positionIndex %d, m_indexBuffer.size() %d\n", m_positionIndex, m_indexBuffer.size());
	
	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::parseCurrentLocation()
{
	//uv_err_t rcSuper = UV_ERR_GENERAL;
	UVDBenchmark benchmark;
	UVDAddress startPosition;
	UVDConfig *config = g_uvd->m_config;
	
	uv_assert_err_ret(m_iter.getAddress(&startPosition));
	
	benchmark.start();
	
	//These assertions aren't valid because initial process might be things like string table etc that has no instruction
	//uv_assert_ret(m_instruction);
	//uv_assert_ret(m_instruction->m_inst_size);

	uv_assert_err_ret(clearBuffers());

	if( config->m_addressLabel )
	{
		uv_assert_err_ret(nextAddressLabel(startPosition));
	}

	if( config->m_addressComment )
	{
		uv_assert_err_ret(nextAddressComment(startPosition));
	}
	
	if( config->m_vectorComment )
	{
		uv_assert_err_ret(nextVectorComment(startPosition));
	}
	
	if( config->m_calledSources )
	{
		uv_assert_err_ret(nextCalledSources(startPosition));
	}
	
	if( config->m_jumpedSources )
	{
		uv_assert_err_ret(nextJumpedSources(startPosition));
	}

	//Best to have data follow analysis
	//Instruction is fully parsed now
	//Convert to necessary string values
	UVDInstruction *instruction = NULL;
	uv_assert_err_ret(m_iter.get(&instruction));
	//printf("instruction: %d\n", instruction != NULL);
	if( instruction )
	{
		uv_assert_ret(instruction->m_inst_size);
		uv_assert_err_ret(g_uvd->stringListAppend(instruction, m_indexBuffer));
	}
	//printf("Generated string list, size: %d\n", m_indexBuffer.size());

	benchmark.stop();
	//printf_debug_level(UVD_DEBUG_SUMMARY, "other than nextInstruction() time: %s\n", benchmark.toString().c_str());

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::printReferenceList(UVDAnalyzedMemoryRange *memLoc, uint32_t type)
{
	UVDAnalyzedMemoryRangeReferences references;
	char buff[256];
	UVD *uvd = NULL;
	UVDFormat *format = NULL;
		
	uvd = g_uvd;
	uv_assert_ret(uvd);
	format = uvd->m_format;
	uv_assert_ret(format);

	//Get all the locations that call this address
	//FIXME: should this be call source?
	uv_assert_err_ret(memLoc->getReferences(references, type));

	for( UVDAnalyzedMemoryRangeReferences::iterator iter = references.begin(); iter != references.end(); ++iter )
	{
		//uint32_t key = (*iter).first;
		UVDMemoryReference *value = (*iter).second;
		uint32_t from = 0;
		
		uv_assert_ret(value);
		from = value->m_from;
		
		std::string formattedAddress;
		uv_assert_err_ret(format->formatAddress(from, formattedAddress));
		snprintf(buff, 256, "#\t%s", formattedAddress.c_str());
		m_indexBuffer.push_back(buff);
	}
	
	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::addWarning(const std::string &lineRaw)
{
	return UV_DEBUG(addComment(lineRaw));
}

uv_err_t UVDStdPrintIterator::addComment(const std::string &lineRaw)
{
	std::string lineCommented;
	
	uv_assert_err_ret(g_uvd->m_format->m_compiler->comment(lineRaw, lineCommented));
	m_indexBuffer.push_back(lineCommented);

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::nextAddressLabel(UVDAddress startPosition)
{
	char buff[256];
	
	//This is like convention adapted by ds52
	//Limit leading zeros by max address size?
	//X00001234:
	snprintf(buff, 256, "X%.8X:", startPosition.m_addr);
	
	m_indexBuffer.insert(m_indexBuffer.end(), buff);

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::nextAddressComment(UVDAddress startPosition)
{
	char buff[256];
	
	//0x00001234:
	snprintf(buff, 256, "0x%.8X", startPosition.m_addr);
	uv_assert_err_ret(addComment(buff));

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::nextVectorComment(UVDAddress startPosition) 
{
	uv_err_t rcTemp = UV_ERR_GENERAL;
	UVDCPUVector *vector = NULL;
	
	rcTemp = g_uvd->m_runtime->m_architecture->getVector(&startPosition, &vector);
	if( rcTemp == UV_ERR_NOTFOUND ) {
		return UV_ERR_OK;
	}
	uv_assert_err_ret(rcTemp);
	uv_assert_ret(vector);
	//.start
	uv_assert_err_ret(addComment(UVDSprintf("Vector \"%s\"", vector->m_name.c_str())));

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::nextCalledSources(UVDAddress startPosition)
{
	char buff[256];
	std::string sNameBlock;
	UVDAnalyzedFunction analyzedFunction;
	UVDAnalyzedMemoryRange *memLoc = NULL;
	UVDAnalyzedMemorySpace calledAddresses;
	UVDConfig *config = g_uvd->m_config;

	uv_assert_err_ret(g_uvd->m_analyzer->getCalledAddresses(calledAddresses));
	if( calledAddresses.find(startPosition.m_addr) == calledAddresses.end() )
	{
		return UV_ERR_OK;
	}

	memLoc = (*(calledAddresses.find(startPosition.m_addr))).second;
	
	//empty name indicates no data
	if( !analyzedFunction.m_sName.empty() )
	{
		sNameBlock = analyzedFunction.m_sName + "(args?) ";
	}
	
	m_indexBuffer.insert(m_indexBuffer.end(), "\n");
	m_indexBuffer.insert(m_indexBuffer.end(), "\n");
	std::string formattedAddress;
	uv_assert_err_ret(g_uvd->m_format->formatAddress(startPosition.m_addr, formattedAddress));
	snprintf(buff, 256, "# FUNCTION START %s@ %s", sNameBlock.c_str(), formattedAddress.c_str());
	m_indexBuffer.insert(m_indexBuffer.end(), buff);

	//Print number of callees?
	if( config->m_calledCount )
	{
		snprintf(buff, 256, "# References: %d", memLoc->getReferenceCount());
		m_indexBuffer.push_back(buff);
	}

	//Print callees?
	if( config->m_calledSources )
	{
		uv_assert_err_ret(printReferenceList(memLoc, UVD_MEMORY_REFERENCE_CALL_DEST));
	}
	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::nextJumpedSources(UVDAddress startPosition)
{
	char buff[256];
	std::string sNameBlock;
	UVDAnalyzedMemoryRange *memLoc = NULL;
	UVDAnalyzedMemorySpace jumpedAddresses;
	UVDConfig *config = g_uvd->m_config;

	uv_assert_err_ret(g_uvd->m_analyzer->getJumpedAddresses(jumpedAddresses));
	//Can be an entry and continue point
	if( jumpedAddresses.find(startPosition.m_addr) == jumpedAddresses.end() )
	{
		return UV_ERR_OK;
	}

	memLoc = (*(jumpedAddresses.find(startPosition.m_addr))).second;
			
	std::string formattedAddress;
	uv_assert_err_ret(g_uvd->m_format->formatAddress(startPosition.m_addr, formattedAddress));
	snprintf(buff, 256, "# Jump destination %s@ %s", sNameBlock.c_str(), formattedAddress.c_str());
	m_indexBuffer.push_back(buff);

	//Print number of references?
	if( config->m_jumpedCount )
	{
		snprintf(buff, 256, "# References: %d", memLoc->getReferenceCount());
		m_indexBuffer.push_back(buff);
	}

	//Print sources?
	if( config->m_jumpedSources )
	{
		uv_assert_err_ret(printReferenceList(memLoc, UVD_MEMORY_REFERENCE_JUMP_DEST));
	}

	return UV_ERR_OK;
}

uv_err_t UVDStdPrintIterator::getAddress(UVDAddress *out) {
	return UV_DEBUG(m_iter.getAddress(out));
}

uv_err_t UVDStdPrintIterator::check() {
	UVDInstruction *instruction = NULL;
	
	//printf("Print check\n");
	
	uv_assert_ret(m_iter.m_iter);
	uv_assert_err_ret(m_iter.get(&instruction));
	//If there is an instruction we should have something to print
	//otherwise we may be at end or similar
	if( instruction ) {
		uv_assert_ret(m_positionIndex < m_indexBuffer.size());
	}
	return UV_DEBUG(m_iter.check());
}

