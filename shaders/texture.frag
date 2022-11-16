#version 450

layout (binding = 1) uniform sampler2D samplerColor;

layout (binding = 2) uniform UBO
{
  vec3 vanishingPoint;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{

  if(gl_FragCoord.x > ubo.vanishingPoint.x - ubo.vanishingPoint.z / 2  && gl_FragCoord.x < ubo.vanishingPoint.x + ubo.vanishingPoint.z / 2) {
    if (gl_FragCoord.y > ubo.vanishingPoint.y - ubo.vanishingPoint.z / 2 && gl_FragCoord.y < ubo.vanishingPoint.y + ubo.vanishingPoint.z / 2) {
      outFragColor = texture(samplerColor, inUV) + vec4(1.0, 0.0, 0.0, 1.0);
      return;
    }
  }

  outFragColor = texture(samplerColor, inUV);
}