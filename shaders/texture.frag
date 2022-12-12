#version 450

#define MAX_KEYPOINTS 200
#define KEYPOINT_SIZE 3

layout (binding = 1) uniform sampler2D samplerColor;

layout (binding = 2) uniform UBO
{
    int showVanishingPoint;
    int showKeyPoints;
    vec3 vanishingPoint;
    vec2 keyPoints[MAX_KEYPOINTS];
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main()
{
    if (bool(ubo.showKeyPoints)) {
        // Visualise keypoints
        for (int i = 0; i < MAX_KEYPOINTS; i += 2) {
            if (gl_FragCoord.x > ubo.keyPoints[i].x - KEYPOINT_SIZE && gl_FragCoord.x < ubo.keyPoints[i].x + KEYPOINT_SIZE) {
                if (gl_FragCoord.y > ubo.keyPoints[i].y - KEYPOINT_SIZE && gl_FragCoord.y < ubo.keyPoints[i].y + KEYPOINT_SIZE) {
                    outFragColor = texture(samplerColor, inUV) + vec4(1.0, 1.0, 0.0, 1.0);
                    return;
                }
            }
        }
    }

    if (bool(ubo.showVanishingPoint)) {
        // Visualise Vanishing point
        if (gl_FragCoord.x > ubo.vanishingPoint.x - ubo.vanishingPoint.z / 2 && gl_FragCoord.x < ubo.vanishingPoint.x + ubo.vanishingPoint.z / 2) {
            if (gl_FragCoord.y > ubo.vanishingPoint.y - ubo.vanishingPoint.z / 2 && gl_FragCoord.y < ubo.vanishingPoint.y + ubo.vanishingPoint.z / 2) {
                outFragColor = texture(samplerColor, inUV) + vec4(0.2, 0.0, 0.0, 1.0);
                return;
            }
        }
    }

    outFragColor = texture(samplerColor, inUV);
}