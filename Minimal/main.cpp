/************************************************************************************

Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
Copyright   :   Copyright Brad Davis. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include <iostream>
#include <memory>
#include <exception>
#include <algorithm>

#include <Windows.h>

#define __STDC_FORMAT_MACROS 1

#define FAIL(X) throw std::runtime_error(X)

///////////////////////////////////////////////////////////////////////////////
//
// GLM is a C++ math library meant to mirror the syntax of GLSL 
//

#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "Skybox.h"
#include "Model.h"

// Import the most commonly used types into the default namespace
using glm::ivec3;
using glm::ivec2;
using glm::uvec2;
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::quat;

///////////////////////////////////////////////////////////////////////////////
//
// GLEW gives cross platform access to OpenGL 3.x+ functionality.  
//

#include <GL/glew.h>

bool checkFramebufferStatus(GLenum target = GL_FRAMEBUFFER)
{
  GLuint status = glCheckFramebufferStatus(target);
  switch (status)
  {
  case GL_FRAMEBUFFER_COMPLETE:
    return true;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
    std::cerr << "framebuffer incomplete attachment" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
    std::cerr << "framebuffer missing attachment" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
    std::cerr << "framebuffer incomplete draw buffer" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
    std::cerr << "framebuffer incomplete read buffer" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
    std::cerr << "framebuffer incomplete multisample" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
    std::cerr << "framebuffer incomplete layer targets" << std::endl;
    break;

  case GL_FRAMEBUFFER_UNSUPPORTED:
    std::cerr << "framebuffer unsupported internal format or image" << std::endl;
    break;

  default:
    std::cerr << "other framebuffer error" << std::endl;
    break;
  }

  return false;
}

bool checkGlError()
{
  GLenum error = glGetError();
  if (!error)
  {
    return false;
  }
  else
  {
    switch (error)
    {
    case GL_INVALID_ENUM:
      std::cerr <<
        ": An unacceptable value is specified for an enumerated argument.The offending command is ignored and has no other side effect than to set the error flag.";
      break;
    case GL_INVALID_VALUE:
      std::cerr <<
        ": A numeric argument is out of range.The offending command is ignored and has no other side effect than to set the error flag";
      break;
    case GL_INVALID_OPERATION:
      std::cerr <<
        ": The specified operation is not allowed in the current state.The offending command is ignored and has no other side effect than to set the error flag..";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      std::cerr <<
        ": The framebuffer object is not complete.The offending command is ignored and has no other side effect than to set the error flag.";
      break;
    case GL_OUT_OF_MEMORY:
      std::cerr <<
        ": There is not enough memory left to execute the command.The state of the GL is undefined, except for the state of the error flags, after this error is recorded.";
      break;
    case GL_STACK_UNDERFLOW:
      std::cerr <<
        ": An attempt has been made to perform an operation that would cause an internal stack to underflow.";
      break;
    case GL_STACK_OVERFLOW:
      std::cerr << ": An attempt has been made to perform an operation that would cause an internal stack to overflow.";
      break;
    }
    return true;
  }
}

void glDebugCallbackHandler(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* msg,
                            GLvoid* data)
{
  OutputDebugStringA(msg);
  std::cout << "debug call: " << msg << std::endl;
}

//////////////////////////////////////////////////////////////////////
//
// GLFW provides cross platform window creation
//

#include <GLFW/glfw3.h>

namespace glfw
{
  inline GLFWwindow* createWindow(const uvec2& size, const ivec2& position = ivec2(INT_MIN))
  {
    GLFWwindow* window = glfwCreateWindow(size.x, size.y, "glfw", nullptr, nullptr);
    if (!window)
    {
      FAIL("Unable to create rendering window");
    }
    if ((position.x > INT_MIN) && (position.y > INT_MIN))
    {
      glfwSetWindowPos(window, position.x, position.y);
    }
    return window;
  }
}

// A class to encapsulate using GLFW to handle input and render a scene
class GlfwApp
{
protected:
  uvec2 windowSize;
  ivec2 windowPosition;
  GLFWwindow* window{nullptr};
  unsigned int frame{0};

public:
  GlfwApp()
  {
    // Initialize the GLFW system for creating and positioning windows
    if (!glfwInit())
    {
      FAIL("Failed to initialize GLFW");
    }
    glfwSetErrorCallback(ErrorCallback);
  }

  virtual ~GlfwApp()
  {
    if (nullptr != window)
    {
      glfwDestroyWindow(window);
    }
    glfwTerminate();
  }

  virtual int run()
  {
    preCreate();

    window = createRenderingTarget(windowSize, windowPosition);

    if (!window)
    {
      std::cout << "Unable to create OpenGL window" << std::endl;
      return -1;
    }

    postCreate();

    initGl();

    while (!glfwWindowShouldClose(window))
    {
      ++frame;
      glfwPollEvents();
      update();
      draw();
      finishFrame();
    }

    shutdownGl();

    return 0;
  }

protected:
  virtual GLFWwindow* createRenderingTarget(uvec2& size, ivec2& pos) = 0;

  virtual void draw() = 0;

  void preCreate()
  {
    glfwWindowHint(GLFW_DEPTH_BITS, 16);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
  }

  void postCreate()
  {
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwMakeContextCurrent(window);

    // Initialize the OpenGL bindings
    // For some reason we have to set this experminetal flag to properly
    // init GLEW if we use a core context.
    glewExperimental = GL_TRUE;
    if (0 != glewInit())
    {
      FAIL("Failed to initialize GLEW");
    }
    glGetError();

    if (GLEW_KHR_debug)
    {
      GLint v;
      glGetIntegerv(GL_CONTEXT_FLAGS, &v);
      if (v & GL_CONTEXT_FLAG_DEBUG_BIT)
      {
        //glDebugMessageCallback(glDebugCallbackHandler, this);
      }
    }
  }

  virtual void initGl()
  {
  }

  virtual void shutdownGl()
  {
  }

  virtual void finishFrame()
  {
    glfwSwapBuffers(window);
  }

  virtual void destroyWindow()
  {
    glfwSetKeyCallback(window, nullptr);
    glfwSetMouseButtonCallback(window, nullptr);
    glfwDestroyWindow(window);
  }

  virtual void onKey(int key, int scancode, int action, int mods)
  {
    if (GLFW_PRESS != action)
    {
      return;
    }

    switch (key)
    {
    case GLFW_KEY_ESCAPE:
      glfwSetWindowShouldClose(window, 1);
      return;
    }
  }

  virtual void update()
  {
  }

  virtual void onMouseButton(int button, int action, int mods)
  {
  }

protected:
  virtual void viewport(const ivec2& pos, const uvec2& size)
  {
    glViewport(pos.x, pos.y, size.x, size.y);
  }

private:

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
  {
    GlfwApp* instance = (GlfwApp *)glfwGetWindowUserPointer(window);
    instance->onKey(key, scancode, action, mods);
  }

  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
  {
    GlfwApp* instance = (GlfwApp *)glfwGetWindowUserPointer(window);
    instance->onMouseButton(button, action, mods);
  }

  static void ErrorCallback(int error, const char* description)
  {
    FAIL(description);
  }
};

//////////////////////////////////////////////////////////////////////
//
// The Oculus VR C API provides access to information about the HMD
//

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

namespace ovr
{
  // Convenience method for looping over each eye with a lambda
  template <typename Function>
  inline void for_each_eye(Function function)
  {
    for (ovrEyeType eye = ovrEyeType::ovrEye_Left;
         eye < ovrEyeType::ovrEye_Count;
         eye = static_cast<ovrEyeType>(eye + 1))
    {
      function(eye);
    }
  }

  inline mat4 toGlm(const ovrMatrix4f& om)
  {
    return glm::transpose(glm::make_mat4(&om.M[0][0]));
  }

  inline mat4 toGlm(const ovrFovPort& fovport, float nearPlane = 0.01f, float farPlane = 10000.0f)
  {
    return toGlm(ovrMatrix4f_Projection(fovport, nearPlane, farPlane, true));
  }

  inline vec3 toGlm(const ovrVector3f& ov)
  {
    return glm::make_vec3(&ov.x);
  }

  inline vec2 toGlm(const ovrVector2f& ov)
  {
    return glm::make_vec2(&ov.x);
  }

  inline uvec2 toGlm(const ovrSizei& ov)
  {
    return uvec2(ov.w, ov.h);
  }

  inline quat toGlm(const ovrQuatf& oq)
  {
    return glm::make_quat(&oq.x);
  }

  inline mat4 toGlm(const ovrPosef& op)
  {
    mat4 orientation = glm::mat4_cast(toGlm(op.Orientation));
    mat4 translation = glm::translate(mat4(), ovr::toGlm(op.Position));
    return translation * orientation;
  }

  inline ovrMatrix4f fromGlm(const mat4& m)
  {
    ovrMatrix4f result;
    mat4 transposed(glm::transpose(m));
    memcpy(result.M, &(transposed[0][0]), sizeof(float) * 16);
    return result;
  }

  inline ovrVector3f fromGlm(const vec3& v)
  {
    ovrVector3f result;
    result.x = v.x;
    result.y = v.y;
    result.z = v.z;
    return result;
  }

  inline ovrVector2f fromGlm(const vec2& v)
  {
    ovrVector2f result;
    result.x = v.x;
    result.y = v.y;
    return result;
  }

  inline ovrSizei fromGlm(const uvec2& v)
  {
    ovrSizei result;
    result.w = v.x;
    result.h = v.y;
    return result;
  }

  inline ovrQuatf fromGlm(const quat& q)
  {
    ovrQuatf result;
    result.x = q.x;
    result.y = q.y;
    result.z = q.z;
    result.w = q.w;
    return result;
  }
}

class RiftManagerApp
{
protected:
  ovrSession _session;
  ovrHmdDesc _hmdDesc;
  ovrGraphicsLuid _luid;

public:
  RiftManagerApp()
  {
    if (!OVR_SUCCESS(ovr_Create(&_session, &_luid)))
    {
      FAIL("Unable to create HMD session");
    }

    _hmdDesc = ovr_GetHmdDesc(_session);
  }

  ~RiftManagerApp()
  {
    ovr_Destroy(_session);
    _session = nullptr;
  }
};

//variables
int button_A = 1;
int button_B = 1;
int button_X = 1;
bool isPressed = false;
bool isTouched = false;
int set_Cubesize = 1;
int tracking_lag = 0;
int render_lag = 0;
bool superRotation = false;

class RiftApp : public GlfwApp, public RiftManagerApp
{
public:

private:
  GLuint _fbo{0};
  GLuint _depthBuffer{0};
  ovrTextureSwapChain _eyeTexture;

  GLuint _mirrorFbo{0};
  ovrMirrorTexture _mirrorTexture;

  ovrEyeRenderDesc _eyeRenderDescs[2];

  mat4 _eyeProjections[2];
  mat4 projection_old[2];

  ovrLayerEyeFov _sceneLayer;
  ovrViewScaleDesc _viewScaleDesc;

  uvec2 _renderTargetSize;
  uvec2 _mirrorSize;

  mat4 left_pos_old;
  mat4 left_pos_new;
  mat4 right_pos_old;
  mat4 right_pos_new;

  double iod, iod_origin;
  int set_iod = 1;
  int count = 0;

public:

  RiftApp()
  {
    using namespace ovr;
    _viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;

    memset(&_sceneLayer, 0, sizeof(ovrLayerEyeFov));
    _sceneLayer.Header.Type = ovrLayerType_EyeFov;
    _sceneLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

    ovr::for_each_eye([&](ovrEyeType eye)
    {
      ovrEyeRenderDesc& erd = _eyeRenderDescs[eye] = ovr_GetRenderDesc(_session, eye, _hmdDesc.DefaultEyeFov[eye]);
      ovrMatrix4f ovrPerspectiveProjection =
        ovrMatrix4f_Projection(erd.Fov, 0.01f, 1000.0f, ovrProjection_ClipRangeOpenGL);
      _eyeProjections[eye] = ovr::toGlm(ovrPerspectiveProjection);
      _viewScaleDesc.HmdToEyePose[eye] = erd.HmdToEyePose;

	  //set iod
	  iod = abs(_viewScaleDesc.HmdToEyePose[0].Position.x - _viewScaleDesc.HmdToEyePose[1].Position.x);
	  iod_origin = abs(_viewScaleDesc.HmdToEyePose[0].Position.x - _viewScaleDesc.HmdToEyePose[1].Position.x);

      ovrFovPort& fov = _sceneLayer.Fov[eye] = _eyeRenderDescs[eye].Fov;
      auto eyeSize = ovr_GetFovTextureSize(_session, eye, fov, 1.0f);
      _sceneLayer.Viewport[eye].Size = eyeSize;
      _sceneLayer.Viewport[eye].Pos = {(int)_renderTargetSize.x, 0};

      _renderTargetSize.y = std::max(_renderTargetSize.y, (uint32_t)eyeSize.h);
      _renderTargetSize.x += eyeSize.w;
    });
    // Make the on screen window 1/4 the resolution of the render target
    _mirrorSize = _renderTargetSize;
    _mirrorSize /= 4;
  }

protected:
  GLFWwindow* createRenderingTarget(uvec2& outSize, ivec2& outPosition) override
  {
    return glfw::createWindow(_mirrorSize);
  }

  void initGl() override
  {
    GlfwApp::initGl();

    // Disable the v-sync for buffer swap
    glfwSwapInterval(0);

    ovrTextureSwapChainDesc desc = {};
    desc.Type = ovrTexture_2D;
    desc.ArraySize = 1;
    desc.Width = _renderTargetSize.x;
    desc.Height = _renderTargetSize.y;
    desc.MipLevels = 1;
    desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleCount = 1;
    desc.StaticImage = ovrFalse;
    ovrResult result = ovr_CreateTextureSwapChainGL(_session, &desc, &_eyeTexture);
    _sceneLayer.ColorTexture[0] = _eyeTexture;
    if (!OVR_SUCCESS(result))
    {
      FAIL("Failed to create swap textures");
    }

    int length = 0;
    result = ovr_GetTextureSwapChainLength(_session, _eyeTexture, &length);
    if (!OVR_SUCCESS(result) || !length)
    {
      FAIL("Unable to count swap chain textures");
    }
    for (int i = 0; i < length; ++i)
    {
      GLuint chainTexId;
      ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, i, &chainTexId);
      glBindTexture(GL_TEXTURE_2D, chainTexId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // Set up the framebuffer object
    glGenFramebuffers(1, &_fbo);
    glGenRenderbuffers(1, &_depthBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _renderTargetSize.x, _renderTargetSize.y);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    ovrMirrorTextureDesc mirrorDesc;
    memset(&mirrorDesc, 0, sizeof(mirrorDesc));
    mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    mirrorDesc.Width = _mirrorSize.x;
    mirrorDesc.Height = _mirrorSize.y;
    if (!OVR_SUCCESS(ovr_CreateMirrorTextureGL(_session, &mirrorDesc, &_mirrorTexture)))
    {
      FAIL("Could not create mirror texture");
    }
    glGenFramebuffers(1, &_mirrorFbo);
  }

  void onKey(int key, int scancode, int action, int mods) override
  {
    if (GLFW_PRESS == action)
      switch (key)
      {
      case GLFW_KEY_R:
        ovr_RecenterTrackingOrigin(_session);
        return;
      }

    GlfwApp::onKey(key, scancode, action, mods);
  }

  void update() final override {
	  ovrInputState inputState;
	  if (OVR_SUCCESS(ovr_GetInputState(_session, ovrControllerType_Touch, &inputState))) {

		  //Triggers
		  if (inputState.IndexTrigger[ovrHand_Left] < 0.01f && inputState.IndexTrigger[ovrHand_Right] < 0.01f
				&& inputState.HandTrigger[ovrHand_Left] < 0.01f && inputState.HandTrigger[ovrHand_Right] < 0.01f) {
			  isTouched = false;
		  }

		  if (inputState.IndexTrigger[ovrHand_Left] > 0.1f && tracking_lag > 0 && !isTouched) {
			  isTouched = true;
			  tracking_lag--;
			  printf("Tracking lag: %d frames\n", tracking_lag);
		  }

		  if (inputState.IndexTrigger[ovrHand_Right] > 0.1f && tracking_lag < 29 && !isTouched) {
			  isTouched = true;
			  tracking_lag++;
			  printf("Tracking lag: %d frames\n", tracking_lag);
		  }

		  if (inputState.HandTrigger[ovrHand_Left] > 0.1f && render_lag > 0 && !isTouched) {
			  isTouched = true;
			  render_lag--;
			  printf("Rendering delay: %d frames\n", render_lag);
		  }

		  if (inputState.HandTrigger[ovrHand_Right] > 0.1f && render_lag < 10 && !isTouched) {
			  isTouched = true;
			  render_lag++;
			  printf("Rendering delay: %d frames\n", render_lag);
		  }

		  //update iod
		  if (inputState.Thumbstick[ovrHand_Right].x < 0) {
			  set_iod = 2;
		  }

		  else if (inputState.Thumbstick[ovrHand_Right].x > 0) {
			  set_iod = 3;
		  }

		  else if (inputState.Buttons && ovrButton_RThumb) {
			  set_iod = 4;
		  }

		  //update cube size
		  set_Cubesize = 1;
		  if (inputState.Thumbstick[ovrHand_Left].x < 0) {
			  set_Cubesize = 2;
		  }

		  else if (inputState.Thumbstick[ovrHand_Left].x > 0) {
			  set_Cubesize = 3;
		  }

		  else if (inputState.Buttons && ovrButton_LThumb) {
			  set_Cubesize = 4;
		  }

		  //Buttons
		  if (!inputState.Buttons) {
			  isPressed = false;
		  }

		  if ((inputState.Buttons & ovrButton_A) && !isPressed) {
			  isPressed = true;

			  if (button_A == 5) {
				  button_A = 1;
			  }
			  else {
				  button_A++;
			  }
		  }

		  else if ((inputState.Buttons & ovrButton_B) && !isPressed) {
			  isPressed = true;

			  if (button_B == 4) {
				  button_B = 1;
			  }
			  else {
				  button_B++;
			  }
		  }

		  else if ((inputState.Buttons & ovrButton_X) && !isPressed) {
			  isPressed = true;

			  if (button_X == 4) {
				  button_X = 1;
			  }
			  else {
				  button_X++;
			  }
		  }

		  else if ((inputState.Buttons & ovrButton_Y) && !isPressed) {
			  isPressed = true;
			  superRotation = !superRotation;
		  }
	  }

	  //change iod
	  if (set_iod == 3 && iod < 0.3) {
		  iod += 0.01;
	  }
	  else if (set_iod == 2 && iod > -0.3) {
		  iod -= 0.01;
	  }
	  else if (set_iod == 4) {
		  iod = iod_origin;
	  }
	  _viewScaleDesc.HmdToEyePose[0].Position.x = (float)(-iod / 2);
	  _viewScaleDesc.HmdToEyePose[1].Position.x = (float)(iod / 2);
	  set_iod = 1;

	  if (count == 0) {
		  count = render_lag;
	  }
	  else {
		  count--;
	  }
  }

  void draw() final override
  {
    ovrPosef eyePoses[2];
    ovr_GetEyePoses(_session, frame, true, _viewScaleDesc.HmdToEyePose, eyePoses, &_sceneLayer.SensorSampleTime);

	if (count == 0) {
		left_pos_new = ovr::toGlm(eyePoses[ovrEye_Left]);
		right_pos_new = ovr::toGlm(eyePoses[ovrEye_Right]);
		projection_old[0] = _eyeProjections[0];
		projection_old[1] = _eyeProjections[1];
	}
	else {
		left_pos_new = left_pos_old;
		right_pos_new = right_pos_old;
		_eyeProjections[0] = projection_old[0];
		_eyeProjections[1] = projection_old[1];
	}

	if (button_B == 2) {
		left_pos_new[3] = left_pos_old[3];
		right_pos_new[3] = right_pos_old[3];
	}

	else if (button_B == 3) {
		left_pos_new[0] = left_pos_old[0];
		left_pos_new[1] = left_pos_old[1];
		left_pos_new[2] = left_pos_old[2];

		right_pos_new[0] = right_pos_old[0];
		right_pos_new[1] = right_pos_old[1];
		right_pos_new[2] = right_pos_old[2];
	}

	else if (button_B == 4) {
		left_pos_new = left_pos_old;
		right_pos_new = right_pos_old;
	}

    int curIndex;
    ovr_GetTextureSwapChainCurrentIndex(_session, _eyeTexture, &curIndex);
    GLuint curTexId;
    ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, curIndex, &curTexId);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ovr::for_each_eye([&](ovrEyeType eye)
    {
      const auto& vp = _sceneLayer.Viewport[eye];
      glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
      _sceneLayer.RenderPose[eye] = eyePoses[eye];

	  if (button_A == 1) {
		if (eye == ovrEye_Left) {
		  renderScene(_eyeProjections[ovrEye_Left], left_pos_new, true);
		}

		else if (eye == ovrEye_Right) {
		  renderScene(_eyeProjections[ovrEye_Right], right_pos_new, false);
		}
	  }

	  else if (button_A == 2) {
		  renderScene(_eyeProjections[eye], left_pos_new, true);
	  }

	  else if (button_A == 3) {
		  if (eye == ovrEye_Left) {
			  renderScene(_eyeProjections[ovrEye_Left], left_pos_new, true);
		  }
	  }
      
	  else if (button_A == 4) {
		  if (eye == ovrEye_Right) {
			  renderScene(_eyeProjections[ovrEye_Right], right_pos_new, false);
		  }
	  }

	  else if (button_A == 5) {
		  if (eye == ovrEye_Left) {
			  renderScene(_eyeProjections[ovrEye_Right], right_pos_new, false);
		  }

		  else if (eye == ovrEye_Right) {
			  renderScene(_eyeProjections[ovrEye_Left], left_pos_new, true);
		  }
	  }

    });
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    ovr_CommitTextureSwapChain(_session, _eyeTexture);
    ovrLayerHeader* headerList = &_sceneLayer.Header;
    ovr_SubmitFrame(_session, frame, &_viewScaleDesc, &headerList, 1);

    GLuint mirrorTextureId;
    ovr_GetMirrorTextureBufferGL(_session, _mirrorTexture, &mirrorTextureId);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _mirrorFbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureId, 0);
    glBlitFramebuffer(0, 0, _mirrorSize.x, _mirrorSize.y, 0, _mirrorSize.y, _mirrorSize.x, 0, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	//update position
	left_pos_old = left_pos_new;
	right_pos_old = right_pos_new;
  }

  virtual void renderScene(const glm::mat4& projection, const glm::mat4& headPose, bool isLeft) = 0;
};

class Cursor {

	// Shader ID
	GLuint shaderID;

	// Cursor
	std::unique_ptr<Model> cursor;

	// User's Dominant Hand's Controller Position 
	vec3 position;


public:
	Cursor() {
		shaderID = LoadShaders("shader_cursor.vert", "shader_cursor.frag");
		cursor = std::make_unique<Model>("webtrcc.obj");
	}

	/* Render sphere at User's Dominant Hand's Controller Position */
	void render(const glm::mat4& projection, const glm::mat4& view, vec3 pos) {
		position = pos;
		glm::mat4 toWorld = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(0.02f));
		cursor->Draw(shaderID, projection, view, toWorld);
	}

};

//////////////////////////////////////////////////////////////////////
//
// The remainder of this code is specific to the scene we want to 
// render.  I use glfw to render an array of cubes, but your 
// application would perform whatever rendering you want
//

#include <vector>
#include "shader.h"
#include "Cube.h"

// a class for building and rendering cubes
class Scene
{
  // Program
  std::vector<glm::mat4> instance_positions;
  GLuint instanceCount;
  GLuint shaderID;

  std::unique_ptr<TexturedCube> cube;
  std::unique_ptr<Skybox> skybox_left;
  std::unique_ptr<Skybox> skybox_right;
  std::unique_ptr<Skybox> skybox_custom;

  const unsigned int GRID_SIZE{5};

  glm::mat4 cubeSize;

public:
  Scene()
  {
    // Create two cube
    instance_positions.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -0.3)));
    instance_positions.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -0.9)));

    instanceCount = instance_positions.size();

    // Shader Program 
    shaderID = LoadShaders("skybox.vert", "skybox.frag");

    cube = std::make_unique<TexturedCube>("cube"); 

	  // 10m wide sky box: size doesn't matter though
    skybox_left = std::make_unique<Skybox>("skybox_left");
	skybox_right = std::make_unique<Skybox>("skybox_right");
	skybox_left ->toWorld = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));
	skybox_right->toWorld = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));

	cubeSize = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

	skybox_custom = std::make_unique<Skybox>("skybox_custom");
	skybox_custom->toWorld = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));
  }

  void render(const glm::mat4& projection, const glm::mat4& view, bool isLeft)
  {
	  //Change the size of cubes
	  if (set_Cubesize == 2 && cubeSize[0][0] > 0.01f) {
		  cubeSize = cubeSize * glm::scale(glm::mat4(1.0f), glm::vec3(0.99f));
	  }

	  if (set_Cubesize == 3 && cubeSize[0][0] < 0.5f) {
		  cubeSize = cubeSize * glm::scale(glm::mat4(1.0f), glm::vec3(1.01f));
	  }

	  if (set_Cubesize == 4) {
		  cubeSize = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
	  }

    // Render two cubes
	if (button_X == 1) {
		for (int i = 0; i < instanceCount; i++)
			{
			  // Scale to 20cm: 200cm * 0.1
			  cube->toWorld = instance_positions[i] * cubeSize;
			  cube->draw(shaderID, projection, view);
			}
	}
    
	if (button_X == 1 || button_X == 2) {
		// Render Skybox : remove view translation
			if (isLeft) {
				skybox_left->draw(shaderID, projection, view);
			}
			else {
				skybox_right->draw(shaderID, projection, view);
			}
	}
    
	else if (button_X == 3) {
		skybox_left->draw(shaderID, projection, view);
	}

	else if (button_X == 4) {
		skybox_custom->draw(shaderID, projection, view);
	}
  }
};

//Ring buffer
class Buffer {
	vec3 positions[30];
	int read, write, num;

public:
	Buffer() {
		read = 0;
		write = 0;
		num = 0;
	}

	void push(vec3 position) {
		positions[write] = position;
		write = (write + 1) % 30;
		if (num < 30) {
			num++;
		}
	}

	vec3 pop(int lag) {
		if (num == 30) {
			int i = (read - lag + 30) % 30;
			read = (read + 1) % 30;
			return positions[i];
		}
		
		else {
			return positions[0];
		}
	}

};

mat3 computeRotation(float theta_x, float theta_y, float theta_z) {
	mat3 X(1.0f, 0.0f, 0.0f, 0.0f, cosf(theta_x), -sinf(theta_x), 0.0f, sinf(theta_x), cosf(theta_x));
	mat3 Y(cosf(theta_y), 0.0f, sinf(theta_y), 0.0f, 1.0f, 0.0f, -sinf(theta_y), 0.0f, cosf(theta_y));
	mat3 Z(cosf(theta_z), -sinf(theta_z), 0.0f, sinf(theta_z), cosf(theta_z), 0.0f, 0.0f, 0.0f, 1.0f);

	mat3 R = Z * Y * X;
	return R;
}

// An example application that renders a simple cube
class ExampleApp : public RiftApp
{
  std::shared_ptr<Scene> scene;
  std::shared_ptr<Cursor> cursor;

  double displayMidpointSeconds;
  ovrTrackingState trackState;
  ovrInputState  inputState;
  unsigned int handStatus[2];
  ovrPosef handPoses[2];
  ovrVector3f handPosition[2];
  
  vector<vec3> positions;
  std::shared_ptr<Buffer> buffer;

public:
  ExampleApp()
  {
  }

protected:
  void initGl() override
  {
    RiftApp::initGl();
    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    ovr_RecenterTrackingOrigin(_session);
    scene = std::shared_ptr<Scene>(new Scene());
	cursor = std::shared_ptr<Cursor>(new Cursor());
	buffer = std::shared_ptr<Buffer>(new Buffer());
  }

  void shutdownGl() override
  {
    scene.reset();
  }

  void renderScene(const glm::mat4& projection, const glm::mat4& headPose, bool isLeft) override
  {
	displayMidpointSeconds = ovr_GetPredictedDisplayTime(_session, 0);
	trackState = ovr_GetTrackingState(_session, displayMidpointSeconds, ovrTrue);
	handStatus[0] = trackState.HandStatusFlags[0];
	handStatus[1] = trackState.HandStatusFlags[1];
	handPoses[0] = trackState.HandPoses[0].ThePose;
	handPoses[1] = trackState.HandPoses[1].ThePose;
	handPosition[0] = handPoses[0].Position;
	handPosition[1] = handPoses[1].Position;

	buffer->push(vec3(handPosition[ovrHand_Right].x, handPosition[ovrHand_Right].y, handPosition[ovrHand_Right].z));

	if (!superRotation) {
		scene->render(projection, glm::inverse(headPose), isLeft);
		cursor->render(projection, glm::inverse(headPose), buffer->pop(tracking_lag));
	}
    
	else {
		mat3 R(headPose[0], headPose[1], headPose[2]);
		float theta_1 = atan2f(R[1][2], R[2][2]);
		float c2 = sqrt(pow(R[0][0], 2) + pow(R[0][1], 2));
		float theta_2 = atan2f(-R[0][2], c2);
		float theta_3 = atan2f(sinf(theta_1)*R[2][0] - cosf(theta_1)*R[1][0], cosf(theta_1)*R[1][1] - sinf(theta_1)*R[2][1]);

		mat3 new_R = computeRotation(theta_1, -2.0f * theta_2, theta_3);
		mat4 new_headPose = mat4(new_R);
		new_headPose[3] = headPose[3];

		scene->render(projection, glm::inverse(new_headPose), isLeft);
		cursor->render(projection, glm::inverse(new_headPose), buffer->pop(tracking_lag));
	}
  }
};

// Execute our example class
int main(int argc, char** argv)
{
  int result = -1;

  if (!OVR_SUCCESS(ovr_Initialize(nullptr)))
  {
    FAIL("Failed to initialize the Oculus SDK");
  }
  result = ExampleApp().run();

  ovr_Shutdown();
  return result;
}
