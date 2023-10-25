#pragma once

#include <vector>
#include <chrono>
#include <cstdint>
#include <string>
#include <filesystem>
#include <iosfwd>
#include <format>
#include <unordered_map>
#include <array>
#include <utility>
#include <iosfwd>
#include <assert.h>
#include "PrimitiveTypes.h"
#include <cmath>

#define IMGUI_DEFINE_MATH_OPERATORS // For ImGuizmo
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <ImGuizmo.h>

#include <ImGuiNotify.hpp>
#include <IconsFontAwesome6.h>

// Include all GLM core / GLSL features
#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/opencl.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp> // vec2, vec3, mat4, radians

// Include all GLM extensions
#include <glm/ext.hpp> // perspective, translate, rotate
#include <glm/gtc/matrix_transform.hpp>

#include "strippedwindows.h"

#include "Timer.h"
#include "LogUtility.h"
#include "IOUtility.h"
#include "Utilities.h"

#include "Math.h"

#define TINYGLTF_NOEXCEPTION
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_STB_IMAGE_WRITE
//#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_USE_CPP14
#include "json.hpp"
#include <stb_image.h>
#include <stb_image_write.h>

#include "tiny_gltf.h"
