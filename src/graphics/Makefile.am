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
	opengl/glRenderer.h \
	opengl/glTexture.h \
	opengl/glMaterial.h \
	opengl/glRenderState.h \
	opengl/glRenderTarget.h \
	opengl/glVertexBuffer.h \
	opengl/FresnelColourMaterial.h \
	opengl/GasGiantMaterial.h \
	opengl/GeoSphereMaterial.h \
	opengl/MultiMaterial.h \
	opengl/Program.h \
	opengl/RingMaterial.h \
	opengl/ShieldMaterial.h \
	opengl/StarfieldMaterial.h \
	opengl/SkyboxMaterial.h \
	opengl/UIMaterial.h \
	opengl/VtxColorMaterial.h \
	opengl/Attribute.h \
	opengl/Uniform.h

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
	opengl/glRenderer.cpp \
	opengl/glTexture.cpp \
	opengl/glMaterial.cpp \
	opengl/glRenderState.cpp \
	opengl/glRenderTarget.cpp \
	opengl/glVertexBuffer.cpp \
	opengl/FresnelColourMaterial.cpp \
	opengl/GasGiantMaterial.cpp \
	opengl/GeoSphereMaterial.cpp \
	opengl/MultiMaterial.cpp \
	opengl/Program.cpp \
	opengl/ShieldMaterial.cpp \
	opengl/RingMaterial.cpp \
	opengl/UIMaterial.cpp \
	opengl/VtxColorMaterial.cpp \
	opengl/Attribute.cpp \
	opengl/Uniform.cpp
