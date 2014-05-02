// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "libs.h"
#include "utils.h"
#include <cstdio>
#include <cstdlib>

#include "scenegraph/SceneGraph.h"

#include "FileSystem.h"
#include "GameConfig.h"
#include "graphics/Graphics.h"
#include "graphics/Light.h"
#include "graphics/Renderer.h"
#include "graphics/Texture.h"
#include "graphics/TextureBuilder.h"
#include "graphics/Drawables.h"
#include "graphics/VertexArray.h"
#include "scenegraph/DumpVisitor.h"
#include "scenegraph/FindNodeVisitor.h"
#include "scenegraph/BinaryConverter.h"
#include "OS.h"
#include "StringF.h"
#include "ModManager.h"
#include <sstream>

void RunCompiler(const std::string &modelName)
{
	std::unique_ptr<GameConfig> config(new GameConfig);

	//init components
	FileSystem::Init();
	FileSystem::userFiles.MakeDirectory(""); // ensure the config directory exists
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		Error("SDL initialization failed: %s\n", SDL_GetError());

	ModManager::Init();

	//video
	Graphics::Settings videoSettings = {};
	videoSettings.width = config->Int("ScrWidth");
	videoSettings.height = config->Int("ScrHeight");
	videoSettings.fullscreen = (config->Int("StartFullscreen") != 0);
	videoSettings.hidden = false;
	videoSettings.requestedSamples = config->Int("AntiAliasingMode");
	videoSettings.vsync = (config->Int("VSync") != 0);
	videoSettings.useTextureCompression = (config->Int("UseTextureCompression") != 0);
	videoSettings.iconFile = OS::GetIconFilename();
	videoSettings.title = "Model viewer";
	Graphics::Renderer *renderer = Graphics::Init(videoSettings);

	//load the current model in a pristine state (no navlights, shields...)
	//and then save it into binary
	std::unique_ptr<SceneGraph::Model> model;
	try {
		SceneGraph::Loader ld(renderer);
		model.reset(ld.LoadModel(modelName));
	} catch (...) {
		//minimal error handling, this is not expected to happen since we got this far.
		return;
	}

	try {
		SceneGraph::BinaryConverter bc(renderer);
		bc.Save(modelName, model.get());
	} catch (const CouldNotOpenFileException&) {
	} catch (const CouldNotWriteToFileException&) {
	}
}


enum RunMode {
	MODE_MODELCOMPILER=0,
	MODE_VERSION,
	MODE_USAGE,
	MODE_USAGE_ERROR
};

int main(int argc, char** argv)
{
#ifdef PIONEER_PROFILER
	Profiler::detect( argc, argv );
#endif

	RunMode mode = MODE_MODELCOMPILER;

	if (argc > 1) {
		const char switchchar = argv[1][0];
		if (!(switchchar == '-' || switchchar == '/')) {
			mode = MODE_USAGE_ERROR;
			goto start;
		}

		const std::string modeopt(std::string(argv[1]).substr(1));

		if (modeopt == "compile" || modeopt == "c") {
			mode = MODE_MODELCOMPILER;
			goto start;
		}

		if (modeopt == "version" || modeopt == "v") {
			mode = MODE_VERSION;
			goto start;
		}

		if (modeopt == "help" || modeopt == "h" || modeopt == "?") {
			mode = MODE_USAGE;
			goto start;
		}

		mode = MODE_USAGE_ERROR;
	}

start:

	switch (mode) {
		case MODE_MODELCOMPILER: {
			std::string modelName;
			if (argc > 2) {
				modelName = argv[2];
				RunCompiler(modelName);
			}
			break;
		}

		case MODE_VERSION: {
			std::string version(PIONEER_VERSION);
			if (strlen(PIONEER_EXTRAVERSION)) version += " (" PIONEER_EXTRAVERSION ")";
			Output("modelcompiler %s\n", version.c_str());
			break;
		}

		case MODE_USAGE_ERROR:
			Output("modelcompiler: unknown mode %s\n", argv[1]);
			// fall through

		case MODE_USAGE:
			Output(
				"usage: modelcompiler [mode] [options...]\n"
				"available modes:\n"
				"    -compile     [-c]     model compiler\n"
				"    -version     [-v]     show version\n"
				"    -help        [-h,-?]  this help\n"
			);
			break;
	}

	return 0;
}