#version 430
#define PI 3.1415

layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D proceduralTexture;
layout(binding = 1) uniform sampler2D checkerTexture;
layout (binding = 2) uniform samplerCube cubeMapTexture;
layout(push_constant) uniform PushConstants {
    vec2 resolution;
    vec2 mouse;
    float time;
} pc;

mat3 rotateX(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(1, 0, 0),
        vec3(0, c, -s),
        vec3(0, s, c)
    );
}

mat3 rotateY(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, 0, s),
        vec3(0, 1, 0),
        vec3(-s, 0, c)
    );
}

mat3 rotateZ(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, -s, 0),
        vec3(s, c, 0),
        vec3(0, 0, 1)
    );
}

float sphere(vec3 point, float radius) {
    return length(point) - radius;
}

float box(vec3 point, vec3 dimensions) {
  vec3 q = abs(point) - dimensions;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float plane(vec3 point, vec3 normal) {
    return abs(dot(point, normal) + 0.1);
}

float box4d(vec4 point, vec4 dimensions) {
    vec4 q = abs(point) - dimensions;

    return length(max(q, 0.0)) + min(max(q.x, max(q.y, max(q.z, q.w))), 0.0);
}

float smoothIntersection(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5*(d2-d1)/k, 0.0, 1.0 );

    return mix(d2, d1, h) + k*h*(1.0-h); 
}
float smoothSubtraction(float d1, float d2, float k) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );

    return mix( d2, -d1, h ) + k*h*(1.0-h); 
}
float smoothUnion ( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );

    return mix( d2, d1, h ) - k*h*(1.0-h); 
}

int minIndex(float x, float y, float z) {
    return int((y<z)&&(y<x)) + (int((z<y)&&(z<x))*2);
}
int minIndex(float x, float y) {
    return int(y>x);
}

float sdf(vec3 point, out int object) {
    //return plane(point + vec3(0., -2.1, 0.), vec3(0., -1., 0.));
    float angleZW = pc.time * 0.6;
    mat4x4 rotationZW = mat4x4(
        cos(angleZW), -sin(angleZW), 0., 0.,
        sin(angleZW), cos(angleZW), 0., 0.,
        0., 0., 1., 0.,
        0., 0., 0., 1.
    );
    float angleXY = pc.time * 0.5;
    mat4x4 rotationXY = mat4x4(
        1., 0., 0., 0.,
        0., 1., 0., 0.,
        0., 0., cos(angleXY), -sin(angleXY),
        0., 0., sin(angleXY), cos(angleXY)
    );
    float angleXW = pc.time * 0.3;
    mat4x4 rotationXW = mat4x4(
        1., 0., 0., 0.,
        0., cos(angleXW), -sin(angleXW), 0.,
        0., sin(angleXW), cos(angleXW), 0.,
        0., 0., 0., 1.
    );
    float angleYZ = pc.time * 0.4;
    mat4x4 rotationYZ = mat4x4(
        cos(angleYZ), 0., 0., -sin(angleYZ),
        0., 1., 0., 0.,
        0., 0., 1., 0.,
        sin(angleYZ), 0., 0., cos(angleYZ)
    );
    float angleYW = pc.time * 0.2;
    mat4x4 rotationYW = mat4x4(
        cos(angleYW), 0., -sin(angleYW), 0.,
        0., 1., 0., 0.,
        sin(angleYW), 0., cos(angleYW), 0.,
        0., 0., 0., 1.
    );
    float angleXZ = pc.time * 0.5;
    mat4x4 rotationXZ = mat4x4(
        1., 0., 0., 0.,
        0., cos(angleXZ), 0., -sin(angleXZ),
        0., 0., 1., 0.,
        0., sin(angleXZ), 0., cos(angleXZ)
    );

    float sphereValue = sphere(point + vec3(1. * sin(pc.time), 0.5 * sin(pc.time * 5.), 1. * cos(pc.time)), 0.3);
    float box4dValue = box4d(vec4(point, 0.4 * sin(pc.time * 0.3)) * rotationXY * rotationZW * rotationXW * rotationYZ * rotationYW * rotationXZ + vec4(0., 0., 0., 0.), vec4(0.1, 0.1, 0.1, 0.1) * (4. + 1. * sin(pc.time / 4.)));
    float planeValue = plane(point + vec3(0., -2.1, 0.), vec3(0., -1., 0.));
    object = minIndex(box4dValue, planeValue, sphereValue);
    return min(min(box4dValue - 0.03, planeValue), sphereValue);
}

float sdf(vec3 point) {
    int object;

    return sdf(point, object);
}

vec3 raymarch(vec3 from, vec3 direction, out bool hit, out int object) {
    const int maxSteps = 100;
    const float maxPath = 100.0;
    const float epsilon = 0.001;

    float pathChange = 0.;
    float pathTotal = 0.;
    vec3 point = from;
    hit = false;

    for (int steps = 0; pathTotal < maxPath; ++steps) {
        pathChange = sdf(point, object);
        pathTotal += pathChange;
        point = point + direction * pathChange;

        if (pathChange < epsilon) {
            hit = true;
            break;
        }
    }

    return point;
}

vec3 raymarch(vec3 from, vec3 direction, out bool hit) {
    int object;

    return raymarch(from, direction, hit, object);
}

vec3 generateNormal(vec3 point) {
    const float epsilon = 0.001;

    float dx1 = sdf(point + vec3(epsilon, 0, 0));
    float dx2 = sdf(point - vec3(epsilon, 0, 0));
    float dy1 = sdf(point + vec3(0, epsilon, 0));
    float dy2 = sdf(point - vec3(0, epsilon, 0));
    float dz1 = sdf(point + vec3(0, 0, epsilon));
    float dz2 = sdf(point - vec3(0, 0, epsilon));

    return normalize(vec3(dx1 - dx2, dy1 - dy2, dz1 - dz2));
}

vec4 planeColor(vec3 normal, vec3 point) {
    float multiplier = 0.1;
    return vec4(vec3(-normal.y * texture(checkerTexture, (point.xz * multiplier)).r * 0.7 + 0.3), 1.0);
}

vec4 boxTexture(vec3 normal, vec3 point) {
    float multiplier = 2.;
    normal = abs(normal);

    return vec4(vec3(
        normal.x * texture(proceduralTexture, (point.yz * multiplier))
        + normal.y * texture(proceduralTexture, (point.xz * multiplier))
        + normal.z * texture(proceduralTexture, (point.xy * multiplier))
    ), 1.0);
    return vec4(0.5, 0.5, 0.5, 1.);
}

void main()
{
    vec2 mouse = pc.mouse.xy / pc.resolution.xy * 0.6 + 0.4;
    vec2 scale = pc.resolution.xy / max(pc.resolution.x, pc.resolution.y);
    // vec2 uv = (gl_FragCoord.xy / pc.resolution - vec2(0.5)) * scale;

    vec2 uv = vec2(gl_FragCoord.x, pc.resolution.y - gl_FragCoord.y) / pc.resolution - vec2(0.5);
    uv *= scale;

    bool hit = false;
    int object;

    vec3 cameraPosition = vec3(0., 0., 6.) * rotateX(-mouse.y*3. - PI/2.) * rotateY(-mouse.x*5.);
    vec3 pointOfInterest = vec3(0., 0., 0.);
    vec3 cameraDirection = pointOfInterest - cameraPosition;

    vec3 forward = normalize(cameraDirection);
    vec3 right = normalize(cross(forward, vec3(0., 1., 0.)));
    vec3 up = cross(forward, right);
    float focalLength = 1.;

    vec3 rayDirection = normalize(uv.x * right + uv.y * up + focalLength * forward);
    vec3 intersectionPoint = raymarch(cameraPosition, rayDirection, hit, object);
    vec3 lightSource = vec3(5., -5., -1.) * 5.;

    if (hit) {
        float ambientLight = 0.1;

        vec3 lightVector = normalize(lightSource - intersectionPoint);
        vec3 surfaceNormal = generateNormal(intersectionPoint);
        float diffuseLight = max(0.0, dot(surfaceNormal, lightVector)); 

        vec4[3] objectColors = (vec4[](
            boxTexture(surfaceNormal, intersectionPoint),
            planeColor(surfaceNormal, intersectionPoint),
            vec4(1.0, 1.0, 0.0, 1.0)
        ));
        vec4 objectColor = objectColors[object];


        // blinn and phong
        vec3 viewVector = normalize(cameraPosition - intersectionPoint);
        vec3 halfwayVector = normalize(lightVector + viewVector);
        float specularLight = pow(max(dot(surfaceNormal, halfwayVector), 0.0), 50.);

        // shadow realization with raymarch
        bool shadow = false;
        vec3 shadowRayDirection = normalize(lightSource - intersectionPoint);
        vec3 shadowPoint = raymarch(intersectionPoint + shadowRayDirection * 0.1, shadowRayDirection, shadow);

        if (object == 2) {
            bool reflection = false;
            float reflectedAmbientLight = 0.2;
            int anotherObject;
            diffuseLight = diffuseLight + 0.2;

            vec3 reflectionRayDirection = rayDirection - 2.0 * dot(surfaceNormal, rayDirection) * surfaceNormal;
            vec3 reflectedPoint = raymarch(intersectionPoint + reflectionRayDirection * 0.1, reflectionRayDirection, reflection, anotherObject);
            vec3 reflectedLightVector = normalize(lightSource - reflectedPoint);
            vec3 reflectedSurfaceNormal = generateNormal(reflectedPoint);
            float reflectedDiffuseLight = max(0.0, dot(reflectedSurfaceNormal, reflectedLightVector)); 
            vec3 reflectedViewVector = normalize(cameraPosition - reflectedPoint);
            vec3 reflectedHalfwayVector = normalize(reflectedLightVector + reflectedViewVector);
            float reflectedSpecularLight = pow(max(dot(reflectedSurfaceNormal, reflectedHalfwayVector), 0.0), 50.);

            vec4[2] objectColors = (vec4[](
                boxTexture(reflectedSurfaceNormal, reflectedPoint),
                planeColor(reflectedSurfaceNormal, reflectedPoint)
            ));
            vec4 objectColor = objectColors[anotherObject];

            if (reflection) {
                bool shadow = false;
                vec3 shadowRayDirection = normalize(lightSource - reflectedPoint);
                vec3 shadowPoint = raymarch(reflectedPoint + shadowRayDirection * 1., shadowRayDirection, shadow);

                objectColor = objectColor * (reflectedAmbientLight + reflectedDiffuseLight + reflectedSpecularLight);
                color = objectColor * (ambientLight + diffuseLight);

                if (shadow && dot(reflectedSurfaceNormal, lightVector) > 0.2) {
                    color *= 0.2;
                }
            }
            else {
                color = texture(cubeMapTexture, -normalize(reflectionRayDirection)) * (ambientLight + diffuseLight);
            }
            if (shadow && dot(surfaceNormal, lightVector) > 0.2) {
                color *= 0.2;
            }
            else {
                color = color + specularLight * vec4(1., 1., 1., 1.);
            }
        }
        else {
            if (shadow && dot(surfaceNormal, lightVector) > 0.2) {
                color = objectColor * (ambientLight + diffuseLight) * 0.2;
            }
            else {
                color = objectColor * (ambientLight + diffuseLight + specularLight);
            }
        }

    }
    else {
        vec3 col = 0.5 + 0.5*cos(pc.time+uv.xyx+vec3(0,2,4));
        color = texture(cubeMapTexture, -normalize(uv.x * right + uv.y * up + 1. * forward));
    }
}
