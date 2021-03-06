/*
UVNet Universal Decompiler (uvudec)
Copyright 2008 John McMaster <JohnDMcMaster@gmail.com>
Licensed under the terms of the LGPL V3 or later, see COPYING for details

uvudec entry point
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "uvd/config/arg_property.h"
#include "uvd/config/arg_util.h"
#include "uvd/util/error.h"
#include "uvd/core/init.h"
#include "uvd/util/util.h"
#include "uvd/core/uvd.h"
#include "uvd/data/data.h"

/*
During parse, several things can happen:
-Exact opcode match
-A prefix
-A multibyte opcode

Before this was handled by function pointers and corresponding opcode table
Seems a solid architecture, should stick with it
*/

//typedef uv_err_t (*uv_disasm_func)(struct );

#define UVD_PROP_INIT_ONLY			"uvudec.debug.init_only"
#define UVD_PROP_INIT_ONLY_DEFAULT	false

static std::string g_outputFile;
static FILE *g_pOutputFile = NULL;
//UVD_PROP_INIT_ONLY
static uvd_bool_t g_initOnly = UVD_PROP_INIT_ONLY_DEFAULT;

uv_err_t versionPrintPrefixThunk();

static uv_err_t runTasks()
{
	uv_err_t rc = UV_ERR_GENERAL;
	std::string output;
	UVD *uvd = NULL;
	UVDData *data = NULL;

	printf_debug_level(UVD_DEBUG_PASSES, "main: initializing data streams\n");

	uv_assert_ret(g_config);
	if( g_config->m_targetFileName.empty() )
	{
		printf_error("Target file not specified\n");
		g_config->printHelp();
		uv_assert(UV_ERR_GENERAL);
	}

	//Select input
	printf_debug_level(UVD_DEBUG_SUMMARY, "Initializing data stream on %s...\n", g_config->m_targetFileName.c_str());
	if( UV_FAILED(UVDDataFile::getUVDDataFile(&data, g_config->m_targetFileName)) )
	{
		printf_error("Failed to initialize data stream!\n");
		printf_error("Could not read file: %s\n", g_config->m_targetFileName.c_str());
		return UV_DEBUG(UV_ERR_GENERAL);
	}
	uv_assert(data);
	
	//Create a runTasksr engine active on that input
	printf_debug_level(UVD_DEBUG_SUMMARY, "runTasks: initializing engine...\n");
	if( UV_FAILED(UVD::getUVDFromData(&uvd, data)) )
	{
		printf_error("Failed to initialize engine\n");
		uv_assert_err(UV_ERR_GENERAL);
	}
	uv_assert(uvd);
	uv_assert(g_uvd);

	if( !g_initOnly )
	{
		if( g_config->m_analysisOnly )
		{
			uv_assert_err_ret(uvd->analyze());
		}
		else
		{
			//Get string output
			printf_debug_level(UVD_DEBUG_SUMMARY, "Disassembling...\n");
			rc = uvd->disassembleByCallback(UVDPrintToFileStringCallback, g_pOutputFile);
			if( UV_FAILED(rc) )
			{
				printf_error("Failed to runTasks!\n");
				uv_assert_err(UV_ERR_GENERAL);
			}
		}
	}

	printf_debug_level(UVD_DEBUG_PASSES, "main: runTasksd\n");
	rc = UV_ERR_OK;
	
error:
	delete data;
	return rc;
}

static uv_err_t argParser(const UVDArgConfig *argConfig, std::vector<std::string> argumentArguments, void *user)
{
	UVDConfig *config = NULL;
	//If present
	std::string firstArg;
	uint32_t firstArgNum = 0;
	bool firstArgBool = true;
	
	config = g_config;
	uv_assert_ret(config);
	uv_assert_ret(config->m_argv);
	uv_assert_ret(argConfig);

	if( !argumentArguments.empty() )
	{
		firstArg = argumentArguments[0];
		firstArgNum = strtol(firstArg.c_str(), NULL, 0);
		firstArgBool = UVDArgToBool(firstArg);
	}

	if( argConfig->isNakedHandler() )
	{
		/*
		Try to guess type based on file suffixes
		*/
		
		if( argumentArguments.size() >= 1 )
		{
			config->m_targetFileName = argumentArguments[0];
		}
		if( argumentArguments.size() >= 2 )
		{
			g_outputFile = argumentArguments[1];
		}
	}
	else if( argConfig->m_propertyForm == UVD_PROP_TARGET_FILE )
	{
		uv_assert_ret(!argumentArguments.empty());
		config->m_targetFileName = firstArg;
	}
	else if( argConfig->m_propertyForm == UVD_PROP_OUTPUT_FILE )
	{
		uv_assert_ret(!argumentArguments.empty());
		g_outputFile = firstArg;
	}
	else if( argConfig->m_propertyForm == UVD_PROP_INIT_ONLY )
	{
		g_initOnly = firstArgBool;
	}
	else
	{
		//return UV_DEBUG(argParserDefault(argConfig, argumentArguments));
		return UV_DEBUG(UV_ERR_GENERAL);
	}

	return UV_ERR_OK;
}

uv_err_t initProgConfig()
{
	uv_assert_ret(g_config);
	
	//Arguments
	uv_assert_err_ret(g_config->registerDefaultArgument(argParser, " [input binary, analyzed program]"));
	uv_assert_err_ret(g_config->registerArgument(UVD_PROP_TARGET_FILE, 0, "input", "source file for data", 1, argParser, false));
	uv_assert_err_ret(g_config->registerArgument(UVD_PROP_OUTPUT_FILE, 0, "output", "output program (default: stdout)", 1, argParser, false));
	uv_assert_err_ret(g_config->registerArgument(UVD_PROP_INIT_ONLY, 0, "init-only", "only initialize, don't do anything (to debug plugin load selection)", 1, argParser, true));

	//Callbacks
	g_config->versionPrintPrefixThunk = versionPrintPrefixThunk;

	g_outputFile = "/dev/stdout";

	return UV_ERR_OK;	
}

uv_err_t versionPrintPrefixThunk()
{
	const char *program_name = "uvudec";
	
	/*
	if( g_config && g_config->m_argv )
	{
		program_name = g_config->m_argv[0];
	}
	*/

	printf_help("%s version %s\n", program_name, UVUDEC_VER_STRING);
	return UV_ERR_OK;
}

uv_err_t uvmain(int argc, char **argv)
{
	uv_err_t rc = UV_ERR_GENERAL;
	UVDConfig *config = NULL;
	uv_err_t parseMainRc = UV_ERR_GENERAL;
	
	UVD_WARN_IF_VERSION_MISMATCH();
	
	//Early library initialization.  Logging and arg parsing structures
	uv_assert_err_ret(UVDInit());

	config = g_config;
	uv_assert_ret(config);
	//Early local initialization
	uv_assert_err_ret(initProgConfig());
	
	//Grab our command line options
	parseMainRc = config->parseMain(argc, argv);
	uv_assert_err_ret(parseMainRc);
	if( parseMainRc == UV_ERR_DONE )
	{
		rc = UV_ERR_OK;
		goto error;
	}

	if( UV_FAILED(parseFileOption(g_outputFile, &g_pOutputFile)) )
	{
		printf_error("Could not open file: %s\n", g_outputFile.c_str());
		uv_assert(UV_ERR_GENERAL);
	}

	if( UV_FAILED(runTasks()) )
	{
		printf_error("Top level runTasks failed\n");
		uv_assert(UV_ERR_GENERAL);
	}	

	rc = UV_ERR_OK;

error:
	uv_assert_err_ret(UVDDeinit());
		
	return UV_DEBUG(rc);
}

int main(int argc, char **argv)
{
	printf_debug("main: enter\n");

	//Simple translation to keep most stuff in the framework
	uv_err_t rc = uvmain(argc, argv);
	printf_debug("main: exit\n");
	if( UV_FAILED(rc) )
	{
		printf_error("failed\n");
		return 1;
	}
	else
	{
		return 0;
	}
}

