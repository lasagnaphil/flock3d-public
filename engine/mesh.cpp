#include "mesh.h"

/*
void TexturedMeshCpuData::create_sphere(glm::vec3 pos, float radius) {
    // From: http://www.songho.ca/opengl/gl_sphere.html

    const int stackCount = 12;
    const int sectorCount = 12;
    const float PI = glm::pi<float>();

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * PI / sectorCount;
    float stackStep = PI / stackCount;
    float sectorAngle, stackAngle;

    for(int i = 0; i <= stackCount; ++i)
    {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for(int j = 0; j <= sectorCount; ++j)
        {
            TexturedVertex vertex;
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            vertex.pos.x = pos.x + xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            vertex.pos.y = pos.y + xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            vertex.pos.z = pos.z + z;

            // normalized vertex normal (nx, ny, nz)
            vertex.normal.x = x * lengthInv;
            vertex.normal.y = y * lengthInv;
            vertex.normal.z = z * lengthInv;

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / sectorCount;
            t = (float)i / stackCount;
            vertex.texcoord.x = s;
            vertex.texcoord.y = t;

            vertices.push_back(vertex);
        }
    }

    // generate CCW index list of sphere triangles
    // k1--k1+1
    // |  / |
    // | /  |
    // k2--k2+1
    int k1, k2;
    for(int i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for(int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if(i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            // k1+1 => k2 => k2+1
            if(i != (stackCount-1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}
*/
