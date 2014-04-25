// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Color.h"
#include <SDL_stdinc.h>

const Color4f Color4f::BLACK  = Color4f(0.0f,0.0f,0.0f,1.0f);
const Color4f Color4f::WHITE  = Color4f(1.0f,1.0f,1.0f,1.0f);
const Color4f Color4f::RED    = Color4f(1.0f,0.0f,0.0f,1.0f);
const Color4f Color4f::GREEN  = Color4f(0.0f,1.0f,0.0f,1.0f);
const Color4f Color4f::BLUE   = Color4f(0.0f,0.0f,1.0f,1.0f);
const Color4f Color4f::YELLOW = Color4f(1.0f,1.0f,0.0f,1.0f);
const Color4f Color4f::GRAY   = Color4f(0.5f,0.5f,0.5f,1.f);

const Color4ub Color::BLACK   = Color(0, 0, 0, 255);
const Color4ub Color::WHITE   = Color(255, 255, 255, 255);
const Color4ub Color::RED     = Color(255, 0, 0, 255);
const Color4ub Color::GREEN   = Color(0, 255, 0, 255);
const Color4ub Color::BLUE    = Color(0, 0, 255, 255);
const Color4ub Color::YELLOW  = Color(255, 255, 0, 255);
const Color4ub Color::GRAY    = Color(128,128,128,255);

const Color3ub Color3ub::BLACK   = Color3ub(0, 0, 0);
const Color3ub Color3ub::WHITE   = Color3ub(255, 255, 255);
const Color3ub Color3ub::RED     = Color3ub(255, 0, 0);
const Color3ub Color3ub::GREEN   = Color3ub(0, 255, 0);
const Color3ub Color3ub::BLUE    = Color3ub(0, 0, 255);
const Color3ub Color3ub::YELLOW  = Color3ub(255, 255, 0);

float Color4f::GetLuminance() const
{
	return (0.299f * r) + (0.587f * g) + (0.114f * b);
}

Uint8 Color4ub::GetLuminance() const
{
	// these weights are those used for the JPEG luma channel
	return (r*299 + g*587 + b*114) / 1000;
}
