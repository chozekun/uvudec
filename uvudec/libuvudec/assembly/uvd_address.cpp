/*
UVNet Universal Decompiler (uvudec)
Copyright 2008 John McMaster
JohnDMcMaster@gmail.com
Licensed under the terms of the LGPL V3 or later, see COPYING for details
*/

#include "uvd_address.h"

#if 0
UVDAddressSpaceMapper::UVDAddressSpaceMapper()
{
	m_src_shared = NULL;
	m_dst_shared = NULL;
	
	m_src_min_addr = 0;
	m_src_max_addr = 0;
	m_dst_min_addr = 0;
	m_dst_max_addr = 0;
}

uv_err_t UVDAddressSpaceMapper::finalizeConfig()
{
	//Sizes in bits
	//In future may allow for slight descrepency between source and dest, but for now must be same
	uint32_t src_data_size = 0;
	uint32_t dst_data_size = 0;
	
	printf_debug("initial src; min: %d, max: %d\n", m_src_min_addr, m_src_max_addr);
	printf_debug("initial dst; min: %d, max: %d\n", m_dst_min_addr, m_dst_max_addr);

	/*
	For source, assume full range if not specified
	This is because we usually will map to a larger space
	*/
	if( m_src_max_addr == 0 )
	{
		m_src_max_addr = m_src_shared->m_max_addr;
	}
	
	uv_assert_ret(m_src_min_addr <= m_src_max_addr);
	uv_assert_ret(m_src_shared);
	uv_assert_ret(m_dst_shared);
	uv_assert_ret(m_src_shared->m_word_size);
	uv_assert_ret(m_dst_shared->m_word_size);
	src_data_size = (m_src_max_addr - m_src_min_addr + 1) * m_src_shared->m_word_size;
	
	//If destination address is 0, assume not mapped
	if( m_dst_max_addr != 0 )
	{
		uv_assert_ret(m_dst_min_addr <= m_dst_max_addr);
		uv_assert_ret(m_dst_shared);
	}
	else
	{
		//Assume equal then
		//Set max addr to min + sizeof(src) / word size
		m_dst_max_addr = m_dst_min_addr + src_data_size / m_dst_shared->m_word_size - 1;
		//Assume equal then
	}

	//Make sure in the end they are equal
	dst_data_size = (m_dst_max_addr - m_dst_min_addr + 1) * m_dst_shared->m_word_size;
	printf_debug("final src; min: %d, max: %d\n", m_src_min_addr, m_src_max_addr);
	printf_debug("final dst; min: %d, max: %d\n", m_dst_min_addr, m_dst_max_addr);
	printf_debug("word; src : %d, dst: %d\n", m_src_shared->m_word_size, m_dst_shared->m_word_size);
	printf_debug("size; src: %d, dst: %d\n", src_data_size, dst_data_size);
	uv_assert_ret(src_data_size == dst_data_size);

	return UV_ERR_OK;
}
#endif

/*
UVDAddressSpace
*/

UVDAddressSpace::UVDAddressSpace()
{
	m_type = 0;
	m_min_addr = 0;
	m_max_addr = 0;
	m_cap = 0;
	m_word_size = 0;
	m_word_alignment = 0;
}

UVDAddressSpace::~UVDAddressSpace()
{
	deinit();
}

uv_err_t UVDAddressSpace::deinit()
{
	m_synonyms.clear();
	/*
	for( std::vector<UVDAddressSpaceMapper *>::iterator iter = m_mappers.begin(); iter != m_mappers.end(); ++iter )
	{
		delete *iter;
	}
	m_mappers.clear();
	*/
	
	return UV_ERR_OK;
}

uv_err_t UVDAddressSpace::setEquivMemName(uint32_t addr, const std::string &name)
{
	printf_debug("setEquivMemName: %s(0x%.8X) = %s\n", m_name.c_str(), addr, name.c_str());
	m_synonyms[addr] = name;
	return UV_ERR_OK;
}

uv_err_t UVDAddressSpace::getEquivMemName(uint32_t addr, std::string &name)
{
	if( m_synonyms.find(addr) == m_synonyms.end() )
	{
		return UV_ERR_GENERAL;
	}
	name = m_synonyms[addr];
	return UV_ERR_OK;
}

/*
UVDAddress
*/

UVDAddress::UVDAddress()
{
	m_space = NULL;
	m_addr = 0;
}

UVDAddress::UVDAddress(uv_addr_t addr, UVDAddressSpace *space)
{
	m_space = space;
	m_addr = addr;
}

bool UVDAddress::intersects(const UVDAddressRange &range) const
{
	return UVDAddressRange(m_addr, m_addr, m_space).intersects(range);
}

//By minimum address
int UVDAddress::compare(const UVDAddress *other) const
{
	//Checked builds only
	if( other == NULL )
	{
		return 1;
	}
	
	//We assume they are in the same address space or the comparison doesn't make sense
	return m_addr - other->m_addr;
}

bool UVDAddress::operator<(const UVDAddress *other) const
{
	return compare(other) < 0;
}

bool UVDAddress::operator>(const UVDAddress *other) const
{
	return compare(other) > 0;
}

bool UVDAddress::operator==(const UVDAddress *other) const
{
	return compare(other) == 0;
}

/*
UVDAddressRange
*/

UVDAddressRange::UVDAddressRange()
{
	m_min_addr = 0;
	m_max_addr = 0;
	m_space = NULL;
}

UVDAddressRange::UVDAddressRange(uv_addr_t min_addr)
{
	m_min_addr = min_addr;
	m_max_addr = min_addr;
	m_space = NULL;
}

UVDAddressRange::UVDAddressRange(uv_addr_t min_addr, uv_addr_t max_addr, UVDAddressSpace *space)
{
	m_min_addr = min_addr;
	m_max_addr = max_addr;
	m_space = space;
}

bool UVDAddressRange::intersects(const UVDAddressRange &other) const
{
	//A aaaa B abababa A bbbbb B
	return (other.m_min_addr <= m_max_addr && other.m_max_addr >= m_min_addr)
			|| (m_min_addr <= other.m_max_addr && m_max_addr >= other.m_min_addr);
}

int UVDAddressRange::compareStatic(const UVDAddressRange *l, const UVDAddressRange *r) 
{
	if( l == r )
	{
		return 0;
	}
	if( r == NULL )
	{
		return -1;
	}
	if( l == NULL )
	{
		return 1;
	}
	
	return r->m_min_addr - l->m_min_addr;
}

int UVDAddressRange::compare(const UVDAddressRange *other) const
{
	return compareStatic(this, other);
}

bool UVDAddressRange::operator<(const UVDAddressRange *other) const
{
	return compare(other) < 0;
}

bool UVDAddressRange::operator>(const UVDAddressRange *other) const
{
	return compare(other) > 0;
}

bool UVDAddressRange::operator==(const UVDAddressRange *other) const
{
	return compare(other) == 0;
}

