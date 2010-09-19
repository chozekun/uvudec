/*
UVNet Universal Decompiler (uvudec)
Copyright 2010 John McMaster <JohnDMcMaster@gmail.com>
Licensed under the terms of the LGPL V3 or later, see COPYING for details
*/

#include "uvdasm/plugin.h"
#include "core/uvd.h"

UVDDisassemblerPlugin::UVDDisassemblerPlugin()
{
}

UVDDisassemblerPlugin::~UVDDisassemblerPlugin()
{
}

uv_err_t UVDDisassemblerPlugin::getName(std::string &out)
{
	out = "uvdasm";
	return UV_ERR_OK;
}

uv_err_t UVDDisassemblerPlugin::getDescription(std::string &out)
{
	out = "Configuration file based assembly engine";
	return UV_ERR_OK;
}

uv_err_t UVDDisassemblerPlugin::getVersion(UVDVersion &out)
{
	out.m_version = UVUDEC_VER_STRING;
	return UV_ERR_OK;
}
