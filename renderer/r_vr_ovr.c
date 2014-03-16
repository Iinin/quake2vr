#include "r_vr_ovr.h"
#include "../vr/vr_ovr.h"
#include "../renderer/r_local.h"
#include "r_vr.h"


void OVR_FrameStart(Sint32 changeBackBuffers);
void OVR_BindView(vr_eye_t eye);
void OVR_GetViewRect(vr_eye_t eye, vr_rect_t *rect);
void OVR_Present();
Sint32 OVR_Enable();
void OVR_Disable();
Sint32 OVR_Init();
void OVR_GetState(vr_param_t *state);

hmd_render_t vr_render_ovr = 
{
	HMD_RIFT,
	OVR_Init,
	OVR_Enable,
	OVR_Disable,
	OVR_BindView,
	OVR_GetViewRect,
	OVR_FrameStart,
	OVR_GetState,
	OVR_Present
};


extern void VR_OVR_CalcRenderParam();
extern float VR_OVR_GetDistortionScale();
extern void VR_OVR_GetFOV(float *fovx, float *fovy);
extern Sint32 VR_OVR_RenderLatencyTest(vec4_t color);


//static fbo_t world;
static fbo_t left, right;

static vr_rect_t renderTargetRect;

static unsigned useBilinear;

// Default Lens Warp Shader
static r_shaderobject_t ovr_shader_norm = {
	0, 
	
	// vertex shader (identity)
	"varying vec2 texCoords;\n"
	"void main(void) {\n"
		"gl_Position = gl_Vertex;\n"
		"texCoords = gl_MultiTexCoord0.xy;\n"
	"}\n",
	// fragment shader
	"varying vec2 texCoords;\n"
	"uniform vec2 scale;\n"
	"uniform vec2 screenCenter;\n"
	"uniform vec2 lensCenter;\n"
	"uniform vec2 scaleIn;\n"
	"uniform vec4 hmdWarpParam;\n"
	"uniform sampler2D texture;\n"
	"vec2 warp(vec2 uv)\n"
	"{\n"
		"vec2 theta = (vec2(uv) - lensCenter) * scaleIn;\n"
		"float rSq = theta.x*theta.x + theta.y*theta.y;\n"
		"vec2 rvector = theta*(hmdWarpParam.x + hmdWarpParam.y*rSq + hmdWarpParam.z*rSq*rSq + hmdWarpParam.w*rSq*rSq*rSq);\n"
		"return (lensCenter + scale * rvector);\n"
	"}\n"
	"void main()\n"
	"{\n"
		"vec2 tc = warp(texCoords);\n"
		"if (any(bvec2(clamp(tc, screenCenter - vec2(0.5,0.5), screenCenter + vec2(0.5,0.5))-tc)))\n"
			"gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"else\n"
			"gl_FragColor = texture2D(texture, tc);\n"
	"}\n"

};

// Default Lens Warp Shader
static r_shaderobject_t ovr_shader_bicubic_norm = {
	0,

	// vertex shader (identity)
	"varying vec2 texCoords;\n"
	"void main(void) {\n"
	"gl_Position = gl_Vertex;\n"
	"texCoords = gl_MultiTexCoord0.xy;\n"
	"}\n",

	// fragment shader
	"varying vec2 texCoords;\n"
	"uniform vec2 scale;\n"
	"uniform vec2 screenCenter;\n"
	"uniform vec2 lensCenter;\n"
	"uniform vec2 scaleIn;\n"
	"uniform vec4 textureSize;\n"
	"uniform vec4 hmdWarpParam;\n"
	"uniform sampler2D texture;\n"
	// hack to fix artifacting with AMD cards
	"vec3 sampleClamp(sampler2D tex, vec2 uv)\n"
	"{\n"
		"return texture2D(tex,clamp(uv,vec2(0.0),vec2(1.0))).rgb;\n"
	"}\n"
	"vec2 warp(vec2 uv)\n"
	"{\n"
		"vec2 theta = (vec2(uv) - lensCenter) * scaleIn;\n"
		"float rSq = theta.x*theta.x + theta.y*theta.y;\n"
		"vec2 rvector = theta*(hmdWarpParam.x + hmdWarpParam.y*rSq + hmdWarpParam.z*rSq*rSq + hmdWarpParam.w*rSq*rSq*rSq);\n"
		"return (lensCenter + scale * rvector);\n"
	"}\n"
	"vec3 filter(sampler2D texture, vec2 texCoord)\n"
	"{\n"
		// calculate the center of the texel
		"texCoord *= textureSize.xy;\n"
		"vec2 texelCenter   = floor( texCoord - 0.5 ) + 0.5;\n"
		"vec2 fracOffset    = texCoord - texelCenter;\n"
		"vec2 fracOffset_x2 = fracOffset * fracOffset;\n"
		"vec2 fracOffset_x3 = fracOffset * fracOffset_x2;\n"
		// calculate bspline weight function
		"vec2 weight0 = fracOffset_x2 - 0.5 * ( fracOffset_x3 + fracOffset );\n"
		"vec2 weight1 = 1.5 * fracOffset_x3 - 2.5 * fracOffset_x2 + 1.0;\n"
		"vec2 weight3 = 0.5 * ( fracOffset_x3 - fracOffset_x2 );\n"
		"vec2 weight2 = 1.0 - weight0 - weight1 - weight3;\n"
		// calculate texture coordinates
		"vec2 scalingFactor0 = weight0 + weight1;\n"
		"vec2 scalingFactor1 = weight2 + weight3;\n"
		"vec2 f0 = weight1 / ( weight0 + weight1 );\n"
		"vec2 f1 = weight3 / ( weight2 + weight3 );\n"
		"vec2 texCoord0 = texelCenter - 1.0 + f0;\n"
		"vec2 texCoord1 = texelCenter + 1.0 + f1;\n"
		"texCoord0 *= textureSize.zw;\n"
		"texCoord1 *= textureSize.zw;\n"
		// sample texture and apply weighting
		"return sampleClamp( texture, (vec2( texCoord0.x, texCoord0.y )) ) * scalingFactor0.x * scalingFactor0.y +\n"
			   "sampleClamp( texture, (vec2( texCoord1.x, texCoord0.y )) ) * scalingFactor1.x * scalingFactor0.y +\n"
			   "sampleClamp( texture, (vec2( texCoord0.x, texCoord1.y )) ) * scalingFactor0.x * scalingFactor1.y +\n"
			   "sampleClamp( texture, (vec2( texCoord1.x, texCoord1.y )) ) * scalingFactor1.x * scalingFactor1.y;\n"
	"}\n"
	"void main()\n"
	"{\n"
		"vec2 tc = warp(texCoords);\n"
		"if (!all(equal(clamp(tc, screenCenter - vec2(0.5,0.5), screenCenter + vec2(0.5,0.5)),tc)))\n"
			"gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);\n"
		"else\n"
			// have to reapply warping for the bicubic sampling
			"gl_FragColor = vec4(filter(texture, tc),1.0);\n"
	"}\n"
};


// Lens Warp Shader with Chromatic Aberration 
static r_shaderobject_t ovr_shader_chrm = {
	0, 
	
	// vertex shader (identity)
	"varying vec2 theta;\n"
	"uniform vec2 lensCenter;\n"
	"uniform vec2 scaleIn;\n"
	"void main(void) {\n"
		"gl_Position = gl_Vertex;\n"
		"theta = (vec2(gl_MultiTexCoord0) - lensCenter) * scaleIn;\n"
	"}\n",

	// fragment shader
	"varying vec2 theta;\n"
	"uniform vec2 lensCenter;\n"
	"uniform vec2 scale;\n"
	"uniform vec2 screenCenter;\n"
	"uniform vec4 hmdWarpParam;\n"
	"uniform vec4 chromAbParam;\n"
	"uniform sampler2D texture;\n"

	// Scales input texture coordinates for distortion.
	// ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
	// larger due to aspect ratio.
	"void main()\n"
	"{\n"
		"float rSq = theta.x*theta.x + theta.y*theta.y;\n"
		"vec2 theta1 = theta*(hmdWarpParam.x + hmdWarpParam.y*rSq + hmdWarpParam.z*rSq*rSq + hmdWarpParam.w*rSq*rSq*rSq);\n"

		// Detect whether blue texture coordinates are out of range since these will scaled out the furthest.
		"vec2 thetaBlue = theta1 * (chromAbParam.z + chromAbParam.w * rSq);\n"
		"vec2 tcBlue = lensCenter + scale * thetaBlue;\n"

		"if (!all(equal(clamp(tcBlue, screenCenter - vec2(0.5,0.5), screenCenter + vec2(0.5,0.5)),tcBlue)))\n"
		"{\n"
			"gl_FragColor = vec4(0.0,0.0,0.0,1.0);\n"
			"return;\n"
		"}\n"

		// Now do blue texture lookup.
		"float blue = texture2D(texture, tcBlue).b;\n"

		// Do green lookup (no scaling).
		"vec2  tcGreen = lensCenter + scale * theta1;\n"

		"float green = texture2D(texture, tcGreen).g;\n"

		// Do red scale and lookup.
		"vec2  thetaRed = theta1 * (chromAbParam.x + chromAbParam.y * rSq);\n"
		"vec2  tcRed = lensCenter + scale * thetaRed;\n"

		"float red = texture2D(texture, tcRed).r;\n"

		"gl_FragColor = vec4(red, green, blue, 1.0);\n"
	"}\n"
};

static r_shaderobject_t ovr_shader_bicubic_chrm = {
	0,
	
	// vertex shader (identity)
	"varying vec2 theta;\n"
	"uniform vec2 lensCenter;\n"
	"uniform vec2 scaleIn;\n"
	"void main(void) {\n"
		"gl_Position = gl_Vertex;\n"
		"theta = (vec2(gl_MultiTexCoord0) - lensCenter) * scaleIn;\n"
	"}\n",

	// fragment shader
	"varying vec2 theta;\n"
	"uniform vec2 lensCenter;\n"
	"uniform vec2 scale;\n"
	"uniform vec2 screenCenter;\n"
	"uniform vec4 hmdWarpParam;\n"
	"uniform vec4 chromAbParam;\n"
	"uniform vec4 textureSize;\n"
	"uniform sampler2D texture;\n"
	// hack to fix artifacting with AMD cards
	// not necessary for chromatic
	"vec3 sampleClamp(sampler2D tex, vec2 uv)\n"
	"{\n"
		"return texture2D(tex,clamp(uv,vec2(0.0),vec2(1.0))).rgb;\n"
	"}\n"
	"vec3 filter(sampler2D texture, vec2 texCoord)\n"
	"{\n"
		// calculate the center of the texel
		"texCoord *= textureSize.xy;\n"
		"vec2 texelCenter   = floor( texCoord - 0.5 ) + 0.5;\n"
		"vec2 fracOffset    = texCoord - texelCenter;\n"
		"vec2 fracOffset_x2 = fracOffset * fracOffset;\n"
		"vec2 fracOffset_x3 = fracOffset * fracOffset_x2;\n"
		// calculate bspline weight function
		"vec2 weight0 = fracOffset_x2 - 0.5 * ( fracOffset_x3 + fracOffset );\n"
		"vec2 weight1 = 1.5 * fracOffset_x3 - 2.5 * fracOffset_x2 + 1.0;\n"
		"vec2 weight3 = 0.5 * ( fracOffset_x3 - fracOffset_x2 );\n"
		"vec2 weight2 = 1.0 - weight0 - weight1 - weight3;\n"
		// calculate texture coordinates
		"vec2 scalingFactor0 = weight0 + weight1;\n"
		"vec2 scalingFactor1 = weight2 + weight3;\n"
		"vec2 f0 = weight1 / ( weight0 + weight1 );\n"
		"vec2 f1 = weight3 / ( weight2 + weight3 );\n"
		"vec2 texCoord0 = texelCenter - 1.0 + f0;\n"
		"vec2 texCoord1 = texelCenter + 1.0 + f1;\n"
		"texCoord0 *= textureSize.zw;\n"
		"texCoord1 *= textureSize.zw;\n"
		// sample texture and apply weighting
		"return sampleClamp( texture, (vec2( texCoord0.x, texCoord0.y )) ) * scalingFactor0.x * scalingFactor0.y +\n"
			   "sampleClamp( texture, (vec2( texCoord1.x, texCoord0.y )) ) * scalingFactor1.x * scalingFactor0.y +\n"
			   "sampleClamp( texture, (vec2( texCoord0.x, texCoord1.y )) ) * scalingFactor0.x * scalingFactor1.y +\n"
			   "sampleClamp( texture, (vec2( texCoord1.x, texCoord1.y )) ) * scalingFactor1.x * scalingFactor1.y;\n"
	"}\n"

	// Scales input texture coordinates for distortion.
	// ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
	// larger due to aspect ratio.
		"void main()\n"
	"{\n"
		"float rSq = theta.x*theta.x + theta.y*theta.y;\n"
		"vec2 theta1 = theta*(hmdWarpParam.x + hmdWarpParam.y*rSq + hmdWarpParam.z*rSq*rSq + hmdWarpParam.w*rSq*rSq*rSq);\n"

		// Detect whether blue texture coordinates are out of range since these will scaled out the furthest.
		"vec2 thetaBlue = theta1 * (chromAbParam.z + chromAbParam.w * rSq);\n"
		"vec2 tcBlue = lensCenter + scale * thetaBlue;\n"

		"if (!all(equal(clamp(tcBlue, screenCenter - vec2(0.5,0.5), screenCenter + vec2(0.5,0.5)),tcBlue)))\n"
		"{\n"
			"gl_FragColor = vec4(0.0,0.0,0.0,1.0);\n"
			"return;\n"
		"}\n"

		// Now do blue texture lookup.
		"float blue = filter(texture, tcBlue).b;\n"

		// Do green lookup (no scaling).
		"vec2  tcGreen = lensCenter + scale * theta1;\n"

		"float green = filter(texture, tcGreen).g;\n"

		// Do red scale and lookup.
		"vec2  thetaRed = theta1 * (chromAbParam.x + chromAbParam.y * rSq);\n"
		"vec2  tcRed = lensCenter + scale * thetaRed;\n"

		"float red = filter(texture, tcRed).r;\n"

		"gl_FragColor = vec4(red, green, blue, 1.0);\n"
	"}\n"

};


typedef struct {
	r_shaderobject_t *shader;
	struct {
		GLuint scale;
		GLuint scale_in;
		GLuint lens_center;
		GLuint screen_center;
		GLuint texture_size;
		GLuint hmd_warp_param;
		GLuint chrom_ab_param;
	} uniform;

} r_ovr_shader_t;

typedef enum {
	OVR_FILTER_BILINEAR,
	OVR_FILTER_WEIGHTED_BILINEAR,
	OVR_FILTER_BICUBIC,
	NUM_OVR_FILTER_MODES
} ovr_filtermode_t;

static r_ovr_shader_t ovr_shaders[2];
static r_ovr_shader_t ovr_bicubic_shaders[2];

// util function
void VR_OVR_InitShader(r_ovr_shader_t *shader, r_shaderobject_t *object)
{

	if (!object->program)
		R_CompileShaderProgram(object);

	shader->shader = object;
	glUseProgram(shader->shader->program);

	shader->uniform.scale = glGetUniformLocation(shader->shader->program, "scale");
	shader->uniform.scale_in = glGetUniformLocation(shader->shader->program, "scaleIn");
	shader->uniform.lens_center = glGetUniformLocation(shader->shader->program, "lensCenter");
	shader->uniform.screen_center = glGetUniformLocation(shader->shader->program, "screenCenter");
	shader->uniform.hmd_warp_param = glGetUniformLocation(shader->shader->program, "hmdWarpParam");
	shader->uniform.chrom_ab_param = glGetUniformLocation(shader->shader->program, "chromAbParam");
	shader->uniform.texture_size = glGetUniformLocation(shader->shader->program,"textureSize");
	glUseProgram(0);
}



void OVR_FrameStart(Sint32 changeBackBuffers)
{


	if (vr_ovr_distortion->modified)
	{
		Cvar_SetInteger("vr_ovr_distortion", !!(Sint32) vr_ovr_distortion->value);
		changeBackBuffers = 1;
		vr_ovr_distortion->modified = false;
	}

	if (vr_ovr_scale->modified)
	{
		if (vr_ovr_scale->value < 1.0)
			Cvar_Set("vr_ovr_scale", "1.0");
		else if (vr_ovr_scale->value > 2.0)
			Cvar_Set("vr_ovr_scale", "2.0");
		changeBackBuffers = 1;
		vr_ovr_scale->modified = false;
	}

	if (vr_ovr_autoscale->modified)
	{
		if (vr_ovr_autoscale->value < 0.0)
			Cvar_SetInteger("vr_ovr_autoscale", 0);
		else if (vr_ovr_autoscale->value > 2.0)
			Cvar_SetInteger("vr_ovr_autoscale",2);
		else
			Cvar_SetInteger("vr_ovr_autoscale", (Sint32) vr_ovr_autoscale->value);
		changeBackBuffers = 1;
		vr_ovr_autoscale->modified = false;
	}

	if (vr_ovr_lensdistance->modified)
	{
		if (vr_ovr_lensdistance->value < 0.0)
			Cvar_SetToDefault("vr_ovr_lensdistance");
		else if (vr_ovr_lensdistance->value > 100.0)
			Cvar_Set("vr_ovr_lensdistance", "100.0");
		changeBackBuffers = 1;
		VR_OVR_CalcRenderParam();
		vr_ovr_lensdistance->modified = false;
	}

	if (vr_ovr_autolensdistance->modified)
	{
		Cvar_SetValue("vr_ovr_autolensdistance",!!vr_ovr_autolensdistance->value);
		changeBackBuffers = 1;
		VR_OVR_CalcRenderParam();
		vr_ovr_autolensdistance->modified = false;
	}

	if (vr_ovr_supersample->modified)
	{
		if (vr_ovr_supersample->value < 1.0)
			Cvar_Set("vr_ovr_supersample", "1.0");
		else if (vr_ovr_supersample->value > 2.0)
			Cvar_Set("vr_ovr_supersample", "2.0");
		changeBackBuffers = 1;
		vr_ovr_supersample->modified = false;
	}

	if (vr_ovr_filtermode->modified)
	{
		switch ((Sint32) vr_ovr_filtermode->value)
		{
		default:
		case OVR_FILTER_BILINEAR:
		case OVR_FILTER_BICUBIC:
			useBilinear = 1;
			break;
		case OVR_FILTER_WEIGHTED_BILINEAR:
			useBilinear = 0;
			break;
		}
//		Com_Printf("OVR: %s bilinear texture filtering\n", useBilinear ? "Using" : "Not using");
		R_SetFBOFilter(useBilinear,&left);
		R_SetFBOFilter(useBilinear,&right);
		vr_ovr_filtermode->modified = false;
	}

	if (changeBackBuffers)
	{
		float ovrScale = VR_OVR_GetDistortionScale() *  vr_ovr_supersample->value;
		renderTargetRect.width = ovrScale * vid.width * 0.5;
		renderTargetRect.height = ovrScale  * vid.height;
		Com_Printf("OVR: Set render target size to %ux%u\n",renderTargetRect.width,renderTargetRect.height);

	}
}


void OVR_BindView(vr_eye_t eye)
{
	
	switch(eye)
	{
	case EYE_LEFT:
		if (renderTargetRect.width != left.width || renderTargetRect.height != left.height)
		{
			R_ResizeFBO(renderTargetRect.width, renderTargetRect.height, useBilinear, &left);
	//		Com_Printf("OVR: Set left render target size to %ux%u\n",left.width,left.height);
		}
		vid.height = left.height;
		vid.width = left.width;
		R_BindFBO(&left);
		break;
	case EYE_RIGHT:
		if (renderTargetRect.width != right.width || renderTargetRect.height != right.height)
		{
			R_ResizeFBO(renderTargetRect.width, renderTargetRect.height, useBilinear, &right);
	//		Com_Printf("OVR: Set right render target size to %ux%u\n",right.width, right.height);
		}
		vid.height = right.height;
		vid.width = right.width;
		R_BindFBO(&right);
		break;
	default:
		return;
	}
}

void OVR_GetViewRect(vr_eye_t eye, vr_rect_t *rect)
{
	*rect = renderTargetRect;
}


void OVR_GetState(vr_param_t *state)
{
		vr_param_t ovrState;
		float ovrScale = VR_OVR_GetDistortionScale() *  vr_ovr_supersample->value;
		VR_OVR_GetFOV(&ovrState.viewFovX, &ovrState.viewFovY);
		ovrState.projOffset = ovrConfig.projOffset;
		ovrState.ipd = ovrConfig.ipd;
		ovrState.pixelScale = ovrScale * vid.width / (Sint32) ovrConfig.hmdWidth;
		ovrState.aspect = ovrConfig.aspect;
		*state = ovrState;
}

void OVR_Present()
{
	vec4_t debugColor;

	if (vr_ovr_distortion->value)
	{

		float scale = VR_OVR_GetDistortionScale();
		float superscale = vr_ovr_supersample->value;
		r_ovr_shader_t *current_shader;
		if (vr_ovr_filtermode->value)
			current_shader = &ovr_bicubic_shaders[!!(Sint32) vr_chromatic->value];
		else
			current_shader = &ovr_shaders[!!(Sint32) vr_chromatic->value];

		// draw left eye
		glUseProgram(current_shader->shader->program);

		glUniform4fv(current_shader->uniform.chrom_ab_param, 1, ovrConfig.chrm);
		glUniform4fv(current_shader->uniform.hmd_warp_param, 1, ovrConfig.dk);
		glUniform2f(current_shader->uniform.scale_in, 2.0f, 2.0f / ovrConfig.aspect);
		glUniform2f(current_shader->uniform.scale, 0.5f / scale, 0.5f * ovrConfig.aspect / scale);
		glUniform4f(current_shader->uniform.texture_size, renderTargetRect.width / superscale, renderTargetRect.height / superscale, superscale / renderTargetRect.width, superscale / renderTargetRect.height);
	
		glUniform2f(current_shader->uniform.lens_center, 0.5 + vrState.projOffset * 0.5, 0.5);
		glUniform2f(current_shader->uniform.screen_center, 0.5 , 0.5);
		GL_MBind(0,left.texture);

		glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0, 0); glVertex2f(-1, -1);
		glTexCoord2f(0, 1); glVertex2f(-1, 1);
		glTexCoord2f(1, 0); glVertex2f(0, -1);
		glTexCoord2f(1, 1); glVertex2f(0, 1);
		glEnd();

		// draw right eye
		glUniform2f(current_shader->uniform.lens_center, 0.5 - vrState.projOffset * 0.5, 0.5 );
		glUniform2f(current_shader->uniform.screen_center, 0.5 , 0.5);
		GL_MBind(0,right.texture);

		glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0, 0); glVertex2f(0, -1);
		glTexCoord2f(0, 1); glVertex2f(0, 1);
		glTexCoord2f(1, 0); glVertex2f(1, -1);
		glTexCoord2f(1, 1); glVertex2f(1, 1);
		glEnd();
		glUseProgram(0);
		
		GL_MBind(0,0);

	} else {
		GL_MBind(0,left.texture);


		glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0, 0); glVertex2f(-1, -1);
		glTexCoord2f(0, 1); glVertex2f(-1, 1);
		glTexCoord2f(1, 0); glVertex2f(0, -1);
		glTexCoord2f(1, 1); glVertex2f(0, 1);
		glEnd();

		GL_MBind(0,right.texture);

		glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0, 0); glVertex2f(0, -1);
		glTexCoord2f(0, 1); glVertex2f(0, 1);
		glTexCoord2f(1, 0); glVertex2f(1, -1);
		glTexCoord2f(1, 1); glVertex2f(1, 1);
		glEnd();
		GL_MBind(0,0);
	}
	if (VR_OVR_RenderLatencyTest(debugColor))
	{
		glColor4fv(debugColor);

		glBegin(GL_TRIANGLE_STRIP);
		glVertex2f(0.3, -0.4);
		glVertex2f(0.3, 0.4);
		glVertex2f(0.7, -0.4);
		glVertex2f(0.7, 0.4); 
		glEnd();

		glBegin(GL_TRIANGLE_STRIP);
		glVertex2f(-0.3, -0.4);
		glVertex2f(-0.3, 0.4);
		glVertex2f(-0.7, -0.4);
		glVertex2f(-0.7, 0.4); 
		glEnd();

		glColor4f(1.0,1.0,1.0,1.0);
	}

}

Sint32 OVR_Enable()
{
	if (left.valid)
		R_DelFBO(&left);
	if (right.valid)
		R_DelFBO(&right);

	VR_OVR_InitShader(&ovr_shaders[0],&ovr_shader_norm);
	VR_OVR_InitShader(&ovr_shaders[1],&ovr_shader_chrm);
	VR_OVR_InitShader(&ovr_bicubic_shaders[0],&ovr_shader_bicubic_norm);
	VR_OVR_InitShader(&ovr_bicubic_shaders[1],&ovr_shader_bicubic_chrm);

//	OVR_FrameStart(true);

	return true;
}

void OVR_Disable()
{
	R_DelShaderProgram(&ovr_shader_norm);
	R_DelShaderProgram(&ovr_shader_chrm);
	R_DelShaderProgram(&ovr_shader_bicubic_norm);
	R_DelShaderProgram(&ovr_shader_bicubic_chrm);

	if (left.valid)
		R_DelFBO(&left);
	if (right.valid)
		R_DelFBO(&right);
}

Sint32 OVR_Init()
{
	R_InitFBO(&left);
	R_InitFBO(&right);
	return true;
}