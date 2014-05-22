include $(top_srcdir)/Makefile.common

AM_CFLAGS += $(WARN_CFLAGS)
AM_CPPFLAGS += $(WARN_CPPFLAGS)
AM_CXXFLAGS += $(STD_CXXFLAGS) $(WARN_CXXFLAGS)

AM_CPPFLAGS += -I$(srcdir)/.. -isystem $(top_srcdir)/contrib

noinst_LIBRARIES = libgraphics.a
noinst_HEADERS = \
	Graphics.h \
	WindowSDL.h \
	Renderer.h \
	RenderTarget.h \
	GLDebug.h \
	Frustum.h \
	Light.h \
	Material.h \
	RenderState.h \
	VertexArray.h \
	Texture.h \
	TextureBuilder.h \
	Drawables.h \
	Types.h \
	VertexBuffer.h \
	gl/glRenderer.h \
	gl/glTexture.h \
	gl/glMaterial.h \
	gl/glRenderState.h \
	gl/glRenderTarget.h \
	gl/glVertexBuffer.h \
	gl/FresnelColourMaterial.h \
	gl/GasGiantMaterial.h \
	gl/GeoSphereMaterial.h \
	gl/MultiMaterial.h \
	gl/Program.h \
	gl/RingMaterial.h \
	gl/ShieldMaterial.h \
	gl/StarfieldMaterial.h \
	gl/SkyboxMaterial.h \
	gl/UIMaterial.h \
	gl/VtxColorMaterial.h \
	gl/Uniform.h

libgraphics_a_SOURCES = \
	Graphics.cpp \
	WindowSDL.cpp \
	Renderer.cpp \
	Frustum.cpp \
	Light.cpp \
	Material.cpp \
	VertexArray.cpp \
	TextureBuilder.cpp \
	Drawables.cpp \
	VertexBuffer.cpp \
	gl/glRenderer.h \
	gl/glTexture.h \
	gl/glMaterial.cpp \
	gl/glRenderState.cpp \
	gl/glRenderTarget.cpp \
	gl/glVertexBuffer.cpp \
	gl/FresnelColourMaterial.cpp \
	gl/GasGiantMaterial.cpp \
	gl/GeoSphereMaterial.cpp \
	gl/MultiMaterial.cpp \
	gl/Program.cpp \
	gl/ShieldMaterial.cpp \
	gl/RingMaterial.cpp \
	gl/UIMaterial.cpp \
	gl/VtxColorMaterial.cpp \
	gl/Uniform.cpp
